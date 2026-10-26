// Time-of-day text. Pure functions so they are trivially testable.

import {lang, tr} from './i18n';

export {plural} from './i18n';

export type TimeFormat = '12h' | '24h';

const DAYS_RU = ['воскресенье', 'понедельник', 'вторник', 'среда', 'четверг', 'пятница', 'суббота'];
const MONTHS_RU = ['января', 'февраля', 'марта', 'апреля', 'мая', 'июня', 'июля', 'августа', 'сентября', 'октября', 'ноября', 'декабря'];
const MONTH_NAMES_RU = ['Январь', 'Февраль', 'Март', 'Апрель', 'Май', 'Июнь', 'Июль', 'Август', 'Сентябрь', 'Октябрь', 'Ноябрь', 'Декабрь'];
const DAY_SHORT_RU = ['Вс', 'Пн', 'Вт', 'Ср', 'Чт', 'Пт', 'Сб'];

const DAYS_EN = ['Sunday', 'Monday', 'Tuesday', 'Wednesday', 'Thursday', 'Friday', 'Saturday'];
const MONTH_NAMES_EN = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];
const DAY_SHORT_EN = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];

/** A month's name as a heading: "October". */
export function monthName (month: number): string {
	return (lang() === 'ru' ? MONTH_NAMES_RU : MONTH_NAMES_EN)[month];
}

/** A short weekday name, Sunday = 0: "Sun". */
export function dayShort (day: number): string {
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
	if (lang() !== 'ru') return DAYS_EN[date.getDay()];
	const name = DAYS_RU[date.getDay()];
	return name.charAt(0).toUpperCase() + name.slice(1);
}

/** "23 September" */
export function dateText (date: Date): string {
	return `${date.getDate()} ${lang() === 'ru' ? MONTHS_RU[date.getMonth()] : MONTH_NAMES_EN[date.getMonth()]}`;
}

/** "Sunday, 11 October 2026" */
export function fullDate (date: Date): string {
	return `${dayName(date)}, ${dateText(date)} ${date.getFullYear()}`;
}

/** The two lines beside the clock: "Sunday" and "11 October 2026". */
export function dateLines (date: Date): {day: string; date: string} {
	return {day: dayName(date), date: `${dateText(date)} ${date.getFullYear()}`};
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
