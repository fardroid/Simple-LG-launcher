// Where the cards stand. The block keeps its size (686 px tall); it is moved so that the space above it (to the top of the
// screen) and the space below it (to the top of the selected app icon, which grows a little) are the same. That depends on the
// icon size, the bottom margin, the names under the icons and the rows.
//
// Inside the block the cards stand in columns, 40 px apart, all ending on the same line:
//  - two columns (880 + 600 px): the clock and the weather on the left; on the right the calendar or the news, with the quote of
//    the day under it when that is on;
//  - three columns of 560 px when the calendar and the news are both on: the clock and the weather, the calendar, the news (with
//    the quote under it).
// The weather, the calendar and the news take whatever height their column leaves; the clock and the quote have their own.
// A card that is alone in its column takes it whole (a quote alone gets the full height).

import {FONT, LAYOUT, MOTION} from '../theme/tokens';
import {splitRows} from './rows';

export const BOARD_HEIGHT = 686;
export const BOARD_GAP = 40;
const MIN_TOP = 16;
const ROW_GAP = 14;           // between two rows of apps (see .rows--multi in global.css)

const LEFT_WIDTH = 880;
const RIGHT_WIDTH = 600;
const THIRD_WIDTH = 560;
/** the quote of the day under the calendar or the news */
const QUOTE_HEIGHT = 128;
const QUOTE_HEIGHT_THIRD = 176;
/** the clock card's padding (top and bottom, see .clock in global.css) and its side padding */
const CLOCK_PAD = 12;
const CLOCK_SIDE = 40;

export interface BoardInput {
	iconScale: number;
	bottomMargin: number;
	showLabels: boolean;
	rows: number;
}

/** The distance from the top of the 1080p screen to the block, in px. `itemCount` is how many tiles are on the home row. */
export function boardTop (a: BoardInput, itemCount: number): number {
	const scale = a.iconScale / 100;
	const tile = Math.round(LAYOUT.tileSize * scale);
	const font = Math.min(30, Math.max(16, Math.round(FONT.tileLabel * scale)));
	const label = a.showLabels ? font * 2.4 + 14 : 0;
	const rows = splitRows(itemCount, a.rows).length;
	const rowsHeight = rows * (tile + label) + (rows - 1) * ROW_GAP;
	const rowsTop = LAYOUT.screenHeight - a.bottomMargin - rowsHeight;
	const biggestTop = rowsTop - tile * (MOTION.focusScale - 1);       // the selected tile grows upwards from its bottom edge
	return Math.max(MIN_TOP, Math.round((biggestTop - BOARD_HEIGHT) / 2));
}

export type CardId = 'clock' | 'weather' | 'calendar' | 'news' | 'quote';

/** What the clock shows, which sets its height. */
export interface ClockShape {
	/** height of the digits, px */
	size: number;
	showDate: boolean;
	/** % of the standard date size */
	dateSize: number;
	/** AM/PM beside the time */
	period: boolean;
}

export interface Cards {
	weather: boolean;
	calendar: boolean;
	news: boolean;
	quote: boolean;
	clock: ClockShape;
}

export interface BoardCard {
	id: CardId;
	x: number;
	y: number;
	w: number;
	h: number;
}

export interface BoardColumn {
	x: number;
	w: number;
	cards: BoardCard[];
}

export interface BoardLayout {
	/** the clock stands in a narrow column: the date goes under the time */
	narrow: boolean;
	columns: BoardColumn[];
	/** the cards on the screen, by name */
	cards: Partial<Record<CardId, BoardCard>>;
}

/** The size of the digits in a narrow column: as chosen, unless the time would not fit the width. */
export function narrowClockSize (clock: ClockShape): number {
	const room = THIRD_WIDTH - 2 * CLOCK_SIDE;
	return Math.min(clock.size, Math.floor(room / (clock.period ? 3.05 : 2.65)));
}

/** The height of the date's line under the time in a narrow column (see Clock.tsx). */
export function narrowDateLine (clock: ClockShape): number {
	return Math.round(32 * clock.dateSize / 100 * 1.2);
}

/** The clock card's height. Wide: the digits beside the date. Narrow: the date under the digits, on one line. */
export function clockHeight (clock: ClockShape, narrow: boolean): number {
	if (!narrow) return clock.size + 2 * CLOCK_PAD;
	return narrowClockSize(clock) + 2 * CLOCK_PAD + (clock.showDate ? narrowDateLine(clock) + 8 : 0);
}

