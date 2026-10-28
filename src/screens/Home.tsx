import {useCallback, useEffect, useMemo, useRef, useState} from 'preact/hooks';
import {CalendarCard} from '../components/CalendarCard';
import {Clock} from '../components/Clock';
import {WeatherCard} from '../components/WeatherCard';
import {NewsCard} from '../components/NewsCard';
import {NewsDetail} from '../components/NewsDetail';
import {QuoteCard} from '../components/QuoteCard';
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
import {columnAt, entryCard, nextInColumn, type BoardCard, type BoardLayout, type CardId} from '../lib/boardlayout';
import {headlinesPerPage, type FeedItem} from '../lib/feeds';
import type {Quote} from '../lib/quote';

type Zone = 'dock' | 'weather' | 'calendar' | 'news';
/** a page of headlines turns by itself this often while the remote is not on the card */
const NEWS_PAGE_MS = 20000;

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
	/** the news card: null when it is off */
	news?: {items: FeedItem[] | null; loading: boolean} | null;
	/** the quote of the day: null when it is off */
	quote?: {quote: Quote | null; loading: boolean} | null;
	/** where the cards stand (lib/boardlayout.ts) */
	layout: BoardLayout;
}

/** The home screen: the clock and the cards in columns (lib/boardlayout.ts), and the tiles along the bottom, in one row or two.
 *  Left/Right move along a row, Up/Down change row (two rows), OK opens, holding OK opens the app's menu.
 *  Up from the top row goes to the card above the selected tile; Up and Down move between the cards of a column, and Up from a card
 *  with nothing above it goes to the next column on the right. On the weather Left/Right change the day; on the calendar they change
 *  the day and Up opens the month; on the news they turn the page, Up/Down choose a headline and OK shows it. Down from the lowest
 *  card goes back to the apps. Back goes to the first tile. */
