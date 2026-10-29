// The settings screens as data: every option of every section (Appearance, Widgets, Wallpaper, Home screen),
// how it reads the config, and how OK / ← → change it. The app builds the Settings lists
// from this table and applies a change with `applyPref`.

import type {OptionItem} from '../components/OptionList';
import type {ThemeName} from '../theme/tokens';
import {
	DEFAULT_CLOCK, INTERVAL_VALUES, RANGES, rangeValues, stepValue,
	type ClockFormat, type ClockWeight, type PixelShift, type RangeName, type UserConfig
} from './storage';
import {CLOCK_FONT_IDS, fontLabel} from './clockfont';
import {CLOCK_COLOR_IDS, colorLabel, type ClockColor} from './clockcolor';
import {baseName} from './wallpaper';
import type {PlateMode} from './plates';
import {WEATHER_ENABLED} from '../config/features';
import type {TempUnit} from './weather';
import {lastFailure, type RootState} from './failure';
import {APP_VERSION} from '../config/constants';
import type {HomeMode, HomeModeInfo} from './homemode';
import type {SplashMode} from './splash';
import {count, fmt, lang, langName, langTag, LANG_SETTINGS, tr, type Lang} from './i18n';
import {countryName, countryOf, tvLanguageTag} from './feeds';

export type PrefTab = 'look' | 'widgets' | 'wallpaper' | 'home';

/** Facts about the TV (not user settings) that some rows display. */
export interface SystemInfo {
	/** the background helper: running, and whether it starts at boot (null while unknown) */
	homeHook: {running: boolean; linked: boolean} | null;
	/** the launcher sits in LG Home's place (tools/takeover.sh or tools/defaulthome.sh); null while unknown */
	takeover?: boolean | null;
	/** how the launcher replaces LG Home (tools/homemode.sh); null while unknown */
	homeMode?: HomeModeInfo | null;
	/** can this TV run commands through the Homebrew Channel (rooted)? */
	root?: RootState;
	/** what the TV said when it could not (shown under a greyed-out row) */
	rootDetail?: string;
}

/** The options that run shell commands, so they can't work without root. */
export const ROOT_OPTIONS = ['replaceHome', 'homeMethod'];

