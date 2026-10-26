// The settings screens as data: every option on the Look, Clock, Wallpaper and Options tabs,
// how it reads the config, and how OK / ← → change it. The app builds the Settings lists
// from this table and applies a change with `applyPref`.

import type {OptionItem} from '../components/OptionList';
import type {ThemeName} from '../theme/tokens';
import {
	DEFAULT_CLOCK, INTERVAL_VALUES, RANGES, rangeValues, stepValue,
	type ClockFormat, type ClockWeight, type RangeName, type UserConfig
} from './storage';
import {CLOCK_FONT_IDS, fontLabel} from './clockfont';
import {CLOCK_COLOR_IDS, colorLabel, type ClockColor} from './clockcolor';
import {baseName} from './wallpaper';
import type {PlateMode} from './plates';
import {WEATHER_ENABLED} from '../config/features';
import type {TempUnit} from './weather';
import {lastFailure, type RootState} from './failure';
import type {LogoState} from './system';
import {count, lang, langName, LANG_SETTINGS, tr, type Lang} from './i18n';

export type PrefTab = 'look' | 'clock' | 'weather' | 'calendar' | 'wallpaper' | 'options';

/** Facts about the TV (not user settings) that some rows display. */
export interface SystemInfo {
	/** the app the TV's Home button opens (null while unknown) */
	homeApp: string | null;
	/** the hand-over watcher: running, and whether it starts at boot (null while unknown) */
	homeHook: {running: boolean; linked: boolean} | null;
	/** the launcher is in the TV's input list (null while unknown) */
	tvInput?: boolean | null;
	/** the TV's "show the LG logo at start" setting (null while unknown) */
	logo?: LogoState | null;
	/** can this TV run commands through the Homebrew Channel (rooted)? */
	root?: RootState;
	/** what the TV said when it could not (shown under a greyed-out row) */
	rootDetail?: string;
}

/** The options that run shell commands, so they can't work without root. */
export const ROOT_OPTIONS = ['closeHome', 'useHomebrew', 'useAsHome', 'replaceHome', 'startAtBoot', 'tvInput', 'closeHomeNow', 'lgLogo'];

/** Why a row is greyed out, or null when it is usable. */
export function disabledReason (id: string, info?: SystemInfo): string | null {
	if (id === 'lgLogo' && info && info.logo === 'unsupported') return tr('This TV has no setting for the LG logo at start.', 'На этом ТВ нет настройки логотипа LG при запуске.');
	if (!info || info.root !== 'no' || ROOT_OPTIONS.indexOf(id) === -1) return null;
	const kind = lastFailure();
	const why = kind === 'denied' ? tr('the TV refused the command.', 'ТВ отклонил команду.') : tr('the Homebrew Channel service is not running. If the TV is rooted, open the Homebrew Channel once, then open Settings again.', 'служба Homebrew Channel не запущена. Если ТВ рутован, один раз откройте Homebrew Channel, затем заново откройте настройки.');
	return tr('Needs a rooted TV: ', 'Нужен рутованный ТВ: ') + why + (info.rootDetail ? tr(' (the TV said: ', ' (ответ ТВ: ') + info.rootDetail + ')' : '');
}

/** How a key press changes a setting: OK steps and wraps round; ← → nudge and stop at the ends. */
export type Press = 'ok' | 'left' | 'right';

export interface Pref {
	id: string;
	tab: PrefTab;
	label: string;
	description: string;
	kind: 'toggle' | 'choice' | 'action';
	/** toggle: is it on */
	on?: (c: UserConfig) => boolean;
	/** choice / action: the current value as text */
	text?: (c: UserConfig, info?: SystemInfo) => string;
	/** the new config after a key press (toggle: any press flips it) */
	change?: (c: UserConfig, press: Press) => UserConfig;
}

const delta = (press: Press): number => (press === 'left' ? -1 : 1);

/** "On", "Off", or "Unknown" while it is not known. */
const onOff = (on: boolean | null): string => on === null ? tr('Unknown', 'Неизвестно') : on ? tr('On', 'Вкл.') : tr('Off', 'Выкл.');

function toggle (id: string, tab: PrefTab, label: string, description: string, get: (c: UserConfig) => boolean, set: (c: UserConfig, v: boolean) => UserConfig): Pref {
	return {
		id, tab, label, description, kind: 'toggle',
		on: get,
		change: (c, press) => (press === 'ok' ? set(c, !get(c)) : c)       // arrows leave switches alone
	};
}

