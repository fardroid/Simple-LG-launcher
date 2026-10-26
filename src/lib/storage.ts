// The user's choices: which apps and inputs appear on the home row, in order, the look
// (theme, sizes, margins), the clock, the wallpaper, and a few options. Persisted in
// localStorage (private to this app on the TV).

import {STORAGE_KEY, WALLPAPER} from '../config/constants';
import type {ThemeName} from '../theme/tokens';
import {CLOCK_FONT_IDS, type ClockFont} from './clockfont';
import {CLOCK_COLOR_IDS, type ClockColor} from './clockcolor';
import {validCoords, type City, type TempUnit} from './weather';
import {validSource} from './calendar';
import type {PlateMode} from './plates';
import type {LangSetting} from './i18n';

export type WallpaperMode = 'gradient' | 'image' | 'folder';
export type ClockFormat = 'tv' | '12h' | '24h';
export type {ClockColor};
export type ClockWeight = 200 | 300 | 400;
export type {PlateMode};

/** Numeric settings: the values they can take (OK steps through them, ← → nudge them). */
export const RANGES = {
	iconScale:    {min: 50,  max: 140, step: 10, def: 100},   // % of the standard tile size
	sideMargin:   {min: 0,   max: 300, step: 10, def: 80},    // px at 1080p, left and right of the app row
	bottomMargin: {min: 0,   max: 200, step: 10, def: 50},    // px at 1080p, below the app row
	clockSize:    {min: 80,  max: 220, step: 10, def: 160},   // px, height of the digits
	clockOpacity: {min: 10,  max: 100, step: 10, def: 100},   // % (how solid the clock, date and weather are)
	// text sizes, % of the standard size (the standard is what looks right on a 43" TV)
	dateSize:     {min: 60,  max: 160, step: 5,  def: 100},
	cardSize:     {min: 60,  max: 160, step: 5,  def: 100}    // the weather and calendar cards (one size for both)
} as const;

export type RangeName = keyof typeof RANGES;

export function rangeValues (name: RangeName): number[] {
	const r = RANGES[name];
	const out: number[] = [];
	for (let v = r.min; v <= r.max; v += r.step) out.push(v);
	return out;
}

export interface ClockConfig {
	size: number;
	weight: ClockWeight;
	/** the typeface of the time and date */
	font: ClockFont;
	color: ClockColor;
	/** how solid the whole widget is, % */
	opacity: number;
	showDate: boolean;
	/** the size of the date under the time, % of the standard */
	dateSize: number;
	/** AM/PM beside the time (12-hour mode only) */
	showPeriod: boolean;
}

export interface WeatherConfig {
	/** show the weather line under the date */
	on: boolean;
	city: City | null;
	unit: TempUnit;
	showCity: boolean;
}

export interface CalendarConfig {
	/** show the calendar card */
	on: boolean;
	/** the calendars: web addresses (Google Calendar: Settings → the calendar → "Secret address in iCal format"),
	 *  files on the TV, or folders (ending with "/") whose .ics files are all used */
	sources: string[];
}

export interface WallpaperConfig {
	/** gradient: the default background (black or the calm gradient, per theme). image: one picture. folder: a slideshow. */
	mode: WallpaperMode;
	/** absolute path of the picture or the folder (empty for the default background) */
	path: string;
	/** seconds between pictures in a slideshow */
	intervalSec: number;
	/** play the slideshow in random order */
	shuffle: boolean;
}

export interface UserConfig {
	/** the language of the screens: 'auto' follows the TV's own language */
	language: LangSetting;
	/** the order the user arranged the apps in (apps not listed follow, in the TV's order); null → the TV's order */
	appIds: string[] | null;
	/** apps the user hid; every other app shows. null → settings from an older version, converted when the app list loads */
	hiddenIds: string[] | null;
	/** ordered input appIds (com.webos.app.hdmiN) shown after the apps; empty → none */
	sourceIds: string[];
	/** the order of the apps and inputs together on the home row (tile keys); null → apps, then inputs */
	tileOrder: string[] | null;
	/** read the live app list through the Homebrew Channel (root) */
	useHomebrew: boolean;
	/** end LG's own home app when the launcher opens, to free memory */
	closeHome: boolean;
	/** a watcher opens this launcher whenever LG Home comes to the front (kept running whenever the launcher opens) */
	replaceHome: boolean;
	/** start that watcher at boot (a link in the startup folder) */
	hookBoot: boolean;
	theme: ThemeName;
	iconScale: number;
	/** show each app's name under its icon on the home row */
	showLabels: boolean;
	/** apps in one row or two */
	rows: 1 | 2;
	sideMargin: number;
	bottomMargin: number;
	/** text size of the weather and calendar cards, % of the standard */
	cardSize: number;
	/** the plates behind the clock and the cards: 'auto' follows the brightness of the wallpaper behind each one */
	plates: PlateMode;
	/** frosted glass: the cards show a blurred copy of the wallpaper under their colour (only with a wallpaper picture) */
	glass: boolean;
	/** the soft shadow under the clock and the cards */
	shadows: boolean;
	/** 'tv' follows the TV's own 12/24-hour setting */
	clockFormat: ClockFormat;
	clock: ClockConfig;
	weather: WeatherConfig;
	calendar: CalendarConfig;
	wallpaper: WallpaperConfig;
}