/** Why a row is greyed out, or null when it is usable. */
export function disabledReason (id: string, info?: SystemInfo): string | null {
	if (!info || info.root !== 'no' || ROOT_OPTIONS.indexOf(id) === -1) return null;
	const kind = lastFailure();
	const why = kind === 'denied' ? tr('the TV refused the command.', 'ТВ отклонил команду.') : tr('the Homebrew Channel service is not running. If the TV is rooted, open the Homebrew Channel once, then open Settings again.', 'служба Homebrew Channel не запущена. Если ТВ рутован, один раз откройте Homebrew Channel, затем заново откройте настройки.');
	return fmt(tr('Needs a rooted TV: {0}', 'Нужен рутованный ТВ: {0}'), why) + (info.rootDetail ? fmt(tr(' (the TV said: {0})', ' (ответ ТВ: {0})'), info.rootDetail) : '');
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
const modeShort = (mode: HomeMode | null): string => mode === null ? tr('Unknown', 'Неизвестно') : mode === 'default' ? tr('Official', 'Штатно') : mode === 'mount' ? tr('Swap', 'Подмена') : mode === 'helper' ? tr('Helper', 'Помощник') : tr('Off', 'Выкл.');

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
const WEIGHTS: readonly ClockWeight[] = [200, 300, 400, 500, 600];
const UNITS: readonly TempUnit[] = ['c', 'f'];
const SHIFTS: readonly PixelShift[] = ['auto', 'on', 'off'];
const SPLASHES: readonly SplashMode[] = ['wallpaper', 'logo', 'black'];
const splashText = (v: SplashMode): string => v === 'logo' ? tr('Logo', 'Логотип') : v === 'black' ? tr('Black', 'Чёрный экран') : tr('Wallpaper', 'Обои');
type Labels = 'all' | 'selected' | 'none';
const LABELS: readonly Labels[] = ['all', 'selected', 'none'];
const labelsOf = (c: UserConfig): Labels => !c.showLabels ? 'none' : c.labelsSelectedOnly ? 'selected' : 'all';
const labelsText = (v: Labels): string => v === 'none' ? tr('None', 'Нет') : v === 'selected' ? tr('Selected only', 'Только у выбранной') : tr('All', 'У всех');
const shiftText = (v: PixelShift): string => v === 'on' ? tr('On', 'Вкл.') : v === 'off' ? tr('Off', 'Выкл.') : tr('Auto', 'Авто');

const plateText = (v: PlateMode): string => v === 'light' ? tr('Light', 'Светлые') : v === 'dark' ? tr('Dark', 'Тёмные') : tr('Auto', 'Авто');
const themeText = (v: ThemeName): string => v === 'black' ? tr('OLED black', 'Чёрная (OLED)') : v === 'grey' ? tr('Calm blue-grey', 'Серо-голубая') : tr('Evening sky', 'Вечернее небо');
const formatText = (v: ClockFormat): string => v === '12h' ? tr('12-hour (AM/PM)', '12-часовой (AM/PM)') : v === '24h' ? tr('24-hour', '24-часовой') : tr('Same as the TV', 'Как на ТВ');
const weightText = (v: ClockWeight): string =>
	v === 300 ? tr('Light', 'Лёгкий', 'Light (weight)') : v === 400 ? tr('Regular', 'Обычный') : v === 500 ? tr('Medium', 'Средний') : v === 600 ? tr('Bold', 'Жирный') : tr('Thin', 'Тонкий');

/** The look, margins and clock back to their defaults (apps, sources and wallpaper are left alone). */
export function resetLook (c: UserConfig): UserConfig {
	return {
		...c,
		theme: 'dusk',
		iconScale: RANGES.iconScale.def,
		showLabels: true,
		labelsSelectedOnly: false,
		sideMargin: RANGES.sideMargin.def,
		bottomMargin: RANGES.bottomMargin.def,
		cardSize: RANGES.cardSize.def,
		plates: 'auto',
		glass: true,
		frost: RANGES.frost.def,
		shadows: true,
		clockFormat: '24h',
		clock: {...DEFAULT_CLOCK}
	};
}

/** The weather rows of Widgets (left out of builds made without the weather widget). */
const weatherPrefs = (): Pref[] => group(tr('Weather', 'Погода'), [
	toggle('weatherOn', 'widgets', tr('Show the weather', 'Показывать погоду'), tr('The temperature and the forecast, on their own card.', 'Температура и погода под датой.'),
		(c) => c.weather.on, (c, v) => ({...c, weather: {...c.weather, on: v && c.weather.city !== null}})),
	{
		id: 'weatherCity', tab: 'widgets', opens: true, label: tr('City', 'Город'), kind: 'action',
		description: tr('Pick a city from the list, or search for any place by name. Needs the internet.', 'Выберите город из списка или найдите любое место по названию. Нужен интернет.'),
		text: (c) => (c.weather.city ? c.weather.city.name : tr('Not chosen', 'Не выбран'))
	},
	choice('weatherUnit', 'widgets', tr('Temperature unit', 'Единицы температуры'), tr('Degrees Celsius or Fahrenheit.', 'Градусы Цельсия или Фаренгейта.'),
		UNITS, (c) => c.weather.unit, (c, v) => ({...c, weather: {...c.weather, unit: v}}), (v) => (v === 'f' ? tr('Fahrenheit (°F)', 'Фаренгейт (°F)') : tr('Celsius (°C)', 'Цельсий (°C)'))),
	toggle('weatherCityName', 'widgets', tr('Show the city', 'Показывать город'), tr('The city name before the temperature.', 'Название города перед температурой.'),
		(c) => c.weather.showCity, (c, v) => ({...c, weather: {...c.weather, showCity: v}}))
]);

/** The calendar rows of Widgets. */
const calendarPrefs = (): Pref[] => group(tr('Calendar', 'Календарь'), [
	toggle('calendarOn', 'widgets', tr('Show the calendar', 'Показывать календарь'), tr('The day\'s events in the top right of the home screen.', 'События дня в правом верхнем углу главного экрана.'),
		(c) => c.calendar.on, (c, v) => ({...c, calendar: {...c.calendar, on: v && c.calendar.sources.length > 0}})),
	{
		id: 'calendarUrl', tab: 'widgets', opens: true, label: tr('Calendars', 'Календари'), kind: 'action',
		description: tr('iCal links from Google Calendar, .ics files or a whole folder. Several calendars can be shown at once.', 'Ссылки в формате iCal из Google Календаря, файлы .ics или целая папка. Можно подключить несколько календарей сразу.'),
		text: (c) => (c.calendar.sources.length ? count(c.calendar.sources.length, ['calendar', 'calendars'], ['календарь', 'календаря', 'календарей']) : tr('None', 'Не задан', 'None (calendars)'))
	}
]);

/** The news rows of Widgets. */
const newsPrefs = (): Pref[] => group(tr('News', 'Новости'), [
	toggle('newsOn', 'widgets', tr('Show the news', 'Показывать новости'), tr('Headlines on their own card. OK on a headline shows it with a QR code to read it on your phone. The TV reads most news sites through the Homebrew Channel (root).', 'Заголовки на отдельной карточке. OK на заголовке показывает его с QR-кодом, чтобы дочитать на телефоне. Большинство новостных сайтов ТВ читает через Homebrew Channel (нужен рут).'),
		(c) => c.news.on, (c, v) => ({...c, news: {...c.news, on: v && (c.news.google || c.news.feeds.length > 0)}})),
	toggle('newsGoogle', 'widgets', tr('Google News', 'Google Новости'), tr('The top stories of the chosen country, in the launcher\'s language when the country has an edition in it.', 'Главные новости выбранной страны — на языке лаунчера, если у страны есть такая редакция.'),
		(c) => c.news.google, (c, v) => ({...c, news: {...c.news, google: v, on: c.news.on && (v || c.news.feeds.length > 0)}})),
	only((c) => c.news.google, {
		id: 'newsCountry', tab: 'widgets', opens: true, label: tr('Country', 'Страна'), kind: 'action',
		description: tr('Whose top stories Google News shows. At first, the TV\'s country, or the country of the launcher\'s language.', 'Чьи главные новости показывает Google Новости. Сначала — страна ТВ или страна языка лаунчера.'),
		text: (c) => countryName(countryOf(c.news.country, tvLanguageTag(), lang()), langTag())
	}),
	{
		id: 'newsFeeds', tab: 'widgets', opens: true, label: tr('Your feeds', 'Свои ленты'), kind: 'action',
		description: tr('Addresses of RSS or Atom feeds of any site, shown together with Google News or instead of it.', 'Адреса RSS- или Atom-лент любых сайтов. Показываются вместе с Google Новостями или вместо них.'),
		text: (c) => (c.news.feeds.length ? count(c.news.feeds.length, ['feed', 'feeds'], ['лента', 'ленты', 'лент']) : tr('None', 'Нет', 'None (feeds)'))
	}
]);

/** The quote rows of Widgets. */
const quotePrefs = (): Pref[] => group(tr('Quote of the day', 'Фраза дня'), [
	toggle('quoteOn', 'widgets', tr('Show the quote of the day', 'Показывать фразу дня'), tr('A quote from Wikiquote in the launcher\'s language, a new one every day.', 'Цитата из Викицитатника на языке лаунчера, каждый день новая.'),
		(c) => c.quote.on, (c, v) => ({...c, quote: {on: v}}))
]);

const basePrefs = (): Pref[] => [
	// ----- Appearance -----
	...group(tr('App row', 'Ряд приложений'), [
		range('iconScale', 'look', tr('Icon size', 'Размер значков'), tr('How big the app tiles are.', 'Насколько крупные плитки приложений.'),
			'iconScale', (c) => c.iconScale, (c, v) => ({...c, iconScale: v}), (v) => `${v}%`),
		choice('showLabels', 'look', tr('App names', 'Названия приложений'), tr('A name under each tile, only under the selected one, or none (only the icons).', 'Подпись под каждой плиткой, только под выбранной или никакой (только значки).'),
			LABELS, labelsOf, (c, v) => ({...c, showLabels: v !== 'none', labelsSelectedOnly: v === 'selected'}), labelsText),
		range('sideMargin', 'look', tr('Side margin', 'Поля по бокам'), tr('Space left and right of the row.', 'Отступ слева и справа от ряда.'),
			'sideMargin', (c) => c.sideMargin, (c, v) => ({...c, sideMargin: v}), (v) => `${v} px`),
		range('bottomMargin', 'look', tr('Bottom margin', 'Поле снизу'), tr('Space below the row.', 'Отступ под рядом.'),
			'bottomMargin', (c) => c.bottomMargin, (c, v) => ({...c, bottomMargin: v}), (v) => `${v} px`)
	]),
	...group(tr('Cards', 'Карточки'), [
		range('cardSize', 'look', tr('Text size', 'Размер текста'), tr('All the text on the weather and calendar cards.', 'Весь текст на карточках погоды и календаря.'),
			'cardSize', (c) => c.cardSize, (c, v) => ({...c, cardSize: v}), (v) => `${v}%`),
		choice('plates', 'look', tr('Card colour', 'Цвет карточек'), tr('Auto: all the cards turn light or dark together, depending on the picture behind them, so the text stays readable on each.', 'Авто: все карточки вместе становятся светлыми или тёмными в зависимости от картинки за ними, чтобы текст хорошо читался на каждой.'),
			PLATE_MODES, (c) => c.plates, (c, v) => ({...c, plates: v}), plateText),
		only((c) => c.wallpaper.mode !== 'gradient', toggle('glass', 'look', tr('Frosted glass', 'Матовое стекло'), tr('The cards blur the picture behind them.', 'Карточки размывают картинку под собой.'),
			(c) => c.glass, (c, v) => ({...c, glass: v}))),
		only((c) => c.wallpaper.mode !== 'gradient' && c.glass, range('frost', 'look', tr('Frost', 'Матовость'), tr('How much the glass blurs the picture behind it: 100% is the most, 30% lets most of the picture through.', 'Насколько стекло размывает картинку под собой: 100% — сильнее всего, на 30% картинка видна чётче всего.'),
			'frost', (c) => c.frost, (c, v) => ({...c, frost: v}), (v) => `${v}%`)),
		toggle('shadows', 'look', tr('Shadows', 'Тени'), tr('A soft shadow under the clock and the cards. Without it the TV has a little less to draw.', 'Мягкая тень под часами и карточками. Без неё ТВ рисует экран чуть легче.'),
			(c) => c.shadows, (c, v) => ({...c, shadows: v})),
		choice('pixelShift', 'look', tr('Pixel shift (OLED)', 'Сдвиг пикселей (OLED)'), tr('The clock, the cards and the apps move a few pixels once a minute, so an OLED screen does not keep the same picture on the same pixels for hours. Auto: on when the TV is an OLED (by its model name).', 'Часы, карточки и приложения раз в минуту сдвигаются на несколько пикселей, чтобы OLED-экран часами не держал одно и то же изображение на одних пикселях. Авто: включается, если ТВ — OLED (по названию модели).'),
			SHIFTS, (c) => c.pixelShift, (c, v) => ({...c, pixelShift: v}), shiftText)
	]),

	// ----- Widgets: the clock, the weather and the calendar -----
	choice('clockFont', 'widgets', tr('Font', 'Шрифт'), tr('For the clock, the date, the weather and the calendar. "TV font" is built into the TV; the others come with the launcher.', 'Для часов, даты, погоды и календаря. «Шрифт ТВ» встроен в телевизор, остальные идут с лаунчером.'),
		CLOCK_FONT_IDS, (c) => c.clock.font, (c, v) => ({...c, clock: {...c.clock, font: v}}), (v) => fontLabel(v)),
	...group(tr('Clock', 'Часы'), [
		choice('clockFormat', 'widgets', tr('Format', 'Формат'), tr('12-hour, 24-hour, or the same as the TV.', '12 или 24 часа, или как на ТВ.'),
			CLOCK_FORMATS, (c) => c.clockFormat, (c, v) => ({...c, clockFormat: v}), formatText),
		range('clockSize', 'widgets', tr('Size', 'Размер'), tr('The height of the digits.', 'Высота цифр.'),
			'clockSize', (c) => c.clock.size, (c, v) => ({...c, clock: {...c.clock, size: v}}), (v) => `${v} px`),
		choice('clockWeight', 'widgets', tr('Weight', 'Толщина'), tr('From thin to bold digits.', 'От тонких до жирных цифр.'),
			WEIGHTS, (c) => c.clock.weight, (c, v) => ({...c, clock: {...c.clock, weight: v}}), weightText),
		choice('clockColor', 'widgets', tr('Colour', 'Цвет'), tr('White is hard to read on a light picture: pick a dark colour or a gradient for light wallpapers.', 'Белый плохо читается на светлой картинке: для светлых обоев выберите тёмный цвет или градиент.'),
			CLOCK_COLOR_IDS, (c) => c.clock.color, (c, v) => ({...c, clock: {...c.clock, color: v}}), (v: ClockColor) => colorLabel(v))
	]),
	...group(tr('Date', 'Дата'), [
		toggle('showDate', 'widgets', tr('Show the date', 'Показывать дату'), tr('The weekday and the date beside the time.', 'День недели и дата рядом со временем.'),
			(c) => c.clock.showDate, (c, v) => ({...c, clock: {...c.clock, showDate: v}})),
		only((c) => c.clock.showDate, range('dateSize', 'widgets', tr('Date size', 'Размер даты'), tr('The size of the weekday and the date.', 'Размер дня недели и даты.'),
			'dateSize', (c) => c.clock.dateSize, (c, v) => ({...c, clock: {...c.clock, dateSize: v}}), (v) => `${v}%`))
	]),

	// ----- Wallpaper -----
	{
		id: 'wallpaper', tab: 'wallpaper', opens: true, label: tr('Picture', 'Картинка'), kind: 'action',
		description: tr('One picture, or a folder of pictures as a slideshow, from a USB drive or the TV\'s storage.', 'Одна картинка или папка с картинками для слайд-шоу, с USB-накопителя или из памяти ТВ.'),
		text: (c) => c.wallpaper.mode === 'gradient' ? tr('None', 'Нет') : c.wallpaper.mode === 'pictures' ? count(c.wallpaper.paths.length, ['picture', 'pictures'], ['картинка', 'картинки', 'картинок']) : baseName(c.wallpaper.path)
	},
	...group(tr('Slideshow', 'Слайд-шоу'), [
		only((c) => c.wallpaper.mode === 'folder' || c.wallpaper.mode === 'pictures', choice('interval', 'wallpaper', tr('Change every', 'Менять каждые'), tr('How long each picture stays on screen.', 'Сколько каждая картинка остаётся на экране.'),
			INTERVAL_VALUES, (c) => c.wallpaper.intervalSec, (c, v) => ({...c, wallpaper: {...c.wallpaper, intervalSec: v}}), intervalText)),
		only((c) => c.wallpaper.mode === 'folder' || c.wallpaper.mode === 'pictures', toggle('shuffle', 'wallpaper', tr('Shuffle', 'Перемешивать'), tr('Random order. Off: in alphabetical order.', 'Случайный порядок. Если выключено, по алфавиту.'),
			(c) => c.wallpaper.shuffle, (c, v) => ({...c, wallpaper: {...c.wallpaper, shuffle: v}}))),
		choice('splash', 'wallpaper', tr('Start-up picture', 'Заставка при запуске'), tr('What the TV shows for a moment while the launcher starts (after the TV has unloaded it to free memory): the wallpaper, the launcher\'s logo or black.', 'Что ТВ показывает на мгновение, пока лаунчер запускается (после того как ТВ выгрузил его из памяти): обои, логотип лаунчера или чёрный экран.'),
			SPLASHES, (c) => c.splash, (c, v) => ({...c, splash: v}), splashText)
	]),
	...group(tr('Colours', 'Цвета'), [
		choice('theme', 'wallpaper', tr('Colour theme', 'Цветовая тема'), tr('The background when there is no picture, and the colours of these screens. OLED black leaves unused pixels off.', 'Фон, когда картинки нет, и цвета этих экранов. Чёрная тема не зажигает лишние пиксели OLED.'),
			THEMES, (c) => c.theme, (c, v) => ({...c, theme: v}), themeText)
	]),

	// ----- Home screen -----
	choice('language', 'home', tr('Language', 'Язык'), tr('The language of the launcher. "Same as the TV" follows the TV\'s own language.', 'Язык лаунчера. «Как на ТВ» следует языку самого телевизора.'),
		LANG_SETTINGS, (c) => c.language, (c, v) => ({...c, language: v}), langName),
	{
		id: 'resetLook', tab: 'home', label: tr('Reset look and clock', 'Сбросить вид и часы'), kind: 'action',
		description: tr('Sizes, margins, cards and the clock go back to how they were at first. Apps, wallpaper, weather and calendars stay.', 'Размеры, поля, карточки и часы вернутся к начальным. Приложения, обои, погода и календари останутся.'),
		text: () => ''
	},
	...group(tr('Opening the launcher', 'Запуск лаунчера'), [{
		id: 'homeMethod', tab: 'home', label: tr('How the launcher opens', 'Как открывается лаунчер'), kind: 'action', opens: true,
		description: tr('Whether the Home button, Back out of the last app and waking the TV open the launcher, and in which way. Each way with its pros and cons.', 'Открывают ли лаунчер кнопка Home, выход из последнего приложения и включение ТВ, и каким способом. У каждого способа свои плюсы и минусы.'),
		text: (_c, info) => modeShort(info && info.homeMode ? info.homeMode.mode : null)
	}]),
	...group(tr('About', 'О лаунчере'), [{
		id: 'about', tab: 'home', label: 'Simple LG launcher', kind: 'info',
		description: tr('Based on Hearth by rudra-avatar. github.com/fardroid/Simple-LG-launcher', 'Основан на Hearth от rudra-avatar. github.com/fardroid/Simple-LG-launcher'),
		text: () => APP_VERSION
	}]),
];

let built: {lang: Lang; prefs: Pref[]} | null = null;

/** Every row, worded in the current language. */
export function prefs (): Pref[] {
	if (!built || built.lang !== lang()) built = {lang: lang(), prefs: (WEATHER_ENABLED ? basePrefs().concat(weatherPrefs()) : basePrefs()).concat(calendarPrefs(), newsPrefs(), quotePrefs())};
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