function choice<T> (id: string, tab: PrefTab, label: string, description: string, values: readonly T[], get: (c: UserConfig) => T, set: (c: UserConfig, v: T) => UserConfig, text: (v: T) => string): Pref {
	return {
		id, tab, label, description, kind: 'choice',
		text: (c) => text(get(c)),
		change: (c, press) => set(c, stepValue(values, get(c), delta(press), press === 'ok'))
	};
}

function range (id: string, tab: PrefTab, label: string, description: string, name: RangeName, get: (c: UserConfig) => number, set: (c: UserConfig, v: number) => UserConfig, text: (v: number) => string): Pref {
	return choice(id, tab, label, description, rangeValues(name), get, set, text);
}

export function intervalText (seconds: number): string {
	if (seconds < 60) return count(seconds, ['second', 'seconds'], ['секунда', 'секунды', 'секунд']);
	if (seconds < 3600) return count(seconds / 60, ['minute', 'minutes'], ['минута', 'минуты', 'минут']);
	return count(seconds / 3600, ['hour', 'hours'], ['час', 'часа', 'часов']);
}

const PLATE_MODES: readonly PlateMode[] = ['auto', 'light', 'dark'];
const THEMES: readonly ThemeName[] = ['dusk', 'black', 'grey'];
const CLOCK_FORMATS: readonly ClockFormat[] = ['24h', '12h', 'tv'];
const WEIGHTS: readonly ClockWeight[] = [200, 300, 400];
const UNITS: readonly TempUnit[] = ['c', 'f'];

const plateText = (v: PlateMode): string => v === 'light' ? tr('Light', 'Светлые') : v === 'dark' ? tr('Dark', 'Тёмные') : tr('Auto', 'Авто');
const themeText = (v: ThemeName): string => v === 'black' ? tr('Black (OLED)', 'Чёрный (OLED)') : v === 'grey' ? tr('Calm blue-grey', 'Спокойный серо-голубой') : tr('Evening sky', 'Вечернее небо');
const formatText = (v: ClockFormat): string => v === '12h' ? tr('12-hour (AM/PM)', '12-часовой (AM/PM)') : v === '24h' ? tr('24-hour', '24-часовой') : tr('Same as the TV', 'Как на ТВ');
const weightText = (v: ClockWeight): string => v === 300 ? tr('Light', 'Лёгкий') : v === 400 ? tr('Regular', 'Обычный') : tr('Thin', 'Тонкий');

/** The look, margins and clock back to their defaults (apps, sources and wallpaper are left alone). */
export function resetLook (c: UserConfig): UserConfig {
	return {
		...c,
		theme: 'dusk',
		iconScale: RANGES.iconScale.def,
		showLabels: true,
		sideMargin: RANGES.sideMargin.def,
		bottomMargin: RANGES.bottomMargin.def,
		cardSize: RANGES.cardSize.def,
		plates: 'auto',
		glass: true,
		shadows: true,
		clockFormat: '24h',
		clock: {...DEFAULT_CLOCK}
	};
}

/** The Weather tab's rows (left out of builds made without the weather widget). */
const weatherPrefs = (): Pref[] => [
	toggle('weatherOn', 'weather', tr('Show the weather', 'Показывать погоду'), tr('The temperature and the forecast, on their own card.', 'Температура и погода под датой.'),
		(c) => c.weather.on, (c, v) => ({...c, weather: {...c.weather, on: v && c.weather.city !== null}})),
	{
		id: 'weatherCity', tab: 'weather', label: tr('City', 'Город'), kind: 'action',
		description: tr('Pick a city from the list, or search for any place by name. Needs the internet.', 'Выберите город из списка или найдите любое место по названию. Нужен интернет.'),
		text: (c) => (c.weather.city ? c.weather.city.name : tr('Not chosen', 'Не выбран'))
	},
	choice('weatherUnit', 'weather', tr('Temperature unit', 'Единицы температуры'), tr('Degrees Celsius or Fahrenheit.', 'Градусы Цельсия или Фаренгейта.'),
		UNITS, (c) => c.weather.unit, (c, v) => ({...c, weather: {...c.weather, unit: v}}), (v) => (v === 'f' ? tr('Fahrenheit (°F)', 'Фаренгейт (°F)') : tr('Celsius (°C)', 'Цельсий (°C)'))),
	toggle('weatherCityName', 'weather', tr('Show the city', 'Показывать город'), tr('The city name before the temperature.', 'Название города перед температурой.'),
		(c) => c.weather.showCity, (c, v) => ({...c, weather: {...c.weather, showCity: v}}))
];