export const DEFAULT_WALLPAPER: WallpaperConfig = {
	mode: 'gradient',
	path: '',
	intervalSec: 60,
	shuffle: true
};

export const DEFAULT_WEATHER: WeatherConfig = {
	on: false,
	city: null,
	unit: 'c',
	showCity: true
};

export const DEFAULT_CALENDAR: CalendarConfig = {
	on: false,
	sources: []
};

export const DEFAULT_CLOCK: ClockConfig = {
	size: RANGES.clockSize.def,
	weight: 200,
	font: 'inter',
	color: 'white',
	opacity: RANGES.clockOpacity.def,
	showDate: true,
	dateSize: RANGES.dateSize.def,
	showPeriod: true
};

export const DEFAULT_CONFIG: UserConfig = {
	language: 'auto',
	appIds: null,
	hiddenIds: [],
	sourceIds: [],
	tileOrder: null,
	useHomebrew: true,
	closeHome: true,
	replaceHome: false,
	hookBoot: true,
	theme: 'dusk',
	iconScale: RANGES.iconScale.def,
	showLabels: true,
	rows: 1,
	sideMargin: RANGES.sideMargin.def,
	bottomMargin: RANGES.bottomMargin.def,
	cardSize: RANGES.cardSize.def,
	plates: 'auto',
	glass: true,
	shadows: true,
	clockFormat: '24h',
	clock: DEFAULT_CLOCK,
	weather: DEFAULT_WEATHER,
	calendar: DEFAULT_CALENDAR,
	wallpaper: DEFAULT_WALLPAPER
};

function stringList (value: unknown): string[] | null {
	return Array.isArray(value) ? value.filter((x): x is string => typeof x === 'string') : null;
}

/** A number within the range's limits, snapped to its step; the default when it isn't a number. */
function ranged (value: unknown, name: RangeName): number {
	const r = RANGES[name];
	if (typeof value !== 'number' || !isFinite(value)) return r.def;
	const snapped = r.min + Math.round((Math.min(r.max, Math.max(r.min, value)) - r.min) / r.step) * r.step;
	return Math.min(r.max, snapped);
}

/** True when this copy of the launcher has settings of its own saved. */
export function hasStoredConfig (): boolean {
	try { return !!localStorage.getItem(STORAGE_KEY); } catch { return false; }
}

export function loadConfig (): UserConfig {
	let raw: string | null = null;
	try { raw = localStorage.getItem(STORAGE_KEY); } catch { /* none */ }
	return parseConfig(raw);
}

