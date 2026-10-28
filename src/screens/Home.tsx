import {useCallback, useEffect, useMemo, useRef, useState} from 'preact/hooks';
import {CalendarCard} from '../components/CalendarCard';
import {Clock} from '../components/Clock';
import {WeatherCard} from '../components/WeatherCard';
import {Row} from '../components/Row';
import type {TileModel} from '../components/Tile';
import {useKeys, type NavKey} from '../hooks/useKeys';
import {useOkPress} from '../hooks/useOkPress';
import {useClock} from '../hooks/useClock';
import type {TimeFormat} from '../lib/clock';
import {clampPosition, initialPosition, movePosition, rowOffsets, splitRows, type RowsPosition} from '../lib/rows';
import type {ClockConfig} from '../lib/storage';
import type {WeatherView} from '../lib/weather';
import type {PlateLooks} from '../lib/plates';
import {shiftAt} from '../lib/pixelshift';
import {CAL_AHEAD_DAYS, CAL_BACK_DAYS, dayOffsetOfMonth, monthOffsetOfDay, type CalendarData} from '../lib/calendar';
import {FONT, LAYOUT} from '../theme/tokens';

export interface Appearance {
	/** icon size, % of the standard size */
	iconScale: number;
	/** px at 1080p */
	sideMargin: number;
	bottomMargin: number;
	showLabels: boolean;
	/** only the selected app's name */
	labelsSelectedOnly?: boolean;
	/** 1 or 2 rows of apps (Settings offers one row only: two leave no room for the cards) */
	rows: number;
	/** the soft shadow under the clock and the cards */
	shadows: boolean;
	/** move everything a few pixels once a minute (lib/pixelshift.ts) */
	pixelShift?: boolean;
	clock: ClockConfig;
}

interface Props {
	/** the tiles: apps, then any chosen inputs, then LG Home and Settings (in one row, or split over two) */
	items: TileModel[];
	timeFormat: TimeFormat;
	appearance: Appearance;
	active: boolean;
	/** the launcher is in the background: stop the clock ticking */
	paused?: boolean;
	onActivate: (item: TileModel) => void;
	/** OK held on a tile */
	onLongPress?: (item: TileModel) => void;
	/** counts up each time the launcher is brought to the front (Home pressed): the highlight goes back to the first tile */
	wake?: number;
	/** put the highlight on the tile with this key (after it was moved); `seq` changes for each request */
	focus?: {key: string; seq: number};
	/** the weather card, when it is on and known */
	weatherAt?: ((day: number) => WeatherView) | null;
	/** text size of the weather and calendar cards, % of the standard */
	cardSize?: number;
	/** the colours of the plates behind the clock and the cards */
	plates?: PlateLooks;
	/** the distance from the top of the screen to the block of the clock and the cards (lib/boardlayout.ts) */
	boardTop?: number;
	/** the calendar card: null when it is off */
	calendar?: {data: CalendarData | null; loading: boolean} | null;
}

/** The home screen: the clock, the weather and calendar cards, and the tiles along the bottom, in one row or two.
 *  Left/Right move along a row, Up/Down change row (two rows), OK opens, holding OK opens the app's menu.
 *  Up from the top row goes to the calendar card (when it is on): Left/Right change the day, OK returns to today,
 *  Down goes back to the apps. Back goes to the first tile. */
