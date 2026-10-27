// The calendar card: one day's events from an iCal feed (Google Calendar's secret address). This file is the
// pure part (turning the feed into days, the saved copy, the words); weathernet.ts' fetchText does the fetching.

import {calendarName, expandIcs, type Occurrence} from './ics';
import {dayTitle, monthName, timeParts, type TimeFormat} from './clock';
import {count, fmt, tr} from './i18n';

export const CAL_CACHE_KEY = 'customlauncher-calendar-v1';
/** How far back and forward the card can be moved (days from today). */
export const CAL_BACK_DAYS = 62;
export const CAL_AHEAD_DAYS = 186;
/** A copy younger than this is not fetched again. */
export const CAL_FRESH_MS = 15 * 60 * 1000;
/** An older copy than this is no longer shown. */
export const CAL_SHOWN_MS = 48 * 60 * 60 * 1000;

/** One calendar of the card: an address, a file or a file found in a folder. */
export interface CalendarSource {
	/** where it is read from */
	src: string;
	name: string;
	/** a file's size and change time when it was read: an unchanged file is not read again */
	sig: string;
}

export interface CalendarData {
	/** the sources this is from, as the settings list them (a different list means different data) */
	key: string;
	/** when it was read (ms since 1970) */
	at: number;
	sources: CalendarSource[];
	/** every event of all the calendars; `src` is the index in `sources` */
	events: Occurrence[];
}

/** The colour of each calendar's bar, in the order the calendars are read. */
export const CAL_COLORS = ['#ffc36b', '#6fb4ff', '#ff8fb1', '#6fe3a0', '#c39bff', '#ff9d6b'];

/** Local midnight `offset` days from the day `now` falls in. */
export function startOfDay (now: number, offset: number): number {
	const d = new Date(now);
	return new Date(d.getFullYear(), d.getMonth(), d.getDate() + offset).getTime();
}

/** Reads a calendar's text into the events of the days the card can show; null when the text is not a calendar. */
export function parseCalendar (text: string, src: string, sig: string, index: number, now: number): {source: CalendarSource; events: Occurrence[]} | null {
	if (text.indexOf('BEGIN:VCALENDAR') === -1) return null;
	const events = expandIcs(text, startOfDay(now, -CAL_BACK_DAYS), startOfDay(now, CAL_AHEAD_DAYS + 1));
	events.forEach((e) => { e.src = index; });
	return {source: {src, name: calendarName(text), sig}, events};
}

/** Puts the calendars read one by one together. */
export function joinCalendars (key: string, parts: {source: CalendarSource; events: Occurrence[]}[], now: number): CalendarData {
	const events: Occurrence[] = [];
	parts.forEach((p) => p.events.forEach((e) => events.push(e)));
	events.sort((a, b) => a.start - b.start || a.end - b.end);
	return {key, at: now, sources: parts.map((p) => p.source), events};
}

