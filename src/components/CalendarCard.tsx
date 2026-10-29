import {CAL_COLORS, dayHeading, dotsOfDay, eventsOfDay, monthView, relativeDay, startOfDay, type CalendarData} from '../lib/calendar';
import {dayShort} from '../lib/clock';
import {tr} from '../lib/i18n';
import {plateVars, type PlateLook} from '../lib/plates';
import {Frost} from './Frost';
import {memoProps} from '../lib/memoprops';
import {useEffect, useLayoutEffect, useMemo, useRef, useState} from 'preact/hooks';
import type {TimeFormat} from '../lib/clock';

/** Monday first. */
const WEEKDAYS = [1, 2, 3, 4, 5, 6, 0];

interface Props {
	data: CalendarData | null;
	plate?: PlateLook;
	/** days from today (negative: past) */
	offset: number;
	now: Date;
	format: TimeFormat;
	/** the TV remote is on this card */
	focused: boolean;
	/** 'day': the events of one day. 'month': the days of a month with dots */
	mode: 'day' | 'month';
	/** the month shown in month mode, in months from the current one */
	month: number;
	/** which way the day just moved (1: forward, -1: back, 0: no slide), for the slide */
	dayDir: number;
	/** text size, % of the standard */
	size: number;
	/** the address is set but nothing has been read from it yet */
	loading: boolean;
	/** the card's height on the screen, px (shorter with the quote of the day under it) */
	height?: number;
	onShift: (days: number) => void;
	onMonthShift: (months: number) => void;
	/** the pointer came onto the card */
	onHover?: () => void;
	/** where the remote is on the card, when it is focused: the events (or the month's days), or the head with the date and arrows */
	part?: 'body' | 'head';
	/** how many of the day's events are scrolled past (↑ ↓ on the card, or the wheel) */
	scroll?: number;
	/** how far the day's events can scroll (0: they all fit) */
	onScrollMax?: (max: number) => void;
	/** the wheel turned over the events: -1 or 1 */
	onScrollBy?: (step: number) => void;
}

/** The events of one day, with the days around it; Up turns it into the month, with the same two layers cross-fading
 *  (the day view shrinks away, the month grows in). The days and the months move as one continuous strip, with no fading,
 *  so the eye can follow where they went; only the list of events fades. */
