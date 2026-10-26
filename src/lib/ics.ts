// Reading an iCalendar (.ics) feed, such as Google Calendar's "secret address in iCal format", and working out
// which events fall in a stretch of time (repeating events are expanded). Pure functions, no network.

import {tr} from './i18n';

export interface Occurrence {
	/** ms since 1970 */
	start: number;
	end: number;
	allDay: boolean;
	title: string;
	place: string;
	/** which calendar it came from (an index the caller assigns; 0 when there is only one) */
	src?: number;
}

type Freq = 'DAILY' | 'WEEKLY' | 'MONTHLY' | 'YEARLY';

interface Rule {
	freq: Freq;
	interval: number;
	count: number | null;
	until: number | null;
	/** weekday (0 = Sunday) with an optional position in the month (2 = second, -1 = last, 0 = every) */
	byDay: {n: number; wd: number}[];
	byMonthDay: number[];
	byMonth: number[];
}

interface Wall {
	y: number; mo: number; d: number; h: number; mi: number; s: number;
}

interface RawEvent {
	uid: string;
	title: string;
	place: string;
	wall: Wall;
	/** the time zone the times are in: null = the TV's own, 'UTC', or an IANA name */
	tz: string | null;
	start: number;
	end: number;
	allDay: boolean;
	rule: Rule | null;
	exdates: number[];
	recurrenceId: number | null;
	cancelled: boolean;
}

interface Prop {
	name: string;
	params: Record<string, string>;
	value: string;
}

const DAY = 86400000;
const WEEKDAYS = ['SU', 'MO', 'TU', 'WE', 'TH', 'FR', 'SA'];
/** Safety limit on how many days one repeating event is walked through. */
const MAX_WALK_DAYS = 20000;

// ---------- reading the text ----------

function unfold (text: string): string[] {
	return text.replace(/\r\n/g, '\n').replace(/\r/g, '\n').replace(/\n[ \t]/g, '').split('\n');
}

function unescapeText (value: string): string {
	return value.replace(/\\([nN,;\\])/g, (_m, c: string) => (c === 'n' || c === 'N' ? '\n' : c));
}

