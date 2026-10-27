// The settings screens as data: every option of every section (General, Appearance, Clock, Weather, Calendar, Wallpaper, Home screen),
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
import {APP_VERSION} from '../config/constants';
import {count, lang, langName, LANG_SETTINGS, tr, type Lang} from './i18n';

export type PrefTab = 'general' | 'look' | 'clock' | 'weather' | 'calendar' | 'wallpaper' | 'home';

/** Facts about the TV (not user settings) that some rows display. */
export interface SystemInfo {
	/** the background helper: running, and whether it starts at boot (null while unknown) */
	homeHook: {running: boolean; linked: boolean} | null;
	/** the launcher sits in LG Home's place (tools/takeover.sh); null while unknown */
	takeover?: boolean | null;
	/** can this TV run commands through the Homebrew Channel (rooted)? */
	root?: RootState;
	/** what the TV said when it could not (shown under a greyed-out row) */
	rootDetail?: string;
}

/** The options that run shell commands, so they can't work without root. */
export const ROOT_OPTIONS = ['replaceHome'];

/** Why a row is greyed out, or null when it is usable. */
export function disabledReason (id: string, info?: SystemInfo): string | null {
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
	/** info: a fact to read, nothing to change */
	kind: 'toggle' | 'choice' | 'action' | 'info';
	/** the heading of the group of rows it belongs to (rows of one group follow each other) */
	group?: string;
	/** an action that opens another screen (a chevron points onwards) */
	opens?: boolean;
	/** shown only when this says so (for example the slideshow rows only with a slideshow) */
	when?: (c: UserConfig, info?: SystemInfo) => boolean;
	/** toggle: is it on */
	on?: (c: UserConfig) => boolean;
	/** choice / action: the current value as text */
	text?: (c: UserConfig, info?: SystemInfo) => string;
	/** the new config after a key press (toggle: any press flips it) */
	change?: (c: UserConfig, press: Press) => UserConfig;
}

const delta = (press: Press): number => (press === 'left' ? -1 : 1);

/** The rows with their group heading set. */
const group = (heading: string, rows: Pref[]): Pref[] => rows.map((p) => ({...p, group: heading}));
/** The row, shown only when `when` says so. */
const only = (when: (c: UserConfig, info?: SystemInfo) => boolean, p: Pref): Pref => ({...p, when});

/** "On", "Off", or "Unknown" while it is not known. */
const onOff = (on: boolean | null): string => on === null ? tr('Unknown', 'Неизвестно') : on ? tr('On', 'Вкл.') : tr('Off', 'Выкл.');
const yesNo = (yes: boolean | null): string => yes === null ? tr('Unknown', 'Неизвестно') : yes ? tr('Yes', 'Да') : tr('No', 'Нет');

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
const themeText = (v: ThemeName): string => v === 'black' ? tr('OLED black', 'Чёрная (OLED)') : v === 'grey' ? tr('Calm blue-grey', 'Серо-голубая') : tr('Evening sky', 'Вечернее небо');
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
		id: 'weatherCity', tab: 'weather', opens: true, label: tr('City', 'Город'), kind: 'action',
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
		id: 'calendarUrl', tab: 'calendar', opens: true, label: tr('Calendars', 'Календари'), kind: 'action',
		description: tr('iCal links from Google Calendar, .ics files or a whole folder. Several calendars can be shown at once.', 'Ссылки в формате iCal из Google Календаря, файлы .ics или целая папка. Можно подключить несколько календарей сразу.'),
		text: (c) => (c.calendar.sources.length ? count(c.calendar.sources.length, ['calendar', 'calendars'], ['календарь', 'календаря', 'календарей']) : tr('None', 'Не задан'))
	}
];