/** Settings from their saved text (checked value by value; the defaults for anything missing or wrong). */
export function parseConfig (raw: string | null): UserConfig {
	try {
		if (!raw) return {...DEFAULT_CONFIG};
		const parsed = JSON.parse(raw) as Partial<UserConfig> & {wallpaper?: Partial<WallpaperConfig>; clock?: Partial<ClockConfig>; weather?: Partial<WeatherConfig>; calendar?: Partial<CalendarConfig>};
		const w: Partial<WallpaperConfig> = parsed.wallpaper || {};
		const c: Partial<ClockConfig> = parsed.clock || {};
		const wx: Partial<WeatherConfig> = parsed.weather || {};
		const cal: Partial<CalendarConfig> = parsed.calendar || {};
		const legacy = (cal as {url?: unknown}).url;
		const calSources = (Array.isArray(cal.sources) ? cal.sources : typeof legacy === 'string' ? [legacy] : []).filter((x): x is string => typeof x === 'string' && validSource(x)).slice(0, 8);
		const wc = wx.city as Partial<City> | null | undefined;
		const city: City | null = wc && typeof wc.name === 'string' && wc.name && validCoords(wc.lat, wc.lon) ? {name: wc.name.slice(0, 60), lat: wc.lat as number, lon: wc.lon as number, detail: typeof wc.detail === 'string' ? wc.detail.slice(0, 80) : undefined} : null;
		const mode: WallpaperMode = w.mode === 'image' || w.mode === 'folder' ? w.mode : 'gradient';
		const path = typeof w.path === 'string' ? w.path : '';
		const interval = typeof w.intervalSec === 'number' && w.intervalSec >= 5 ? w.intervalSec : DEFAULT_WALLPAPER.intervalSec;
		return {
			language: parsed.language === 'en' || parsed.language === 'ru' ? parsed.language : 'auto',
			appIds: stringList(parsed.appIds),
			// older settings listed the apps to SHOW: they are converted once the app list is known
			hiddenIds: stringList(parsed.hiddenIds) !== null ? stringList(parsed.hiddenIds) : (stringList(parsed.appIds) === null ? [] : null),
			sourceIds: stringList(parsed.sourceIds) || [],
			tileOrder: stringList(parsed.tileOrder),
			useHomebrew: parsed.useHomebrew !== false,
			closeHome: parsed.closeHome !== false,
			replaceHome: parsed.replaceHome === true,
			hookBoot: parsed.hookBoot !== false,
			theme: parsed.theme === 'grey' || parsed.theme === 'black' ? parsed.theme : 'dusk',
			iconScale: ranged(parsed.iconScale, 'iconScale'),
			showLabels: parsed.showLabels !== false,
			rows: parsed.rows === 2 ? 2 : 1,
			sideMargin: ranged(parsed.sideMargin, 'sideMargin'),
			bottomMargin: ranged(parsed.bottomMargin, 'bottomMargin'),
			cardSize: ranged(parsed.cardSize, 'cardSize'),
			plates: parsed.plates === 'light' || parsed.plates === 'dark' ? parsed.plates : 'auto',
			glass: parsed.glass !== false,
			shadows: parsed.shadows !== false,
			clockFormat: parsed.clockFormat === 'tv' || parsed.clockFormat === '12h' ? parsed.clockFormat : '24h',
			clock: {
				size: ranged(c.size, 'clockSize'),
				weight: c.weight === 300 || c.weight === 400 ? c.weight : 200,
				font: CLOCK_FONT_IDS.indexOf(c.font as ClockFont) !== -1 ? (c.font as ClockFont) : 'inter',
				color: CLOCK_COLOR_IDS.indexOf(c.color as ClockColor) !== -1 ? (c.color as ClockColor) : 'white',
				opacity: ranged(c.opacity, 'clockOpacity'),
				showDate: c.showDate !== false,
				dateSize: ranged(c.dateSize, 'dateSize'),
				showPeriod: c.showPeriod !== false
			},
			weather: {
				on: wx.on === true && city !== null,
				city,
				unit: wx.unit === 'f' ? 'f' : 'c',
				showCity: wx.showCity !== false
			},
			calendar: {
				on: cal.on === true && calSources.length > 0,
				sources: calSources
			},
			wallpaper: {
				mode: path ? mode : 'gradient',
				path,
				intervalSec: interval,
				shuffle: w.shuffle !== false
			}
		};
	} catch {
		return {...DEFAULT_CONFIG};
	}
}

export function saveConfig (config: UserConfig): void {
	try { localStorage.setItem(STORAGE_KEY, JSON.stringify(config)); }
	catch { /* nothing sensible to do on a TV */ }
}

/** Toggle an ID in an ordered list: remove if present, else append. */
export function toggleId (ids: string[], id: string): string[] {
	return ids.indexOf(id) === -1 ? ids.concat(id) : ids.filter((x) => x !== id);
}

/** Step through a list of allowed values: `delta` is +1 or -1; `wrap` loops round at the ends
 *  (OK key) instead of stopping there (arrow keys). */
export function stepValue<T> (values: readonly T[], current: T, delta: number, wrap: boolean): T {
	const i = values.indexOf(current);
	const from = i === -1 ? 0 : i;
	const next = wrap ? (from + delta + values.length) % values.length : Math.min(values.length - 1, Math.max(0, from + delta));
	return values[next];
}

export const INTERVAL_VALUES: readonly number[] = WALLPAPER.intervals;
