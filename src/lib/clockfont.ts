// The typefaces the clock can use. All but "TV font" are bundled with the launcher (so they look the
// same on every TV and webOS version); "TV font" is whatever the TV's own web view uses for its UI.

import {tr} from './i18n';

export type ClockFont = 'inter' | 'tv' | 'poppins' | 'oswald' | 'mono' | 'serif';

export interface ClockFontInfo {
	id: ClockFont;
	/** the name in English and in Russian */
	label: [string, string];
	/** CSS font-family list, with fallbacks for while a font loads or if it is missing */
	family: string;
	/** where the tops of the digits are below the top of a line of this font (fraction of the font size, line-height 1): the date lines up with it */
	capTop: number;
}

export const CLOCK_FONTS: readonly ClockFontInfo[] = [
	{id: 'inter', label: ['Inter (default)', 'Inter (по умолчанию)'], family: '"Inter Variable", "LG Smart UI", "Helvetica Neue", Arial, sans-serif', capTop: 0.137},
	{id: 'tv', label: ['TV font', 'Шрифт ТВ'], family: '"LG Smart UI", "Helvetica Neue", Helvetica, Arial, sans-serif', capTop: 0.14},
	{id: 'poppins', label: ['Poppins (rounded)', 'Poppins (округлый)'], family: '"Poppins", "Helvetica Neue", Arial, sans-serif', capTop: 0.15},
	{id: 'oswald', label: ['Oswald (narrow)', 'Oswald (узкий)'], family: '"Oswald", "Arial Narrow", Arial, sans-serif', capTop: 0.14},
	{id: 'mono', label: ['Roboto Mono (digital)', 'Roboto Mono (цифровой)'], family: '"Roboto Mono", "Courier New", monospace', capTop: 0.178},
	{id: 'serif', label: ['Cormorant (elegant serif)', 'Cormorant (изящный с засечками)'], family: '"Cormorant Garamond", Georgia, "Times New Roman", serif', capTop: 0.19}
];

export const CLOCK_FONT_IDS: readonly ClockFont[] = CLOCK_FONTS.map((f) => f.id);

export function fontFamily (id: ClockFont): string {
	for (const f of CLOCK_FONTS) if (f.id === id) return f.family;
	return CLOCK_FONTS[0].family;
}

/** The top of the digits, in px, for a clock of this size. */
export function digitsTop (id: ClockFont, size: number): number {
	for (const f of CLOCK_FONTS) if (f.id === id) return Math.round(size * f.capTop);
	return Math.round(size * CLOCK_FONTS[0].capTop);
}

export function fontLabel (id: ClockFont): string {
	let label = CLOCK_FONTS[0].label;
	for (const f of CLOCK_FONTS) if (f.id === id) label = f.label;
	return tr(label[0], label[1]);
}