/** The address of a web calendar as it is fetched: "webcal://" is the same thing over https; spaces from typing are dropped. */
export function normalizeUrl (raw: string): string {
	const url = raw.replace(/\s+/g, '');
	return url.replace(/^webcal:\/\//i, 'https://');
}

export function validUrl (url: string): boolean {
	return /^https?:\/\/[^\s'"`$\\]+$/i.test(url) && url.length <= 600;
}

/** A file or a folder on the TV (a folder ends with "/"): plain characters only, since it goes into a shell command. */
export function validPath (path: string): boolean {
	return /^\/[A-Za-z0-9._@\/-]+$/.test(path) && path.length <= 300 && path.indexOf('..') === -1;
}

/** Something the card can read: a web address, a file or a folder. */
export function validSource (src: string): boolean {
	return validUrl(src) || validPath(src);
}

export interface DayEvent {
	/** "10:00", or "All day" */
	time: string;
	/** the small line under the time: "until 11:00", "now" */
	note: string;
	title: string;
	place: string;
	/** which calendar (an index into `data.sources`) */
	src: number;
	state: 'past' | 'now' | 'next' | 'allday';
}

function clock (ms: number, format: TimeFormat): string {
	const p = timeParts(new Date(ms), format);
	return p.period ? p.time + ' ' + p.period : p.time;
}

/** The events of the day that starts at `dayStart`, with what is over and what is going on at `now`. */
export function eventsOfDay (data: CalendarData | null, dayStart: number, now: number, format: TimeFormat): DayEvent[] {
	if (!data) return [];
	const dayEnd = startOfDay(dayStart, 1);
	const todayStart = startOfDay(now, 0);
	const list = data.events.filter((e) => e.end > dayStart && e.start < dayEnd || (e.start === e.end && e.start >= dayStart && e.start < dayEnd));
	list.sort((a, b) => Number(b.allDay) - Number(a.allDay) || a.start - b.start);
	return list.map((e): DayEvent => {
		if (e.allDay) return {time: tr('All day', 'Весь день'), note: '', title: e.title, place: e.place, src: e.src || 0, state: 'allday'};
		const startsToday = e.start >= dayStart;
		const endsToday = e.end <= dayEnd;
		let state: DayEvent['state'] = 'next';
		if (dayStart < todayStart) state = 'past';
		else if (dayStart === todayStart) state = (e.end <= now && e.end > e.start) || (e.end === e.start && e.start < now) ? 'past' : e.start <= now ? 'now' : 'next';
		let note = e.end > e.start && endsToday ? fmt(tr('until {0}', 'до {0}'), clock(e.end, format)) : e.end > e.start ? tr('until tomorrow', 'до следующего дня') : '';
		if (state === 'now') note = tr('now', 'сейчас');
		return {time: startsToday ? clock(e.start, format) : tr('since yesterday', 'с вчера'), note, title: e.title, place: e.place, src: e.src || 0, state};
	});
}

/** How many events the day has (for the dots under the days). */
export function countOfDay (data: CalendarData | null, dayStart: number): number {
	if (!data) return 0;
	const dayEnd = startOfDay(dayStart, 1);
	return data.events.filter((e) => e.end > dayStart && e.start < dayEnd || (e.start === e.end && e.start >= dayStart && e.start < dayEnd)).length;
}

/** "Today", "Tomorrow", "In 3 days", "2 days ago". */
export function relativeDay (offset: number): string {
	if (offset === 0) return tr('Today', 'Сегодня');
	if (offset === 1) return tr('Tomorrow', 'Завтра');
	if (offset === -1) return tr('Yesterday', 'Вчера');
	if (offset === 2) return tr('In 2 days', 'Послезавтра');
	const words = count(Math.abs(offset), ['day', 'days'], ['день', 'дня', 'дней']);
	return fmt(offset > 0 ? tr('In {0}', 'Через {0}') : tr('{0} ago', '{0} назад'), words);
}

/** "Saturday, 24 October" for the day `offset` days from today. */
export function dayHeading (now: number, offset: number): string {
	return dayTitle(new Date(startOfDay(now, offset)));
}

export function readCalendarCache (): CalendarData | null {
	try {
		const raw = localStorage.getItem(CAL_CACHE_KEY);
		if (!raw) return null;
		const d = JSON.parse(raw);
		if (!d || typeof d.key !== 'string' || typeof d.at !== 'number' || !Array.isArray(d.events) || !Array.isArray(d.sources)) return null;
		const events: Occurrence[] = [];
		d.events.forEach((e: Partial<Occurrence>) => {
			if (typeof e.start === 'number' && typeof e.end === 'number' && typeof e.title === 'string') events.push({start: e.start, end: e.end, allDay: e.allDay === true, title: e.title, place: typeof e.place === 'string' ? e.place : '', src: typeof e.src === 'number' ? e.src : 0});
		});
		const sources: CalendarSource[] = [];
		d.sources.forEach((s: Partial<CalendarSource>) => {
			if (typeof s.src === 'string') sources.push({src: s.src, name: typeof s.name === 'string' ? s.name : '', sig: typeof s.sig === 'string' ? s.sig : ''});
		});
		return {key: d.key, at: d.at, sources, events};
	} catch {
		return null;
	}
}

export function writeCalendarCache (data: CalendarData): void {
	try { localStorage.setItem(CAL_CACHE_KEY, JSON.stringify(data)); } catch { /* storage full: it is just fetched again */ }
}

export function usableCalendar (data: CalendarData | null, key: string, now: number): CalendarData | null {
	return data && data.key === key && now - data.at < CAL_SHOWN_MS && now >= data.at ? data : null;
}

export function calendarIsFresh (data: CalendarData | null, key: string, now: number): boolean {
	return !!data && data.key === key && now - data.at < CAL_FRESH_MS && now >= data.at;
}

/** How many events per calendar the day has, in the calendars' order (for the dots under the days). */
export function dotsOfDay (data: CalendarData | null, dayStart: number): number[] {
	if (!data) return [];
	const dayEnd = startOfDay(dayStart, 1);
	const seen: Record<number, boolean> = {};
	data.events.forEach((e) => { if ((e.end > dayStart && e.start < dayEnd) || (e.start === e.end && e.start >= dayStart && e.start < dayEnd)) seen[e.src || 0] = true; });
	return data.sources.map((_s, i) => i).filter((i) => seen[i]);
}

/** Which calendars have events on each day of [from, to): day start (ms) → calendar indexes. */
export function sourcesByDay (data: CalendarData | null, from: number, to: number): Record<number, number[]> {
	const out: Record<number, number[]> = {};
	if (!data) return out;
	data.events.forEach((e) => {
		const last = startOfDay(Math.max(e.start, e.end - 1), 0);
		for (let day = startOfDay(e.start, 0); day <= last; day = startOfDay(day, 1)) {
			if (day < from || day >= to) continue;
			const list = out[day] || (out[day] = []);
			const src = e.src || 0;
			if (list.indexOf(src) === -1) list.push(src);
		}
	});
	return out;
}

export interface MonthCell {
	/** local midnight of the day */
	start: number;
	day: number;
	inMonth: boolean;
	today: boolean;
	/** the day the day view is on */
	selected: boolean;
	/** calendars with events that day (sorted) */
	dots: number[];
}

export interface MonthView {
	/** "October" */
	name: string;
	year: number;
	/** 6 weeks of 7 days, Monday first */
	weeks: MonthCell[][];
}

/** The month `monthOffset` months from the current one, as 6 weeks (days of the neighbouring months included, dimmed). */
export function monthView (data: CalendarData | null, now: number, monthOffset: number, selectedDay: number): MonthView {
	const today = startOfDay(now, 0);
	const t = new Date(now);
	const first = new Date(t.getFullYear(), t.getMonth() + monthOffset, 1);
	const lead = (first.getDay() + 6) % 7;
	const gridStart = new Date(first.getFullYear(), first.getMonth(), 1 - lead).getTime();
	const gridEnd = startOfDay(gridStart, 42);
	const by = sourcesByDay(data, gridStart, gridEnd);
	const weeks: MonthCell[][] = [];
	for (let w = 0; w < 6; w++) {
		const row: MonthCell[] = [];
		for (let d = 0; d < 7; d++) {
			const start = startOfDay(gridStart, w * 7 + d);
			const date = new Date(start);
			row.push({start, day: date.getDate(), inMonth: date.getMonth() === first.getMonth(), today: start === today, selected: start === selectedDay, dots: (by[start] || []).slice().sort((a, b) => a - b)});
		}
		weeks.push(row);
	}
	return {name: monthName(first.getMonth()), year: first.getFullYear(), weeks};
}

/** Whole months from the month `now` is in to the month `dayOffset` days from today is in. */
export function monthOffsetOfDay (now: number, dayOffset: number): number {
	const a = new Date(now), b = new Date(startOfDay(now, dayOffset));
	return (b.getFullYear() - a.getFullYear()) * 12 + b.getMonth() - a.getMonth();
}

/** The first day of the month `monthOffset` months away, as days from today. */
export function dayOffsetOfMonth (now: number, monthOffset: number): number {
	const t = new Date(now);
	const first = new Date(t.getFullYear(), t.getMonth() + monthOffset, 1).getTime();
	return Math.round((first - startOfDay(now, 0)) / 86400000);
}
