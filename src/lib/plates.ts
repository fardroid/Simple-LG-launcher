// The plates behind the clock and the cards (the weather, the calendar, the news, the quote). In "auto" they take their colour from the brightness of the
// wallpaper behind them: dark plates on a light picture, light ones on a dark picture, so they show against any background.
// All of them get the same colour, chosen for the brightest place any of them covers (white text has to read on every one);
// a light plate next to a dark one looks like a mistake. The wallpaper is measured once, when it appears, as a coarse grid of
// brightness values (16 x 9 cells).

import {THEMES, type ThemeName} from '../theme/tokens';
import type {BoardCard, BoardLayout, CardId} from './boardlayout';

export type PlateMode = 'auto' | 'light' | 'dark';

export const MAP_W = 16;
export const MAP_H = 9;
const SCREEN_W = 1920;
const SCREEN_H = 1080;

export interface PlateLook {
	/** the plate's colour, and the (stronger) colour while the remote is on it */
	bg: string;
	focus: string;
	/** frosted glass: a blurred copy of the wallpaper (a data URL, 16:9) shown under the colour, and where the plate is on the
	 *  screen so the copy lines up with the picture behind it. Both are left out without a picture or with the glass off. */
	frost?: string;
	x?: number;
	y?: number;
}

/** The plate of each card on the screen. */
export type PlateLooks = Partial<Record<CardId, PlateLook>>;

/** A card's place on the 1920 x 1080 screen: x, y, width, height. */
export type Rect = [number, number, number, number];

/** The cards' places as plates.ts takes them (from boardLayout). */
export function plateRects (layout: BoardLayout): Partial<Record<CardId, Rect>> {
	const out: Partial<Record<CardId, Rect>> = {};
	(Object.keys(layout.cards) as CardId[]).forEach((id) => {
		const c = layout.cards[id] as BoardCard;
		out[id] = [c.x, c.y, c.w, c.h];
	});
	return out;
}

/** Average brightness (0 to 1) of the map under a rectangle of the screen. */
export function regionBrightness (map: number[], rect: Rect): number {
	const x0 = Math.max(0, Math.floor(rect[0] / SCREEN_W * MAP_W));
	const x1 = Math.min(MAP_W, Math.max(x0 + 1, Math.ceil((rect[0] + rect[2]) / SCREEN_W * MAP_W)));
	const y0 = Math.max(0, Math.floor(rect[1] / SCREEN_H * MAP_H));
	const y1 = Math.min(MAP_H, Math.max(y0 + 1, Math.ceil((rect[1] + rect[3]) / SCREEN_H * MAP_H)));
	let sum = 0, n = 0;
	for (let y = y0; y < y1; y++) for (let x = x0; x < x1; x++) { sum += map[y * MAP_W + x] || 0; n++; }
	return n ? sum / n : 0;
}

/** Brightness grid of an image drawn at 64 x 36 (RGBA bytes): the mean of 4 x 4 blocks. */
export function mapFromPixels (data: ArrayLike<number>): number[] {
	const out: number[] = [];
	for (let my = 0; my < MAP_H; my++) {
		for (let mx = 0; mx < MAP_W; mx++) {
			let sum = 0;
			for (let y = 0; y < 4; y++) {
				for (let x = 0; x < 4; x++) {
					const i = ((my * 4 + y) * MAP_W * 4 + mx * 4 + x) * 4;
					sum += (0.2126 * data[i] + 0.7152 * data[i + 1] + 0.0722 * data[i + 2]) / 255;
				}
			}
			out.push(Math.round(sum / 16 * 1000) / 1000);
		}
	}
	return out;
}

function hexBrightness (hex: string): number {
	const n = parseInt(hex.slice(1), 16);
	return (0.2126 * ((n >> 16) & 255) + 0.7152 * ((n >> 8) & 255) + 0.0722 * (n & 255)) / 255;
}