export function Home ({items, timeFormat, appearance, active, paused, onActivate, onLongPress, wake, focus, weatherAt = null, cardSize = 100, calendar = null, news = null, quote = null, layout, plates, boardTop = 40}: Props) {
	const now = useClock(paused);
	const shift = appearance.pixelShift ? shiftAt(now) : null;
	const [position, setPosition] = useState<RowsPosition>(initialPosition);
	const sizes = splitRows(items.length, appearance.rows);
	const offsets = rowOffsets(sizes);
	const at = clampPosition(sizes, position);
	const selectedItem = items[offsets[at.row] + at.cols[at.row]];
	const sizeKey = sizes.join(',');
	const [zone, setZone] = useState<Zone>('dock');
	const [newsPage, setNewsPage] = useState(0);
	const [newsSel, setNewsSel] = useState(0);
	const [detail, setDetail] = useState<FeedItem | null>(null);
	const screenRef = useRef<HTMLDivElement>(null);
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
	const newsOn = !!news;
	const newsCard = layout.cards.news;
	const perPage = newsCard ? headlinesPerPage(newsCard.h, cardSize / 100) : 1;
	const newsCount = news && news.items ? news.items.length : 0;
	const newsPages = Math.max(1, Math.ceil(newsCount / perPage));
	const live = useRef({row: at.row, calendarOn, weatherOn: !!weather, newsOn, zone, days, calMode, month, dayOffset, layout, perPage, newsCount, newsPages, newsPage, newsSel, detail});
	live.current = {row: at.row, calendarOn, weatherOn: !!weather, newsOn, zone, days, calMode, month, dayOffset, layout, perPage, newsCount, newsPages, newsPage, newsSel, detail};
	const weatherOn = !!weather;
	const inCalendar = zone === 'calendar' && calendarOn;
	const inWeather = zone === 'weather' && weatherOn;
	const inNews = zone === 'news' && newsOn;

	// Home pressed (the launcher comes to the front): the apps row keeps its place (the app just used stays selected); the cards go back
	// to today and the focus to the row. (The first render is skipped: nothing to reset yet.)
	const firstWake = useRef(true);
	useEffect(() => {
		if (firstWake.current) { firstWake.current = false; return; }
		setPointerPos(null); setZone('dock'); setDayOffset(0); setWeatherDay(0); setCalMode('day'); setMonth(0); setNewsPage(0); setDetail(null);
	}, [wake]);

	// The pages of headlines turn by themselves while the remote is elsewhere (and the screen is in front), back to the first after the last.
	useEffect(() => {
		if (!newsOn || inNews || detail || !active || paused || newsPages < 2) return undefined;
		const timer = window.setInterval(() => setNewsPage((page) => (page + 1) % live.current.newsPages), NEWS_PAGE_MS);
		return () => window.clearInterval(timer);
	}, [newsOn, inNews, !!detail, active, paused, newsPages]);
	// fewer headlines than before: stay on a page that exists
	useEffect(() => { if (newsPage >= newsPages) setNewsPage(0); }, [newsPage, newsPages]);

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

	/** Where the selected tile of the app row is across the screen, in the 1920 px of the layout (the middle when it is not known). */
	const selectedTileX = (): number => {
		const screen = screenRef.current;
		const tile = screen ? screen.querySelector('.row--focused .tile--focused') as HTMLElement | null : null;
		if (!screen || !tile) return LAYOUT.screenWidth / 2;
		const box = screen.getBoundingClientRect(), r = tile.getBoundingClientRect();
		return box.width ? (r.left + r.width / 2 - box.left) * LAYOUT.screenWidth / box.width : LAYOUT.screenWidth / 2;
	};
	/** Steps onto a card: on the news, the first headline of the page shown. */
	const enter = (card: CardId | null): boolean => {
		if (card !== 'weather' && card !== 'calendar' && card !== 'news') return false;
		if (card === 'news') setNewsSel(live.current.newsPage * live.current.perPage);
		setZone(card);
		return true;
	};
	/** The card above, or (with nothing above) the card the remote steps onto in the next column on the right. */
	const enterAbove = (from: CardId): void => {
		const l = live.current.layout;
		if (enter(nextInColumn(l, from, -1))) return;
		const index = l.columns.findIndex((c) => c.cards.some((card) => card.id === from));
		for (let i = index + 1; i < l.columns.length; i++) if (enter(entryCard(l, i))) return;
	};
	const leaveDown = (from: CardId): void => { if (!enter(nextInColumn(live.current.layout, from, 1))) setZone('dock'); };

	const handleKey = useCallback((key: NavKey): boolean => {
		const now = live.current;
		if (now.detail) {
			if (key === 'back') setDetail(null);            // OK closes it in useOkPress below
			return true;
		}
		if (now.zone === 'weather' && now.weatherOn) {
			if (key === 'left') setWeatherDay((d) => Math.max(0, d - 1));
			else if (key === 'right') setWeatherDay((d) => Math.min(Math.max(0, now.days - 1), d + 1));
			else if (key === 'up') enterAbove('weather');
			else if (key === 'down') leaveDown('weather');
			else if (key === 'back') setZone('dock');
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
			else if (key === 'down') leaveDown('calendar');
			else if (key === 'back') setZone('dock');
			return true;
		}
		if (now.zone === 'news' && now.newsOn) {
			const first = now.newsPage * now.perPage;
			const last = Math.min(now.newsCount, first + now.perPage) - 1;
			if (key === 'left' || key === 'right') {
				const page = Math.min(now.newsPages - 1, Math.max(0, now.newsPage + (key === 'left' ? -1 : 1)));
				setNewsPage(page);
				setNewsSel(page * now.perPage);
			} else if (key === 'up') {
				if (now.newsSel > first) setNewsSel(now.newsSel - 1);
				else enterAbove('news');
			} else if (key === 'down') {
				if (now.newsSel < last) setNewsSel(now.newsSel + 1);
				else leaveDown('news');
			} else if (key === 'back') setZone('dock');
			return true;
		}
		// Up from the top row climbs to the card above the selected tile
		if (key === 'up' && now.row === 0) {
			const l = now.layout;
			if (enter(entryCard(l, columnAt(l, selectedTileX())))) return true;
		}
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
			if (live.current.detail) { setDetail(null); return; }
			if (live.current.zone === 'news' && live.current.newsOn) {
				const list = news && news.items;
				const item = list ? list[live.current.newsSel] : undefined;
				if (item) setDetail(item);
				return;
			}
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
	/** One card of the board, in its column. */
	const card = (c: BoardCard) => {
		if (c.id === 'clock') return <Clock key="clock" now={now} format={timeFormat} config={appearance.clock} plate={plates && plates.clock} narrow={layout.narrow} />;
		if (c.id === 'weather') return weather && <WeatherCard key="weather" view={weather} plate={plates && plates.weather} size={cardSize} now={now} width={c.w} focused={inWeather && active} onPick={(day) => { setZone('weather'); setWeatherDay(day); }} />;
		if (c.id === 'calendar') return calendar && <CalendarCard key="calendar" plate={plates && plates.calendar} data={calendar.data} offset={dayOffset} now={now} format={timeFormat} focused={inCalendar && active} mode={calMode} month={month} dayDir={dayDir} size={cardSize} loading={calendar.loading} height={c.h} onShift={(days) => { setZone('calendar'); shiftDay(days); }} onMonthShift={(months) => { setZone('calendar'); shiftMonth(months); }} />;
		if (c.id === 'news') return news && <NewsCard key="news" items={news.items} loading={news.loading} page={newsPage} selected={newsSel} focused={inNews && active} height={c.h} size={cardSize} now={now} plate={plates && plates.news} onPick={(index) => { const item = news.items && news.items[index]; setZone('news'); setNewsSel(index); if (item) setDetail(item); }} />;
		return quote && <QuoteCard key="quote" quote={quote.quote} loading={quote.loading} height={c.h} size={cardSize} plate={plates && plates.quote} />;
	};

	const classes = ['rows', !appearance.showLabels && 'rows--no-labels', appearance.showLabels && appearance.labelsSelectedOnly && 'rows--focus-labels', sizes.length > 1 && 'rows--multi'].filter(Boolean).join(' ');

	return (
		<div ref={screenRef} class={`screen${active ? '' : ' screen--away'}${shift ? ' screen--shift' : ''}`} style={shift ? `--shift-x:${shift.x}px;--shift-y:${shift.y}px` : undefined}>
			<div class={`board${appearance.shadows ? '' : ' board--flat'}`} style={{top: `${boardTop}px`}}>
				{layout.columns.map((column, i) => (
					<div key={i} class="board__col" style={{left: `${column.x}px`, width: `${column.w}px`}}>
						{column.cards.map(card)}
					</div>
				))}
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
							ring={r === at.row && !inCalendar && !inWeather && !inNews}
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
			{detail && <NewsDetail item={detail} now={now} />}
		</div>
	);
}
