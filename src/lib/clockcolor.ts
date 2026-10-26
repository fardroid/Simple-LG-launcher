// The colours the clock, date and weather can have (Settings → Wallpaper → Widget colour). The light ones suit dark
// backgrounds; the dark ones and the gradients stay readable on light pictures.

import {tr} from './i18n';

export type ClockColor = 'white' | 'warm' | 'blue' | 'amber' | 'grey' | 'charcoal' | 'black' | 'ember' | 'ocean' | 'dusk';

export interface ClockColorInfo {
	id: ClockColor;
	/** the name in English and in Russian */
	label: [string, string];
	/** a plain colour, or the gradient (left to right) that fills the text */
	color?: string;
	gradient?: string;
	/** the colour of the small weather picture (it cannot be filled with a gradient) */
	accent: string;
	/** a dark colour: the soft shadow behind it is light instead of dark */
	dark?: boolean;
}

export const CLOCK_COLORS: readonly ClockColorInfo[] = [
	{id: 'white', label: ['White', 'Белый'], color: '#F2F2F2', accent: '#F2F2F2'},
	{id: 'warm', label: ['Warm white', 'Тёплый белый'], color: '#FFE9C7', accent: '#FFE9C7'},
	{id: 'blue', label: ['Ice blue', 'Ледяной синий'], color: '#C4E2FF', accent: '#C4E2FF'},
	{id: 'amber', label: ['Amber', 'Янтарный'], color: '#FFB74D', accent: '#FFB74D'},
	{id: 'grey', label: ['Grey', 'Серый'], color: '#9AA0A6', accent: '#9AA0A6'},
	{id: 'charcoal', label: ['Dark grey', 'Тёмно-серый'], color: '#3C4043', accent: '#3C4043', dark: true},
	{id: 'black', label: ['Black', 'Чёрный'], color: '#0A0A0A', accent: '#0A0A0A', dark: true},
	{id: 'ember', label: ['Gradient: Ember (red to gold)', 'Градиент: Ember (красный в золото)'], gradient: 'linear-gradient(90deg, #D62839 0%, #F08C1A 100%)', accent: '#F0803F'},
	{id: 'ocean', label: ['Gradient: Ocean (blue to teal)', 'Градиент: Ocean (синий в бирюзовый)'], gradient: 'linear-gradient(90deg, #1E4FD8 0%, #19C6D8 100%)', accent: '#1F8BDA', dark: true},
	{id: 'dusk', label: ['Gradient: Dusk (violet to coral)', 'Градиент: Dusk (фиолетовый в коралловый)'], gradient: 'linear-gradient(90deg, #6A2FC9 0%, #F2634F 100%)', accent: '#A04A9A', dark: true}
];

export const CLOCK_COLOR_IDS: readonly ClockColor[] = CLOCK_COLORS.map((c) => c.id);

export function colorInfo (id: ClockColor): ClockColorInfo {
	for (const c of CLOCK_COLORS) if (c.id === id) return c;
	return CLOCK_COLORS[0];
}

export function colorLabel (id: ClockColor): string {
	const label = colorInfo(id).label;
	return tr(label[0], label[1]);
}
