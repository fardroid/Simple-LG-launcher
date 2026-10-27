// Time-of-day text. Pure functions so they are trivially testable.

import {lang, langTag, tr} from './i18n';

export {plural} from './i18n';

export type TimeFormat = '12h' | '24h';

const DAYS_RU = ['воскресенье', 'понедельник', 'вторник', 'среда', 'четверг', 'пятница', 'суббота'];
const MONTHS_RU = ['января', 'февраля', 'марта', 'апреля', 'мая', 'июня', 'июля', 'августа', 'сентября', 'октября', 'ноября', 'декабря'];
const MONTH_NAMES_RU = ['Январь', 'Февраль', 'Март', 'Апрель', 'Май', 'Июнь', 'Июль', 'Август', 'Сентябрь', 'Октябрь', 'Ноябрь', 'Декабрь'];
const DAY_SHORT_RU = ['Вс', 'Пн', 'Вт', 'Ср', 'Чт', 'Пт', 'Сб'];

const DAYS_EN = ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'];
const MONTH_NAMES_EN = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];
const DAY_SHORT_EN = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];

const formatters: Record<string, Intl.DateTimeFormat | null> = {};

/** The platform's own wording of a date in the current language (null when this TV's browser cannot, so English is used). */
function intl (options: Intl.DateTimeFormatOptions, y: number, m: number, d: number): string | null {
	const key = langTag() + JSON.stringify(options);
	if (!(key in formatters)) {
		try { formatters[key] = new Intl.DateTimeFormat(langTag(), {...options, timeZone: 'UTC'}); } catch { formatters[key] = null; }
	}
	const f = formatters[key];
	if (!f) return null;
	try {
		const date = new Date(Date.UTC(2000, m, d));
		date.setUTCFullYear(y);
		const text = f.format(date);
		return text.charAt(0).toUpperCase() + text.slice(1);
	} catch {
		return null;
	}
}

const own = (): boolean => lang() !== 'en' && lang() !== 'ru';

/** A month's name as a heading: "October". */
export function monthName (month: number): string {
	if (own()) { const text = intl({month: 'long'}, 2000, month, 1); if (text) return text; }
	return (lang() === 'ru' ? MONTH_NAMES_RU : MONTH_NAMES_EN)[month];
}

/** A short weekday name, Sunday = 0: "Sun". */
export function dayShort (day: number): string {
	if (own()) { const text = intl({weekday: 'short'}, 2000, 0, 2 + day); if (text) return text; }   // 2 January 2000 was a Sunday
	return (lang() === 'ru' ? DAY_SHORT_RU : DAY_SHORT_EN)[day];
}

export function greeting (date: Date): string {
	const h = date.getHours();
	if (h < 6) return tr('Good night', 'Доброй ночи');
	if (h < 12) return tr('Good morning', 'Доброе утро');
	if (h < 18) return tr('Good afternoon', 'Добрый день');
	return tr('Good evening', 'Добрый вечер');
}

/** "Sunday" */
export function dayName (date: Date): string {
	if (own()) { const text = intl({weekday: 'long'}, 2000, 0, 2 + date.getDay()); if (text) return text; }
	if (lang() !== 'ru') return DAYS_EN[date.getDay()];
	const name = DAYS_RU[date.getDay()];
	return name.charAt(0).toUpperCase() + name.slice(1);
}

/** "23 September" */
export function dateText (date: Date): string {
	if (own()) { const text = intl({day: 'numeric', month: 'long'}, date.getFullYear(), date.getMonth(), date.getDate()); if (text) return text; }
	return `${date.getDate()} ${lang() === 'ru' ? MONTHS_RU[date.getMonth()] : MONTH_NAMES_EN[date.getMonth()]}`;
}

/** "11 October 2026" */
function dateYear (date: Date): string {
	if (own()) { const text = intl({day: 'numeric', month: 'long', year: 'numeric'}, date.getFullYear(), date.getMonth(), date.getDate()); if (text) return text; }
	return `${dateText(date)} ${date.getFullYear()}`;
}

/** "Sunday, 11 October 2026" */
export function fullDate (date: Date): string {
	return `${dayName(date)}, ${dateYear(date)}`;
}

/** The two lines beside the clock: "Sunday" and "11 October 2026". */
export function dateLines (date: Date): {day: string; date: string} {
	return {day: dayName(date), date: dateYear(date)};
}

/** "Sunday, 11 October" */
export function dayTitle (date: Date): string {
	return `${dayName(date)}, ${dateText(date)}`;
}

export interface TimeParts {
	/** "15:42" or "3:42" */
	time: string;
	/** "AM" / "PM" in 12-hour format, otherwise empty */
	period: string;
}

export function timeParts (date: Date, format: TimeFormat): TimeParts {
	const h = date.getHours();
	const m = pad(date.getMinutes());
	if (format === '24h') return {time: `${pad(h)}:${m}`, period: ''};
	return {time: `${h % 12 || 12}:${m}`, period: h < 12 ? 'AM' : 'PM'};
}

function pad (n: number): string {
	return n < 10 ? `0${n}` : String(n);
}