/** The Calendar tab's rows. */
const calendarPrefs = (): Pref[] => [
	toggle('calendarOn', 'calendar', tr('Show the calendar', 'Показывать календарь'), tr('The day\'s events in the top right of the home screen.', 'События дня в правом верхнем углу главного экрана.'),
		(c) => c.calendar.on, (c, v) => ({...c, calendar: {...c.calendar, on: v && c.calendar.sources.length > 0}})),
	{
		id: 'calendarUrl', tab: 'calendar', label: tr('Calendars', 'Календари'), kind: 'action',
		description: tr('iCal links from Google Calendar, .ics files or a whole folder. Several calendars can be shown at once.', 'Ссылки в формате iCal из Google Календаря, файлы .ics или целая папка. Можно подключить несколько календарей сразу.'),
		text: (c) => (c.calendar.sources.length ? count(c.calendar.sources.length, ['calendar', 'calendars'], ['календарь', 'календаря', 'календарей']) : tr('None', 'Не задан'))
	}
];

const basePrefs = (): Pref[] => [
	// ----- Look -----
	choice('language', 'look', 'Language · Язык', tr('The language of the launcher\'s screens. "Same as the TV" follows the TV\'s own language.', 'Язык экранов лаунчера. «Как на ТВ» следует языку самого телевизора.'),
		LANG_SETTINGS, (c) => c.language, (c, v) => ({...c, language: v}), langName),
	choice('theme', 'look', tr('Theme', 'Тема'), tr('OLED black leaves unused pixels off. Blue-grey is softer.', 'Чёрный OLED не зажигает неиспользуемые пиксели. Серо-голубой мягче.'),
		THEMES, (c) => c.theme, (c, v) => ({...c, theme: v}), themeText),
	range('iconScale', 'look', tr('Icon size', 'Размер значков'), tr('Make the app icons smaller or larger. ← → change it.', 'Сделайте значки приложений меньше или больше. ← → меняют.'),
		'iconScale', (c) => c.iconScale, (c, v) => ({...c, iconScale: v}), (v) => `${v}%`),
	range('cardSize', 'look', tr('Card text size', 'Размер текста карточек'), tr('The size of all the text on the weather and calendar cards. ← → change it.', 'Размер всего текста на карточках погоды и календаря. ← → меняют.'),
		'cardSize', (c) => c.cardSize, (c, v) => ({...c, cardSize: v}), (v) => `${v}%`),
	choice('plates', 'look', tr('Card backgrounds', 'Подложки карточек'), tr('Auto: each card picks its shade from the wallpaper behind it (light on dark, dark on light). Or always light, or always dark.', 'Авто: цвет подложки подбирается по яркости обоев за каждой карточкой (светлая на тёмном, тёмная на светлом). Или всегда светлые, или всегда тёмные.'),
		PLATE_MODES, (c) => c.plates, (c, v) => ({...c, plates: v}), plateText),
	toggle('glass', 'look', tr('Frosted glass', 'Матовое стекло'), tr('The cards blur the wallpaper beneath them, like frosted glass. Works with a picture as the wallpaper.', 'Подложки размывают обои под собой, как матовое стекло. Работает, когда выбрана картинка в качестве обоев.'),
		(c) => c.glass, (c, v) => ({...c, glass: v})),
	toggle('shadows', 'look', tr('Card shadows', 'Тень карточек'), tr('A soft shadow under the clock, weather and calendar. Without it the TV has a little less to draw.', 'Мягкая тень под часами, погодой и календарём. Без неё ТВ рисует экран немного легче.'),
		(c) => c.shadows, (c, v) => ({...c, shadows: v})),
	toggle('showLabels', 'look', tr('App names', 'Названия приложений'), tr('A name under each icon. Turn it off to keep only the icons.', 'Подпись под каждым значком. Выключите, чтобы остались только значки.'),
		(c) => c.showLabels, (c, v) => ({...c, showLabels: v})),
	range('sideMargin', 'look', tr('Side margin', 'Боковые поля'), tr('Space left and right of the app row. ← → change it.', 'Отступ слева и справа от ряда приложений. ← → меняют.'),
		'sideMargin', (c) => c.sideMargin, (c, v) => ({...c, sideMargin: v}), (v) => `${v} px`),
	range('bottomMargin', 'look', tr('Bottom margin', 'Нижнее поле'), tr('Space below the app row. ← → change it.', 'Отступ под рядом приложений. ← → меняют.'),
		'bottomMargin', (c) => c.bottomMargin, (c, v) => ({...c, bottomMargin: v}), (v) => `${v} px`),

	{
		id: 'resetLook', tab: 'look', label: tr('Reset look and clock', 'Сбросить вид и часы'), kind: 'action',
		description: tr('The theme, icon size, margins and clock go back to their defaults.', 'Тема, размер значков, поля и часы вернутся к значениям по умолчанию.'),
		text: () => tr('Reset', 'Сбросить')
	},

	// ----- Clock -----
	choice('clockFormat', 'clock', tr('Time format', 'Формат времени'), tr('12-hour, 24-hour or the same as the TV.', '12-часовой, 24-часовой или как на ТВ.'),
		CLOCK_FORMATS, (c) => c.clockFormat, (c, v) => ({...c, clockFormat: v}), formatText),
	range('clockSize', 'clock', tr('Clock size', 'Размер часов'), tr('How big the time is. ← → change it.', 'Насколько крупное время. ← → меняют.'),
		'clockSize', (c) => c.clock.size, (c, v) => ({...c, clock: {...c.clock, size: v}}), (v) => `${v}`),
	choice('clockFont', 'clock', tr('Clock font', 'Шрифт часов'), tr('The typeface of the time and date. "TV font" is built into the TV; the others come with the launcher.', 'Шрифт времени и даты. «Шрифт ТВ» встроен в телевизор, остальные идут с лаунчером.'),
		CLOCK_FONT_IDS, (c) => c.clock.font, (c, v) => ({...c, clock: {...c.clock, font: v}}), (v) => fontLabel(v)),
	choice('clockWeight', 'clock', tr('Clock weight', 'Толщина часов'), tr('Thin, light or regular digits.', 'Тонкие, лёгкие или обычные цифры.'),
		WEIGHTS, (c) => c.clock.weight, (c, v) => ({...c, clock: {...c.clock, weight: v}}), weightText),
	toggle('showDate', 'clock', tr('Show the date', 'Показывать дату'), tr('The weekday and the date beside the time.', 'День недели и дата рядом со временем.'),
		(c) => c.clock.showDate, (c, v) => ({...c, clock: {...c.clock, showDate: v}})),
	range('dateSize', 'clock', tr('Date text size', 'Размер текста даты'), tr('The size of the weekday and date beside the time. ← → change it.', 'Размер дня недели и даты рядом со временем. ← → меняют.'),
		'dateSize', (c) => c.clock.dateSize, (c, v) => ({...c, clock: {...c.clock, dateSize: v}}), (v) => `${v}%`),
	choice('clockColor', 'clock', tr('Clock colour', 'Цвет часов'), tr('The colour of the time and date. White is hard to read on a light picture: for light wallpapers pick a dark colour or a gradient.', 'Цвет времени и даты. Белый плохо читается на светлой картинке: для светлых обоев выберите тёмный цвет или градиент.'),
		CLOCK_COLOR_IDS, (c) => c.clock.color, (c, v) => ({...c, clock: {...c.clock, color: v}}), (v: ClockColor) => colorLabel(v)),

	// ----- Wallpaper -----
	{
		id: 'wallpaper', tab: 'wallpaper', label: tr('Wallpaper', 'Обои'), kind: 'action',
		description: tr('A picture, or a folder of pictures for a slideshow, from a USB drive or the TV.', 'Картинка или папка с картинками для слайд-шоу с USB-накопителя или с ТВ.'),
		text: (c) => c.wallpaper.mode === 'image' ? tr('Picture: ', 'Картинка: ') + baseName(c.wallpaper.path)
			: c.wallpaper.mode === 'folder' ? tr('Slideshow: ', 'Слайд-шоу: ') + baseName(c.wallpaper.path) : tr('Default background', 'Фон по умолчанию')
	},
	choice('interval', 'wallpaper', tr('Change the picture every', 'Менять картинку каждые'), tr('How long each picture stays on screen in a slideshow.', 'Сколько каждая картинка остаётся на экране в слайд-шоу.'),
		INTERVAL_VALUES, (c) => c.wallpaper.intervalSec, (c, v) => ({...c, wallpaper: {...c.wallpaper, intervalSec: v}}), intervalText),
	toggle('shuffle', 'wallpaper', tr('Shuffle', 'Перемешивать'), tr('Random order. When off, the pictures go in alphabetical order.', 'Случайный порядок. Если выключено, картинки идут по алфавиту.'),
		(c) => c.wallpaper.shuffle, (c, v) => ({...c, wallpaper: {...c.wallpaper, shuffle: v}})),

	// ----- Options -----
	toggle('closeHome', 'options', tr('Close LG Home at start', 'Закрывать LG Home при запуске'),
		tr('Frees memory by ending LG Home whenever the launcher starts. Needs the Homebrew Channel. Switches itself off if LG Home keeps coming back.', 'Освобождает память, завершая LG Home при каждом запуске лаунчера. Нужен Homebrew Channel. Выключится сам, если LG Home будет возвращаться.'),
		(c) => c.closeHome, (c, v) => ({...c, closeHome: v})),

	{
		id: 'replaceHome', tab: 'options', label: tr('Make the launcher the default instead of LG Home', 'Сделать лаунчер главным вместо LG Home'), kind: 'action',
		description: tr('A small background helper opens the launcher whenever LG Home comes on screen (the Home and Back keys). Nothing is replaced. OK turns it on or off.', 'Небольшая фоновая программа открывает лаунчер, когда на экран выходит LG Home (кнопки «Домой», «Назад»). Ничего не заменяется. OK включает или выключает.'),
		text: (_c, info) => onOff(!info || !info.homeHook ? null : info.homeHook.running)
	},
	{
		id: 'startAtBoot', tab: 'options', label: tr('Start the helper when the TV starts', 'Запускать помощник при включении ТВ'), kind: 'action',
		description: tr('Adds a link to the TV\'s start-up folder. Turn it off to remove it; the helper then starts when the launcher opens.', 'Добавляет ссылку в папку автозапуска ТВ. Выключите, чтобы убрать её; тогда помощник запустится при открытии лаунчера.'),
		text: (_c, info) => onOff(!info || !info.homeHook ? null : info.homeHook.linked)
	},


	{
		id: 'closeHomeNow', tab: 'options', label: tr('Close LG Home now', 'Закрыть LG Home сейчас'), kind: 'action',
		description: tr('Ends it straight away and shows how much memory that freed.', 'Завершает его сразу и показывает, сколько памяти освободилось.'),
		text: () => tr('Run', 'Выполнить')
	}
];