export function Home ({items, timeFormat, appearance, active, paused, onActivate, onLongPress, wake, focus, weatherAt = null, cardSize = 100, calendar = null, plates, boardTop = 40}: Props) {
	const now = useClock(paused);
	const shift = appearance.pixelShift ? shiftAt(now) : null;
	const [position, setPosition] = useState<RowsPosition>(initialPosition);
	const sizes = splitRows(items.length, appearance.rows);
	const offsets = rowOffsets(sizes);
	const at = clampPosition(sizes, position);
	const selectedItem = items[offsets[at.row] + at.cols[at.row]];
	const sizeKey = sizes.join(',');
	const [zone, setZone] = useState<'dock' | 'weather' | 'calendar'>('dock');
	const [weatherDay, setWeatherDay] = useState(0);
	const weather = useMemo(() => (weatherAt ? weatherAt(weatherDay) : null), [weatherAt, weatherDay]);
	const [dayOffset, setDayOffset] = useState(0);
	const [calMode, setCalMode] = useState<'day' | 'month'>('day');
	const [month, setMonth] = useState(0);
	const [dayDir, setDayDir] = useState(0);
	// The pointer (Magic Remote) over the row: the magnification follows it. The row's scroll position stays with the keys and the wheel,
	// so tiles do not slide away under the pointer.
	const [pointerPos, setPointerPos] = useState<number | null>(null);
	const [anchor, setAnchor] = useState<number[]>([0, 0]);
	const byKey = useRef(false);
	const calendarOn = !!calendar;
	const days = weather ? weather.days.length : 0;
	const live = useRef({row: at.row, calendarOn, weatherOn: !!weather, zone, days, calMode, month, dayOffset});
	live.current = {row: at.row, calendarOn, weatherOn: !!weather, zone, days, calMode, month, dayOffset};
	const weatherOn = !!weather;
	const inCalendar = zone === 'calendar' && calendarOn;
	const inWeather = zone === 'weather' && weatherOn;

	// Home pressed (the launcher comes to the front): the apps row keeps its place (the app just used stays selected); the cards go back
	// to today and the focus to the row. (The first render is skipped: nothing to reset yet.)
	const firstWake = useRef(true);
	useEffect(() => {
		if (firstWake.current) { firstWake.current = false; return; }
		setPointerPos(null); setZone('dock'); setDayOffset(0); setWeatherDay(0); setCalMode('day'); setMonth(0);
	}, [wake]);

	// the row scrolls to follow the keys (not the pointer)
	useEffect(() => { if (byKey.current) { byKey.current = false; setAnchor(clampPosition(sizes, position).cols); } }, [position]);

	// A tile was moved: the highlight follows it to its new place.
	useEffect(() => {
		if (!focus) return;
		const index = items.findIndex((item) => item.key === focus.key);
		if (index === -1) return;
		let row = 0;
		while (row < sizes.length - 1 && index >= offsets[row] + sizes[row]) row++;
		byKey.current = true;
		setPointerPos(null);
		setPosition((p) => ({row, cols: clampPosition(sizes, p).cols.map((c, r) => (r === row ? index - offsets[row] : c))}));
	}, [focus && focus.seq]);

	const monthRange = (): [number, number] => {
		const base = Date.now();
		return [monthOffsetOfDay(base, -CAL_BACK_DAYS), monthOffsetOfDay(base, CAL_AHEAD_DAYS)];
	};
	/** month view: the month moves */
	const shiftMonth = useCallback((months: number) => {
		const [lo, hi] = monthRange();
		const next = Math.min(hi, Math.max(lo, live.current.month + months));
		if (next === live.current.month) return;
		setMonth(next);
	}, []);
	/** back from the month to the days: on the day that was selected, or the first day of the month that was reached */
	const leaveMonth = useCallback(() => {
		const base = Date.now();
		const shown = live.current.month;
		if (monthOffsetOfDay(base, live.current.dayOffset) !== shown) {
			const first = dayOffsetOfMonth(base, shown);
			setDayOffset(shown === 0 ? 0 : Math.min(CAL_AHEAD_DAYS, Math.max(-CAL_BACK_DAYS, first)));
		}
		setDayDir(0);
		setCalMode('day');
	}, []);
	const enterMonth = useCallback(() => {
		setMonth(monthOffsetOfDay(Date.now(), live.current.dayOffset));
		setDayDir(0);
		setCalMode('month');
	}, []);

	const shiftDay = useCallback((days: number) => { setDayDir(days > 0 ? 1 : days < 0 ? -1 : 0); setDayOffset((o) => Math.min(CAL_AHEAD_DAYS, Math.max(-CAL_BACK_DAYS, o + days))); }, []);

	const handleKey = useCallback((key: NavKey): boolean => {
		const now = live.current;
		if (now.zone === 'weather' && now.weatherOn) {
			if (key === 'left') setWeatherDay((d) => Math.max(0, d - 1));
			else if (key === 'right') setWeatherDay((d) => Math.min(Math.max(0, now.days - 1), d + 1));
			else if (key === 'up' && now.calendarOn) setZone('calendar');
			else if (key === 'down' || key === 'back') setZone('dock');
			return true;
		}
		if (now.zone === 'calendar' && now.calendarOn) {
			if (now.calMode === 'month') {
				if (key === 'left') shiftMonth(-1);
				else if (key === 'right') shiftMonth(1);
				else if (key === 'down' || key === 'back') leaveMonth();
				return true;
			}
			if (key === 'left') shiftDay(-1);
			else if (key === 'right') shiftDay(1);
			else if (key === 'up') enterMonth();
			else if (key === 'down' || key === 'back') setZone(now.weatherOn ? 'weather' : 'dock');
			return true;
		}
		// Up from the top row climbs to the weather card, and from there to the calendar card
		if (key === 'up' && now.row === 0 && (now.weatherOn || now.calendarOn)) { setZone(now.weatherOn ? 'weather' : 'calendar'); return true; }
		// functional updates, so quick repeated presses never act on a stale position
		byKey.current = true;
		setPointerPos(null);
		if (key === 'left' || key === 'right' || key === 'up' || key === 'down') setPosition((p) => movePosition(sizes, p, key));
		else if (key === 'back') setPosition(initialPosition());        // Back jumps to the first app
		return true;            // OK is handled by useOkPress below (press vs hold)
	}, [sizeKey]);

	useKeys(handleKey, active);
	useOkPress({
		onPress: () => {
			if (live.current.zone === 'calendar' && live.current.calendarOn) { if (live.current.calMode === 'month') leaveMonth(); else { setDayDir(live.current.dayOffset > 0 ? -1 : 1); setDayOffset(0); } }
			else if (live.current.zone === 'weather' && live.current.weatherOn) setWeatherDay(0);
			else if (selectedItem) onActivate(selectedItem);
		},
		onLongPress: () => { if (selectedItem && onLongPress && live.current.zone === 'dock') onLongPress(selectedItem); }
	}, active);

	// The row's sizes are set on the row itself (not on :root), so the Settings grid keeps its own.
	const scale = appearance.iconScale / 100;
	const tileSize = Math.round(LAYOUT.tileSize * scale);
	const tileGap = Math.round(LAYOUT.tileGap * scale);
	const rowStyle = [
		`left:${appearance.sideMargin}px`,
		`right:${appearance.sideMargin}px`,
		`bottom:${appearance.bottomMargin}px`,
		`--layout-tile-size:${tileSize}px`,
		`--layout-tile-gap:${tileGap}px`,
		`--layout-tile-radius:${Math.round(LAYOUT.tileRadius * scale)}px`,
		`--font-tile-label:${Math.min(30, Math.max(16, Math.round(FONT.tileLabel * scale)))}px`
	].join(';');
	const classes = ['rows', !appearance.showLabels && 'rows--no-labels', appearance.showLabels && appearance.labelsSelectedOnly && 'rows--focus-labels', sizes.length > 1 && 'rows--multi'].filter(Boolean).join(' ');

	return (
		<div class={`screen${active ? '' : ' screen--away'}${shift ? ' screen--shift' : ''}`} style={shift ? `--shift-x:${shift.x}px;--shift-y:${shift.y}px` : undefined}>
			<div class={`board${appearance.shadows ? '' : ' board--flat'}`} style={{top: `${boardTop}px`}}>
				<div class="board__left">
					<Clock now={now} format={timeFormat} config={appearance.clock} plate={plates && plates.clock} />
					{weather && <WeatherCard view={weather} plate={plates && plates.weather} size={cardSize} now={now} focused={inWeather && active} onPick={(day) => { setZone('weather'); setWeatherDay(day); }} />}
				</div>
				{calendar && <CalendarCard plate={plates && plates.calendar} data={calendar.data} offset={dayOffset} now={now} format={timeFormat} focused={inCalendar && active} mode={calMode} month={month} dayDir={dayDir} size={cardSize} loading={calendar.loading} onShift={(days) => { setZone('calendar'); shiftDay(days); }} onMonthShift={(months) => { setZone('calendar'); shiftMonth(months); }} />}
			</div>
			<div class={classes} style={rowStyle}>
				{sizes.map((size, r) => {
					const start = offsets[r];
					return (
						<Row
							key={r}
							items={items.slice(start, start + size)}
							focused={r === at.row}
							dock
							scrollIndex={anchor[r]}
							pointerPos={r === at.row ? pointerPos : null}
							onPointer={(p) => {
								setPointerPos(p);
								if (p !== null) {
									const col = Math.min(size - 1, Math.max(0, Math.round(p)));
									if (col !== at.cols[r] || r !== at.row) setPosition({row: r, cols: at.cols.map((c, i) => (i === r ? col : c))});
								}
							}}
							onWheelStep={(direction) => setAnchor((a) => a.map((c, i) => (i === r ? Math.min(size - 1, Math.max(0, Math.round(c) + direction)) : c)))}
							ring={r === at.row && !inCalendar && !inWeather}
							selected={at.cols[r]}
							onSelect={(col) => setPosition({row: r, cols: at.cols.map((c, i) => (i === r ? col : c))})}
							onActivate={(col) => onActivate(items[start + col])}
							tileSize={tileSize}
							tileGap={tileGap}
							viewportWidth={LAYOUT.screenWidth - 2 * appearance.sideMargin}
						/>
					);
				})}
			</div>
		</div>
	);
}