/** The brightness grid of a theme's own background (the 155 degree gradient of .wallpaper in global.css). */
export function themeMap (theme: ThemeName): number[] {
	const c = THEMES[theme];
	const stops = [[0, hexBrightness(c.wallpaperStart)], [0.55, hexBrightness(c.wallpaperMid)], [1, hexBrightness(c.wallpaperEnd)]];
	const angle = 155 * Math.PI / 180;
	const dx = Math.sin(angle), dy = -Math.cos(angle);
	const length = Math.abs(SCREEN_W * dx) + Math.abs(SCREEN_H * dy);
	const out: number[] = [];
	for (let my = 0; my < MAP_H; my++) {
		for (let mx = 0; mx < MAP_W; mx++) {
			const x = (mx + 0.5) / MAP_W * SCREEN_W - SCREEN_W / 2, y = (my + 0.5) / MAP_H * SCREEN_H - SCREEN_H / 2;
			const t = Math.min(1, Math.max(0, (x * dx + y * dy) / length + 0.5));
			const i = t < stops[1][0] ? 0 : 1;
			const k = (t - stops[i][0]) / (stops[i + 1][0] - stops[i][0]);
			out.push(stops[i][1] + (stops[i + 1][1] - stops[i][1]) * k);
		}
	}
	return out;
}

/* `focus` is laid over the plate (not instead of it), so it is a small step */
const LIGHT: PlateLook = {bg: 'rgba(255, 255, 255, 0.11)', focus: 'rgba(255, 255, 255, 0.06)'};

/** The plate's colour for a background of this brightness: light under dark, dark under light, and in between a
 *  dark one that grows stronger the brighter it gets, so there is never a point where the plate vanishes. */
export function lookFor (brightness: number): PlateLook {
	if (brightness <= 0.34) return LIGHT;
	const alpha = Math.min(0.66, 0.3 + (brightness - 0.34) * 0.9);
	return {bg: `rgba(14, 18, 40, ${alpha.toFixed(2)})`, focus: `rgba(14, 18, 40, ${Math.min(0.3, 0.05 + alpha * 0.25).toFixed(2)})`};
}

/** The look of the plates. `rects` are the cards on the screen (plateRects); `map` is the wallpaper's brightness grid (null: the
 *  theme's own background); `frost` the blurred copy of the wallpaper for the glass (null: no glass). */
export function plateLooks (mode: PlateMode, theme: ThemeName, map: number[] | null, rects: Partial<Record<CardId, Rect>>, frost: string | null = null): PlateLooks {
	const ids = (Object.keys(rects) as CardId[]).filter((id) => (rects[id] as Rect)[3] > 0);
	const glass = (look: PlateLook, rect: Rect): PlateLook => frost ? {bg: look.bg, focus: look.focus, frost, x: rect[0], y: rect[1]} : look;
	let look: PlateLook;
	if (mode === 'light') look = LIGHT;
	else if (mode === 'dark') look = {bg: 'rgba(12, 18, 46, 0.55)', focus: 'rgba(14, 22, 54, 0.18)'};
	else {
		const m = map || themeMap(theme);
		look = ids.length ? lookFor(Math.max(...ids.map((id) => regionBrightness(m, rects[id] as Rect)))) : LIGHT;
	}
	const out: PlateLooks = {};
	ids.forEach((id) => { out[id] = glass(look, rects[id] as Rect); });
	return out;
}

/** The CSS variables a plate sets on its card (see .card in global.css); the glass picture itself is shown by <Frost>. */
export function plateVars (plate: PlateLook | undefined): Record<string, string> {
	if (!plate) return {};
	const vars: Record<string, string> = {'--pb': plate.bg, '--pf': plate.focus};
	if (plate.frost) {
		vars['--fx'] = `${plate.x || 0}px`;
		vars['--fy'] = `${plate.y || 0}px`;
	}
	return vars;
}