function parseProp (line: string): Prop | null {
	const colon = line.indexOf(':');
	if (colon < 1) return null;
	const head = line.slice(0, colon).split(';');
	const params: Record<string, string> = {};
	for (let i = 1; i < head.length; i++) {
		const eq = head[i].indexOf('=');
		if (eq > 0) params[head[i].slice(0, eq).toUpperCase()] = head[i].slice(eq + 1).replace(/"/g, '');
	}
	return {name: head[0].toUpperCase(), params, value: line.slice(colon + 1)};
}

const tzFormatters: Record<string, Intl.DateTimeFormat | null> = {};

function formatterFor (tz: string): Intl.DateTimeFormat | null {
	if (tz in tzFormatters) return tzFormatters[tz];
	let f: Intl.DateTimeFormat | null = null;
	try {
		f = new Intl.DateTimeFormat('en-US', {timeZone: tz, hour12: false, year: 'numeric', month: 'numeric', day: 'numeric', hour: 'numeric', minute: 'numeric', second: 'numeric'});
	} catch { f = null; }
	tzFormatters[tz] = f;
	return f;
}

/** How far the zone's wall clock is ahead of UTC at this moment, in ms (null if the zone is unknown). */
function offsetAt (tz: string, utc: number): number | null {
	const f = formatterFor(tz);
	if (!f) return null;
	const parts: Record<string, number> = {};
	f.formatToParts(new Date(utc)).forEach((p) => { if (p.type !== 'literal') parts[p.type] = parseInt(p.value, 10); });
	const hour = parts.hour === 24 ? 0 : parts.hour;
	return Date.UTC(parts.year, parts.month - 1, parts.day, hour, parts.minute, parts.second) - Math.floor(utc / 1000) * 1000;
}

/** The moment a zone's wall-clock time happens. */
export function wallToMs (w: Wall, tz: string | null): number {
	if (tz === null) return new Date(w.y, w.mo - 1, w.d, w.h, w.mi, w.s).getTime();
	const asUtc = Date.UTC(w.y, w.mo - 1, w.d, w.h, w.mi, w.s);
	if (tz === 'UTC') return asUtc;
	const first = offsetAt(tz, asUtc);
	if (first === null) return new Date(w.y, w.mo - 1, w.d, w.h, w.mi, w.s).getTime();
	const guess = asUtc - first;
	const second = offsetAt(tz, guess);
	return second === null ? guess : asUtc - second;
}

interface Stamp {wall: Wall; tz: string | null; ms: number; allDay: boolean}

function parseStamp (p: Prop): Stamp | null {
	const m = /^(\d{4})(\d{2})(\d{2})(?:T(\d{2})(\d{2})(\d{2})?)?(Z)?$/.exec(p.value.trim());
	if (!m) return null;
	const allDay = m[4] === undefined || p.params.VALUE === 'DATE';
	const wall: Wall = {y: +m[1], mo: +m[2], d: +m[3], h: allDay ? 0 : +m[4], mi: allDay ? 0 : +m[5], s: allDay ? 0 : +(m[6] || 0)};
	const tz = allDay ? null : m[7] ? 'UTC' : p.params.TZID ? p.params.TZID : null;
	return {wall, tz, ms: wallToMs(wall, tz), allDay};
}

function parseDuration (v: string): number | null {
	const m = /^([+-])?P(?:(\d+)W)?(?:(\d+)D)?(?:T(?:(\d+)H)?(?:(\d+)M)?(?:(\d+)S)?)?$/.exec(v.trim());
	if (!m) return null;
	const ms = ((+(m[2] || 0)) * 7 + (+(m[3] || 0))) * DAY + (+(m[4] || 0)) * 3600000 + (+(m[5] || 0)) * 60000 + (+(m[6] || 0)) * 1000;
	return m[1] === '-' ? -ms : ms;
}

function parseRule (value: string): Rule | null {
	const part: Record<string, string> = {};
	value.split(';').forEach((kv) => { const i = kv.indexOf('='); if (i > 0) part[kv.slice(0, i).toUpperCase()] = kv.slice(i + 1); });
	const freq = part.FREQ as Freq;
	if (freq !== 'DAILY' && freq !== 'WEEKLY' && freq !== 'MONTHLY' && freq !== 'YEARLY') return null;
	const nums = (s: string | undefined): number[] => (s ? s.split(',').map((x) => parseInt(x, 10)).filter((x) => isFinite(x)) : []);
	const byDay: Rule['byDay'] = [];
	(part.BYDAY ? part.BYDAY.split(',') : []).forEach((s) => {
		const m = /^([+-]?\d+)?(SU|MO|TU|WE|TH|FR|SA)$/.exec(s.trim());
		if (m) byDay.push({n: m[1] ? parseInt(m[1], 10) : 0, wd: WEEKDAYS.indexOf(m[2])});
	});
	let until: number | null = null;
	if (part.UNTIL) {
		const stamp = parseStamp({name: 'UNTIL', params: {}, value: part.UNTIL});
		if (stamp) until = stamp.allDay ? stamp.ms + DAY - 1 : stamp.ms;
	}
	return {
		freq,
		interval: Math.max(1, parseInt(part.INTERVAL || '1', 10) || 1),
		count: part.COUNT ? parseInt(part.COUNT, 10) || null : null,
		until,
		byDay,
		byMonthDay: nums(part.BYMONTHDAY),
		byMonth: nums(part.BYMONTH)
	};
}

function readEvents (text: string): RawEvent[] {
	const lines = unfold(text);
	const events: RawEvent[] = [];
	let props: Prop[] | null = null;
	let depth = 0;
	for (const line of lines) {
		if (line === 'BEGIN:VEVENT') { props = []; depth = 0; continue; }
		if (line === 'END:VEVENT') {
			if (props) { const ev = buildEvent(props); if (ev) events.push(ev); }
			props = null;
			continue;
		}
		if (!props) continue;
		// nested components (alarms) have their own properties, which must not be mistaken for the event's
		if (line.indexOf('BEGIN:') === 0) { depth++; continue; }
		if (line.indexOf('END:') === 0) { depth--; continue; }
		if (depth > 0) continue;
		const p = parseProp(line);
		if (p) props.push(p);
	}
	return events;
}

function buildEvent (props: Prop[]): RawEvent | null {
	const get = (name: string): Prop | undefined => props.find((p) => p.name === name);
	const dtstart = get('DTSTART');
	const stamp = dtstart ? parseStamp(dtstart) : null;
	if (!stamp) return null;
	const dtend = get('DTEND');
	const endStamp = dtend ? parseStamp(dtend) : null;
	const dur = get('DURATION') ? parseDuration((get('DURATION') as Prop).value) : null;
	let end = endStamp ? endStamp.ms : dur !== null ? stamp.ms + dur : stamp.allDay ? stamp.ms + DAY : stamp.ms;
	if (end < stamp.ms) end = stamp.ms;
	const rrule = get('RRULE');
	const recurrence = get('RECURRENCE-ID');
	const recStamp = recurrence ? parseStamp(recurrence) : null;
	const exdates: number[] = [];
	props.forEach((p) => {
		if (p.name !== 'EXDATE') return;
		p.value.split(',').forEach((v) => {
			const s = parseStamp({name: 'EXDATE', params: p.params, value: v});
			if (s) exdates.push(s.ms);
		});
	});
	const status = get('STATUS');
	return {
		uid: get('UID') ? (get('UID') as Prop).value : '',
		title: get('SUMMARY') ? unescapeText((get('SUMMARY') as Prop).value).trim() : '',
		place: get('LOCATION') ? unescapeText((get('LOCATION') as Prop).value).replace(/\s+/g, ' ').trim() : '',
		wall: stamp.wall,
		tz: stamp.tz,
		start: stamp.ms,
		end,
		allDay: stamp.allDay,
		rule: rrule ? parseRule(rrule.value) : null,
		exdates,
		recurrenceId: recStamp ? recStamp.ms : null,
		cancelled: !!status && status.value.trim().toUpperCase() === 'CANCELLED'
	};
}

// ---------- repeating events ----------

const dayNumber = (y: number, mo: number, d: number): number => Math.floor(Date.UTC(y, mo - 1, d) / DAY);
const weekdayOf = (dn: number): number => (((dn + 4) % 7) + 7) % 7;
const daysIn = (y: number, mo: number): number => new Date(Date.UTC(y, mo, 0)).getUTCDate();

function dayMatches (rule: Rule, ev: RawEvent, dn: number, startDn: number): boolean {
	const date = new Date(dn * DAY);
	const y = date.getUTCFullYear(), mo = date.getUTCMonth() + 1, d = date.getUTCDate(), wd = weekdayOf(dn);
	const inByDay = (): boolean => rule.byDay.some((b) => {
		if (b.wd !== wd) return false;
		if (b.n === 0) return true;
		return b.n > 0 ? Math.floor((d - 1) / 7) + 1 === b.n : Math.floor((daysIn(y, mo) - d) / 7) + 1 === -b.n;
	});
	const inMonthDay = (): boolean => rule.byMonthDay.length
		? rule.byMonthDay.some((x) => (x > 0 ? x === d : daysIn(y, mo) + x + 1 === d))
		: d === ev.wall.d;
	if (rule.freq === 'DAILY') {
		return (dn - startDn) % rule.interval === 0 && (!rule.byDay.length || inByDay());
	}
	if (rule.freq === 'WEEKLY') {
		const weekStart = startDn - ((weekdayOf(startDn) + 6) % 7);
		const week = Math.floor((dn - weekStart) / 7);
		if (week % rule.interval !== 0) return false;
		return rule.byDay.length ? inByDay() : wd === weekdayOf(startDn);
	}
	if (rule.freq === 'MONTHLY') {
		const months = (y - ev.wall.y) * 12 + (mo - ev.wall.mo);
		if (months % rule.interval !== 0) return false;
		return rule.byDay.length ? inByDay() : inMonthDay();
	}
	if ((y - ev.wall.y) % rule.interval !== 0) return false;
	if (!(rule.byMonth.length ? rule.byMonth.indexOf(mo) !== -1 : mo === ev.wall.mo)) return false;
	return rule.byDay.length ? inByDay() : inMonthDay();
}

/** Start times of a repeating event that could matter between `from` and `to`. */
function repeatStarts (ev: RawEvent, rule: Rule, from: number, to: number, spanMs: number): number[] {
	const out: number[] = [];
	const startDn = dayNumber(ev.wall.y, ev.wall.mo, ev.wall.d);
	const lastDn = Math.floor(to / DAY) + 2;
	let count = 0;
	for (let dn = startDn, steps = 0; dn <= lastDn && steps < MAX_WALK_DAYS; dn++, steps++) {
		if (!dayMatches(rule, ev, dn, startDn)) continue;
		const date = new Date(dn * DAY);
		const start = wallToMs({y: date.getUTCFullYear(), mo: date.getUTCMonth() + 1, d: date.getUTCDate(), h: ev.wall.h, mi: ev.wall.mi, s: ev.wall.s}, ev.tz);
		if (rule.until !== null && start > rule.until) break;
		count++;
		if (rule.count !== null && count > rule.count) break;
		if (start + spanMs > from && start < to) out.push(start);
	}
	return out;
}

/** Every event (repeats included) that overlaps [from, to), in time order. */
export function expandIcs (text: string, from: number, to: number): Occurrence[] {
	const events = readEvents(text);
	const replaced: Record<string, number[]> = {};
	events.forEach((e) => { if (e.recurrenceId !== null) (replaced[e.uid] = replaced[e.uid] || []).push(e.recurrenceId); });
	const out: Occurrence[] = [];
	events.forEach((e) => {
		if (e.cancelled) return;
		const span = e.end - e.start;
		const add = (start: number) => {
			let end = start + span;
			if (e.allDay) {
				// an all-day event spans whole local days even when a daylight-saving change falls inside
				const days = Math.round(span / DAY);
				const d = new Date(start);
				end = new Date(d.getFullYear(), d.getMonth(), d.getDate() + days).getTime();
			}
			if ((end > from && start < to) || (span === 0 && start >= from && start < to)) out.push({start, end, allDay: e.allDay, title: e.title || tr('(no title)', 'Без названия'), place: e.place});
		};
		if (!e.rule) { add(e.start); return; }
		const skip = e.exdates.concat(replaced[e.uid] || []);
		repeatStarts(e, e.rule, from, to, span).forEach((s) => { if (skip.indexOf(s) === -1) add(s); });
	});
	return out.sort((a, b) => a.start - b.start || a.end - b.end);
}

/** The calendar's own name (X-WR-CALNAME), or an empty string. */
export function calendarName (text: string): string {
	const m = /^X-WR-CALNAME[^:\r\n]*:(.*)$/m.exec(text);
	return m ? unescapeText(m[1]).trim() : '';
}