function CalendarCardView ({data, plate, offset, now, format, focused, mode, month, dayDir, size, loading, onShift, onMonthShift, onHover, part = 'body', scroll = 0, onScrollMax, onScrollBy}: Props) {
	const nowMs = now.getTime();
	const dayStart = startOfDay(nowMs, offset);
	const inMonth = mode === 'month';
	const events = useMemo(() => eventsOfDay(data, dayStart, nowMs, format), [data, dayStart, nowMs, format]);
	const scale = size / 100;
	// All the day's events are in one list that scrolls inside the card, an event at a time: `scroll` is the first one at the top. It
	// stops when the last one is in view. Where events are hidden above or below, the list fades out at that edge.
	const area = useRef<HTMLDivElement>(null);
	const track = useRef<HTMLDivElement>(null);
	const [maxScroll, setMaxScroll] = useState(0);
	const first = Math.min(scroll, maxScroll);
	useLayoutEffect(() => {
		const box = area.current, list = track.current;
		if (!box || !list) return;
		const rows = list.children as HTMLCollectionOf<HTMLElement>;
		const room = box.clientHeight, total = list.offsetHeight;
		let max = 0;
		while (max < rows.length - 1 && total - rows[max].offsetTop > room) max++;
		if (max !== maxScroll) setMaxScroll(max);
		const top = rows[Math.min(first, max)] ? rows[Math.min(first, max)].offsetTop : 0;
		list.style.transform = `translate3d(0, ${-top}px, 0)`;
		box.classList.toggle('cal__events--above', top > 0);
		box.classList.toggle('cal__events--below', total - top > room + 1);
	});
	useEffect(() => { if (onScrollMax) onScrollMax(maxScroll); }, [maxScroll]);
	const names = data ? data.sources.map((s) => s.name) : [];
	const many = names.length > 1;
	// a window of days around the chosen one; each keeps its own element, so the strip slides instead of being redrawn
	const strip: number[] = [];
	for (let o = offset - 9; o <= offset + 9; o++) strip.push(o);
	const views = useMemo(() => [month - 1, month, month + 1].map((m) => ({m, view: monthView(data, nowMs, m, dayStart)})), [data, nowMs, month, dayStart]);
	// the little dots under the days of the strip
	const stripDots = useMemo(() => strip.map((o) => dotsOfDay(data, startOfDay(nowMs, o)).slice(0, 4)), [data, nowMs, offset]);
	const view = views[1].view;

	const title = inMonth ? String(view.year) : relativeDay(offset);
	const heading = inMonth ? view.name : dayHeading(nowMs, offset);
	return (
		<div class={`card cal${focused ? ' cal--focus' : ''}${focused && part === 'head' ? ' cal--head' : ''}${inMonth ? ' cal--month' : ''}`} style={{'--k': String(scale), ...plateVars(plate)}} onMouseEnter={onHover}>
			<Frost plate={plate} />
			<div class="cal__head">
				<button class="cal__nav" aria-label={inMonth ? tr('Previous month', 'Предыдущий месяц') : tr('Previous day', 'Предыдущий день')} onClick={() => (inMonth ? onMonthShift(-1) : onShift(-1))}>
					<svg viewBox="0 0 24 24"><path d="M15 5l-7 7 7 7" /></svg>
				</button>
				<div class="cal__title" key={mode + title + heading}>
					<div class="cal__rel">{title}</div>
					<div class="cal__full">{heading}</div>
				</div>
				<button class="cal__nav" aria-label={inMonth ? tr('Next month', 'Следующий месяц') : tr('Next day', 'Следующий день')} onClick={() => (inMonth ? onMonthShift(1) : onShift(1))}>
					<svg viewBox="0 0 24 24"><path d="M9 5l7 7-7 7" /></svg>
				</button>
			</div>
			<div class="cal__stage">
				<div class="cal__layer cal__layer--day">
					<div class="cal__strip">
						<div class="cal__hl" />
						<div class="cal__track" style={{transform: `translateX(${(-offset * 100 / 7).toFixed(4)}%)`}}>
							{strip.map((o) => {
								const start = startOfDay(nowMs, o);
								const date = new Date(start);
								const dots = stripDots[o - (offset - 9)];
								return (
									<div key={o} class={`cal__day${o === offset ? ' cal__day--sel' : ''}${o === 0 ? ' cal__day--today' : ''}`} style={{left: `${((o + 3) * 100 / 7).toFixed(4)}%`}} onClick={() => onShift(o - offset)}>
										<span>{dayShort(date.getDay())}</span>
										<b>{date.getDate()}</b>
										<em>{dots.map((d) => <i key={d} style={{background: CAL_COLORS[d % CAL_COLORS.length]}} />)}</em>
									</div>
								);
							})}
						</div>
					</div>
					<div class={`cal__events${dayDir > 0 ? " cal__slide--fwd" : dayDir < 0 ? " cal__slide--back" : ""}`} key={"e" + offset} ref={area} onWheel={(event) => { if (onScrollBy && event.deltaY !== 0) onScrollBy(event.deltaY > 0 ? 1 : -1); }}>
						<div class="cal__evtrack" ref={track}>
						{events.map((e, i) => (
							<div key={i} class={`cal__ev cal__ev--${e.state}`}>
								<div class="cal__time">{e.time}{e.note && <small>{e.note}</small>}</div>
								<div class="cal__bar" style={{background: CAL_COLORS[e.src % CAL_COLORS.length]}} />
								<div class="cal__body">
									<div class="cal__name">{e.title}</div>
									{(e.place || (many && names[e.src])) && <div class="cal__place">{[e.place, many ? names[e.src] : ''].filter((x) => x).join(' · ')}</div>}
								</div>
							</div>
						))}
						</div>
						{events.length === 0 && (
							<div class="cal__empty">{loading ? tr('Loading the calendar…', 'Загружаю календарь…') : data ? tr('Nothing on this day.', 'В этот день событий нет.') : tr('The calendar has not loaded yet.', 'Календарь пока не загрузился.')}</div>
						)}
					</div>
				</div>
				<div class="cal__layer cal__layer--month">
					<div class="cal__weekdays">{WEEKDAYS.map((w) => <span key={w}>{dayShort(w)}</span>)}</div>
					<div class="cal__months">
						<div class="cal__mtrack" style={{transform: `translateX(${-month * 100}%)`}}>
							{views.map((v) => (
								<div key={v.m} class="cal__mpanel" style={{left: `${v.m * 100}%`}}>
									{v.view.weeks.map((week, w) => (
										<div key={w} class="cal__week">
											{week.map((c) => (
												<div key={c.start} class={`cal__cell${c.inMonth ? '' : ' cal__cell--out'}${c.today ? ' cal__cell--today' : ''}${c.selected ? ' cal__cell--sel' : ''}`}>
													<b>{c.day}</b>
													<em>{c.dots.slice(0, 4).map((d) => <i key={d} style={{background: CAL_COLORS[d % CAL_COLORS.length]}} />)}</em>
												</div>
											))}
										</div>
									))}
								</div>
							))}
						</div>
					</div>
				</div>
			</div>
		</div>
	);
}

export const CalendarCard = memoProps(CalendarCardView);
