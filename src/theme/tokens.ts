// Design tokens: the single source of truth for colour, spacing, type and motion.
// `applyTokens()` exposes them as CSS custom properties so the stylesheet and the
// components read the same values.

export type ThemeName = 'black' | 'grey' | 'dusk';

export interface Colors {
	bg: string; wallpaperStart: string; wallpaperMid: string; wallpaperEnd: string;
	textPrimary: string; textSecondary: string; textMuted: string;
	tileBg: string; tileBgFocus: string; tileBorder: string; tileBorderFocus: string;
	badge: string; badgeText: string; liftShadow: string;
	buttonBorder: string; buttonActiveBg: string; buttonActiveText: string;
	switchTrack: string; switchTrackOn: string; switchKnob: string; switchKnobOn: string;
	danger: string;
	/** dim layer behind Settings, so a wallpaper doesn't fight the text */
	overlay: string;
	pickerBg: string;
}

/** OLED black: pure #000 so the pixels are off, with neutral greys on top. */
const BLACK: Colors = {
	bg: '#000000',
	wallpaperStart: '#000000',
	wallpaperMid: '#000000',
	wallpaperEnd: '#000000',
	textPrimary: '#F2F2F2',
	textSecondary: '#A8A8A8',
	textMuted: '#6A6A6A',
	tileBg: '#101010',
	tileBgFocus: '#1D1D1D',
	tileBorder: '#2A2A2A',
	tileBorderFocus: '#F2F2F2',
	badge: '#F2F2F2',
	badgeText: '#000000',
	liftShadow: 'rgba(0, 0, 0, 0.8)',
	buttonBorder: '#3A3A3A',
	buttonActiveBg: '#F2F2F2',
	buttonActiveText: '#000000',
	switchTrack: '#333333',
	switchTrackOn: '#F2F2F2',
	switchKnob: '#A8A8A8',
	switchKnobOn: '#000000',
	danger: '#EF7A73',
	overlay: 'rgba(0, 0, 0, 0.88)',
	pickerBg: '#000000'
};

/** The calm blue-grey look. */
const GREY: Colors = {
	bg: '#101a24',
	wallpaperStart: '#1c2b3a',
	wallpaperMid: '#25384c',
	wallpaperEnd: '#101a24',
	textPrimary: '#F2F5F8',
	textSecondary: '#B9C6D3',
	textMuted: '#7F90A1',
	tileBg: '#1a2733',
	tileBgFocus: '#243546',
	tileBorder: '#34485c',
	tileBorderFocus: '#F2F5F8',
	badge: '#F2F5F8',
	badgeText: '#101a24',
	liftShadow: 'rgba(0, 0, 0, 0.6)',
	buttonBorder: '#43586d',
	buttonActiveBg: '#F2F5F8',
	buttonActiveText: '#101a24',
	switchTrack: '#34485c',
	switchTrackOn: '#F2F5F8',
	switchKnob: '#B9C6D3',
	switchKnobOn: '#101a24',
	danger: '#EF7A73',
	overlay: 'rgba(10, 17, 24, 0.82)',
	pickerBg: '#0a1118'
};

/** Evening sky: deep blue at the top, warm at the horizon. */
const DUSK: Colors = {
	...GREY,
	bg: '#0a0d1f',
	wallpaperStart: '#0b1636',
	wallpaperMid: '#26357a',
	wallpaperEnd: '#a2506f',
	tileBg: '#1b2142',
	tileBgFocus: '#2a3260',
	tileBorder: '#3a4478',
	overlay: 'rgba(8, 11, 30, 0.86)',
	pickerBg: '#080b1e'
};

export const THEMES: Record<ThemeName, Colors> = {black: BLACK, grey: GREY, dusk: DUSK};

export const FONT = {
	family: '"Inter Variable", "LG Smart UI", "Helvetica Neue", Arial, sans-serif',
	rowLabel: 20,
	tileLabel: 25,
	settingsTitle: 40,
	settingsTab: 26,
	settingsButton: 22,
	optionLabel: 26,
	optionDescription: 20,
	toast: 24
} as const;

export const LAYOUT = {
	screenWidth: 1920,
	screenHeight: 1080,
	edgePadding: 140,       // padding of the Settings screens (the home row has its own, adjustable margins)
	headerTop: 84,
	tileSize: 150,          // app / source tiles are square at 100% icon size
	tileGap: 16,
	tileRadius: 24,
	rowLabelGap: 14,        // gap between a row's subheader and its tiles
	rowGap: 28,             // vertical space between rows
	gridColumns: 6          // the grid of apps and inputs in Settings
} as const;

/** Magnification of the home row's selected tile and of the tiles 1 and 2 places away (like a dock). */
export const DOCK: readonly number[] = [1.42, 1.2, 1.07];

export const MOTION = {
	focusMs: 160,
	scrollMs: 260,
	focusScale: 1.12,       // Settings grids (the home row uses DOCK)
	liftScale: 1.2,         // a tile picked up in re-order mode
	unfocusedRowOpacity: 0.55
} as const;

/** Writes every token onto :root as --color-*, --font-*, --layout-*, --motion-*. */
export function applyTokens (theme: ThemeName = 'black', root: HTMLElement = document.documentElement): void {
	const groups: Record<string, object> = {color: THEMES[theme], font: FONT, layout: LAYOUT, motion: MOTION};
	for (const group in groups) {
		const values = groups[group] as Record<string, string | number>;
		for (const key in values) {
			const value = values[key];
			const unit = typeof value === 'number' && !/scale|opacity|columns/i.test(key) ? (key.endsWith('Ms') ? 'ms' : 'px') : '';
			root.style.setProperty(`--${group}-${kebab(key)}`, `${value}${unit}`);
		}
	}
}

/** Switch the colour theme at runtime. */
export function applyTheme (theme: ThemeName, root: HTMLElement = document.documentElement): void {
	const colors = THEMES[theme] as unknown as Record<string, string>;
	for (const key in colors) root.style.setProperty(`--color-${kebab(key)}`, colors[key]);
}

function kebab (s: string): string {
	return s.replace(/([A-Z])/g, '-$1').toLowerCase();
}