const basePrefs = (): Pref[] => [
	// ----- General -----
	choice('language', 'general', 'Language · Язык', tr('The language of the launcher. "Same as the TV" follows the TV\'s own language.', 'Язык лаунчера. «Как на ТВ» следует языку самого телевизора.'),
		LANG_SETTINGS, (c) => c.language, (c, v) => ({...c, language: v}), langName),
	{
		id: 'resetLook', tab: 'general', label: tr('Reset look and clock', 'Сбросить вид и часы'), kind: 'action',
		description: tr('Sizes, margins, cards and the clock go back to how they were at first. Apps, wallpaper, weather and calendars stay.', 'Размеры, поля, карточки и часы вернутся к начальным. Приложения, обои, погода и календари останутся.'),
		text: () => ''
	},
	...group(tr('About', 'О лаунчере'), [{
		id: 'about', tab: 'general', label: 'Simple LG launcher', kind: 'info',
		description: tr('Based on Hearth by rudra-avatar. github.com/fardroid/Simple-LG-launcher', 'Основан на Hearth от rudra-avatar. github.com/fardroid/Simple-LG-launcher'),
		text: () => APP_VERSION
	}]),

	// ----- Appearance -----
	...group(tr('App row', 'Ряд приложений'), [
		range('iconScale', 'look', tr('Icon size', 'Размер значков'), tr('How big the app tiles are.', 'Насколько крупные плитки приложений.'),
			'iconScale', (c) => c.iconScale, (c, v) => ({...c, iconScale: v}), (v) => `${v}%`),
		toggle('showLabels', 'look', tr('App names', 'Названия приложений'), tr('A name under each tile. Off leaves only the icons.', 'Подпись под каждой плиткой. Если выключить, останутся только значки.'),
			(c) => c.showLabels, (c, v) => ({...c, showLabels: v})),
		range('sideMargin', 'look', tr('Side margin', 'Поля по бокам'), tr('Space left and right of the row.', 'Отступ слева и справа от ряда.'),
			'sideMargin', (c) => c.sideMargin, (c, v) => ({...c, sideMargin: v}), (v) => `${v} px`),
		range('bottomMargin', 'look', tr('Bottom margin', 'Поле снизу'), tr('Space below the row.', 'Отступ под рядом.'),
			'bottomMargin', (c) => c.bottomMargin, (c, v) => ({...c, bottomMargin: v}), (v) => `${v} px`)
	]),
	...group(tr('Cards', 'Карточки'), [
		range('cardSize', 'look', tr('Text size', 'Размер текста'), tr('All the text on the weather and calendar cards.', 'Весь текст на карточках погоды и календаря.'),
			'cardSize', (c) => c.cardSize, (c, v) => ({...c, cardSize: v}), (v) => `${v}%`),
		choice('plates', 'look', tr('Card colour', 'Цвет карточек'), tr('Auto: each card turns light or dark depending on the picture behind it, so the text stays readable.', 'Авто: каждая карточка становится светлой или тёмной в зависимости от картинки за ней, чтобы текст хорошо читался.'),
			PLATE_MODES, (c) => c.plates, (c, v) => ({...c, plates: v}), plateText),
		only((c) => c.wallpaper.mode !== 'gradient', toggle('glass', 'look', tr('Frosted glass', 'Матовое стекло'), tr('The cards blur the picture behind them.', 'Карточки размывают картинку под собой.'),
			(c) => c.glass, (c, v) => ({...c, glass: v}))),
		toggle('shadows', 'look', tr('Shadows', 'Тени'), tr('A soft shadow under the clock and the cards. Without it the TV has a little less to draw.', 'Мягкая тень под часами и карточками. Без неё ТВ рисует экран чуть легче.'),
			(c) => c.shadows, (c, v) => ({...c, shadows: v}))
	]),

	// ----- Clock -----
	...group(tr('Time', 'Время'), [
		choice('clockFormat', 'clock', tr('Format', 'Формат'), tr('12-hour, 24-hour, or the same as the TV.', '12 или 24 часа, или как на ТВ.'),
			CLOCK_FORMATS, (c) => c.clockFormat, (c, v) => ({...c, clockFormat: v}), formatText),
		range('clockSize', 'clock', tr('Size', 'Размер'), tr('The height of the digits.', 'Высота цифр.'),
			'clockSize', (c) => c.clock.size, (c, v) => ({...c, clock: {...c.clock, size: v}}), (v) => `${v} px`),
		choice('clockFont', 'clock', tr('Font', 'Шрифт'), tr('"TV font" is built into the TV; the others come with the launcher.', '«Шрифт ТВ» встроен в телевизор, остальные идут с лаунчером.'),
			CLOCK_FONT_IDS, (c) => c.clock.font, (c, v) => ({...c, clock: {...c.clock, font: v}}), (v) => fontLabel(v)),
		choice('clockWeight', 'clock', tr('Weight', 'Толщина'), tr('Thin, light or regular digits.', 'Тонкие, лёгкие или обычные цифры.'),
			WEIGHTS, (c) => c.clock.weight, (c, v) => ({...c, clock: {...c.clock, weight: v}}), weightText),
		choice('clockColor', 'clock', tr('Colour', 'Цвет'), tr('White is hard to read on a light picture: pick a dark colour or a gradient for light wallpapers.', 'Белый плохо читается на светлой картинке: для светлых обоев выберите тёмный цвет или градиент.'),
			CLOCK_COLOR_IDS, (c) => c.clock.color, (c, v) => ({...c, clock: {...c.clock, color: v}}), (v: ClockColor) => colorLabel(v))
	]),
	...group(tr('Date', 'Дата'), [
		toggle('showDate', 'clock', tr('Show the date', 'Показывать дату'), tr('The weekday and the date beside the time.', 'День недели и дата рядом со временем.'),
			(c) => c.clock.showDate, (c, v) => ({...c, clock: {...c.clock, showDate: v}})),
		only((c) => c.clock.showDate, range('dateSize', 'clock', tr('Date size', 'Размер даты'), tr('The size of the weekday and the date.', 'Размер дня недели и даты.'),
			'dateSize', (c) => c.clock.dateSize, (c, v) => ({...c, clock: {...c.clock, dateSize: v}}), (v) => `${v}%`))
	]),

	// ----- Wallpaper -----
	{
		id: 'wallpaper', tab: 'wallpaper', opens: true, label: tr('Picture', 'Картинка'), kind: 'action',
		description: tr('One picture, or a folder of pictures as a slideshow, from a USB drive or the TV\'s storage.', 'Одна картинка или папка с картинками для слайд-шоу, с USB-накопителя или из памяти ТВ.'),
		text: (c) => c.wallpaper.mode === 'gradient' ? tr('None', 'Нет') : baseName(c.wallpaper.path)
	},
	...group(tr('Slideshow', 'Слайд-шоу'), [
		only((c) => c.wallpaper.mode === 'folder', choice('interval', 'wallpaper', tr('Change every', 'Менять каждые'), tr('How long each picture stays on screen.', 'Сколько каждая картинка остаётся на экране.'),
			INTERVAL_VALUES, (c) => c.wallpaper.intervalSec, (c, v) => ({...c, wallpaper: {...c.wallpaper, intervalSec: v}}), intervalText)),
		only((c) => c.wallpaper.mode === 'folder', toggle('shuffle', 'wallpaper', tr('Shuffle', 'Перемешивать'), tr('Random order. Off: in alphabetical order.', 'Случайный порядок. Если выключено, по алфавиту.'),
			(c) => c.wallpaper.shuffle, (c, v) => ({...c, wallpaper: {...c.wallpaper, shuffle: v}})))
	]),
	...group(tr('Colours', 'Цвета'), [
		choice('theme', 'wallpaper', tr('Colour theme', 'Цветовая тема'), tr('The background when there is no picture, and the colours of these screens. OLED black leaves unused pixels off.', 'Фон, когда картинки нет, и цвета этих экранов. Чёрная тема не зажигает лишние пиксели OLED.'),
			THEMES, (c) => c.theme, (c, v) => ({...c, theme: v}), themeText)
	]),

	// ----- Home screen -----
	{
		id: 'homeStatus', tab: 'home', label: tr('In place of LG Home', 'Вместо LG Home'), kind: 'info',
		description: tr('When the launcher takes LG Home\'s place, the TV itself opens it for the Home button, Back out of the last app and waking up. It is set up from a computer with tools/takeover.sh (see the README); "takeover.sh disable" gives LG Home back.', 'Когда лаунчер стоит на месте LG Home, телевизор сам открывает его по кнопке Home, при выходе из последнего приложения и при включении. Это настраивается с компьютера скриптом tools/takeover.sh (см. README); «takeover.sh disable» возвращает LG Home.'),
		text: (_c, info) => yesNo(!info || info.takeover === undefined ? null : info.takeover)
	},
	only((_c, info) => !info || info.takeover !== true, {
		id: 'replaceHome', tab: 'home', label: tr('Background helper', 'Фоновый помощник'), kind: 'action',
		description: tr('The other way, for when the launcher is not in LG Home\'s place: a small program on the TV watches for LG Home and opens the launcher instead, from the moment the TV starts. LG Home can show for a moment first.', 'Другой способ, если лаунчер не стоит на месте LG Home: небольшая программа на ТВ замечает LG Home и открывает вместо него лаунчер, с самого включения ТВ. LG Home может мелькнуть на мгновение.'),
		text: (_c, info) => onOff(!info || !info.homeHook ? null : info.homeHook.running)
	})
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

/** The Settings list for one section. */
export function optionsFor (tab: PrefTab, config: UserConfig, info?: SystemInfo): OptionItem[] {
	return prefs().filter((p) => p.tab === tab && (!p.when || p.when(config, info))).map((p) => ({
		id: p.id,
		label: p.label,
		description: p.description,
		group: p.group,
		kind: p.kind,
		opens: p.opens,
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