let built: {lang: Lang; prefs: Pref[]} | null = null;

/** Every row, worded in the current language. */
export function prefs (): Pref[] {
	if (!built || built.lang !== lang()) built = {lang: lang(), prefs: (WEATHER_ENABLED ? basePrefs().concat(weatherPrefs()) : basePrefs()).concat(calendarPrefs())};
	return built.prefs;
}

export function findPref (id: string): Pref | undefined {
	return prefs().find((p) => p.id === id);
}

/** The order of the Options tab (anything not listed goes last). */
const OPTIONS_ORDER = ['replaceHome', 'startAtBoot', 'closeHome', 'closeHomeNow'];

function orderOf (tab: PrefTab, id: string): number {
	if (tab !== 'options') return 0;
	const i = OPTIONS_ORDER.indexOf(id);
	return i === -1 ? OPTIONS_ORDER.length : i;
}

/** The Settings list for one tab. */
export function optionsFor (tab: PrefTab, config: UserConfig, info?: SystemInfo): OptionItem[] {
	return prefs().filter((p) => p.tab === tab).sort((a, b) => orderOf(tab, a.id) - orderOf(tab, b.id)).map((p) => ({
		id: p.id,
		label: p.label,
		description: p.description,
		disabled: disabledReason(p.id, info) || undefined,
		value: p.kind === 'toggle' && p.on ? p.on(config) : undefined,
		valueText: p.kind !== 'toggle' && p.text ? p.text(config, info) : undefined,
		stepper: p.kind === 'choice'
	}));
}

/** The config after pressing OK / ← / → on option `id`; the same config when nothing changes. */
export function applyPref (config: UserConfig, id: string, press: Press): UserConfig {
	const pref = findPref(id);
	return pref && pref.change ? pref.change(config, press) : config;
}