/** The cards that share a column take its height: fixed ones first, the others share what is left. */
function stack (ids: CardId[], x: number, w: number, top: number, fixed: (id: CardId, alone: boolean) => number | null): BoardColumn {
	const alone = ids.length === 1;
	const heights = ids.map((id) => fixed(id, alone));
	const flexible = heights.filter((h) => h === null).length;
	const used = heights.reduce((sum: number, h) => sum + (h === null ? 0 : h), 0) + BOARD_GAP * (ids.length - 1);
	const share = flexible ? Math.max(0, Math.floor((BOARD_HEIGHT - used) / flexible)) : 0;
	let y = top;
	const cards = ids.map((id, i) => {
		const h = heights[i] === null ? share : heights[i] as number;
		const card = {id, x, y, w, h};
		y += h + BOARD_GAP;
		return card;
	});
	return {x, w, cards};
}

/** Where every card stands on the 1920 x 1080 screen; `top` is the block's top (boardTop). */
export function boardLayout (on: Cards, top: number): BoardLayout {
	const three = on.news && on.calendar;
	const left: CardId[] = on.weather ? ['clock', 'weather'] : ['clock'];
	const lists: CardId[][] = [left];
	if (three) lists.push(['calendar'], on.quote ? ['news', 'quote'] : ['news']);
	else {
		const right: CardId[] = [];
		if (on.calendar) right.push('calendar');
		if (on.news) right.push('news');
		if (on.quote) right.push('quote');
		if (right.length) lists.push(right);
	}
	const widths = three ? [THIRD_WIDTH, THIRD_WIDTH, THIRD_WIDTH] : [LEFT_WIDTH, RIGHT_WIDTH];
	const total = lists.reduce((sum, _, i) => sum + widths[i], 0) + BOARD_GAP * (lists.length - 1);
	let x = Math.round((LAYOUT.screenWidth - total) / 2);
	const fixed = (id: CardId, alone: boolean): number | null => {
		if (id === 'clock') return clockHeight(on.clock, three);
		if (id === 'quote') return alone ? BOARD_HEIGHT : three ? QUOTE_HEIGHT_THIRD : QUOTE_HEIGHT;
		return null;
	};
	const columns = lists.map((ids, i) => {
		const column = stack(ids, x, widths[i], top, fixed);
		x += widths[i] + BOARD_GAP;
		return column;
	});
	const cards: Partial<Record<CardId, BoardCard>> = {};
	columns.forEach((c) => c.cards.forEach((card) => { cards[card.id] = card; }));
	return {narrow: three, columns, cards};
}

/** The cards the remote can step onto (the clock and the quote are only read). */
export const FOCUSABLE: readonly CardId[] = ['weather', 'calendar', 'news'];

/** The column whose middle is closest to `x` among those with a card the remote can step onto (-1: none). */
export function columnAt (layout: BoardLayout, x: number): number {
	let best = -1, distance = Infinity;
	layout.columns.forEach((c, i) => {
		if (!c.cards.some((card) => FOCUSABLE.indexOf(card.id) !== -1)) return;
		const d = x < c.x ? c.x - x : x > c.x + c.w ? x - c.x - c.w : 0;
		if (d < distance) { distance = d; best = i; }
	});
	return best;
}

/** The card the remote steps onto from the app row in column `index`: the lowest one it can step onto. */
export function entryCard (layout: BoardLayout, index: number): CardId | null {
	const column = layout.columns[index];
	if (!column) return null;
	const cards = column.cards.filter((c) => FOCUSABLE.indexOf(c.id) !== -1);
	return cards.length ? cards[cards.length - 1].id : null;
}

/** The card above (-1) or below (+1) `id` in its column that the remote can step onto (null: none). */
export function nextInColumn (layout: BoardLayout, id: CardId, step: -1 | 1): CardId | null {
	for (const column of layout.columns) {
		const i = column.cards.findIndex((c) => c.id === id);
		if (i === -1) continue;
		for (let j = i + step; j >= 0 && j < column.cards.length; j += step) {
			if (FOCUSABLE.indexOf(column.cards[j].id) !== -1) return column.cards[j].id;
		}
		return null;
	}
	return null;
}
