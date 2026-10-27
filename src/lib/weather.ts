// The weather card: now, the hours of today and the next days, from Open-Meteo (free, no key). This file is the
// pure part (reading the reply, describing it, formatting, the saved copy); weathernet.ts does the fetching.

import {dayShort, dayTitle} from './clock';
import {fmt, lang, langTag, tr} from './i18n';

export type TempUnit = 'c' | 'f';
export type WeatherIcon = 'sun' | 'moon' | 'partly' | 'cmoon' | 'cloud' | 'fog' | 'rain' | 'snow' | 'storm';

export interface City {
	name: string;
	lat: number;
	lon: number;
	/** region and country, for telling cities of the same name apart */
	detail?: string;
}

/** One reading in a day's curve (every 3 hours). */
export interface HourPoint {
	/** which day of the forecast: 0 is today */
	day: number;
	/** hour of the day, 0-23 */
	hour: number;
	tempC: number;
	code: number;
	isDay: boolean;
}

/** One day of the forecast. */
export interface DayPoint {
	/** "2026-10-24" */
	date: string;
	code: number;
	highC: number | null;
	lowC: number | null;
}

export interface WeatherData {
	/** the place it is for */
	lat: number;
	lon: number;
	/** °C, as the service gives it (converted for display) */
	tempC: number;
	highC: number | null;
	lowC: number | null;
	/** WMO weather code */
	code: number;
	isDay: boolean;
	/** "feels like", °C */
	feelsC: number | null;
	/** wind, metres per second */
	windMs: number | null;
	/** every 3 hours of each forecast day (empty when the reply had no hourly part) */
	hourly: HourPoint[];
	/** today and the next days */
	daily: DayPoint[];
	/** when it was read (ms since 1970) */
	at: number;
}

export const WEATHER_CACHE_KEY = 'customlauncher-weather-v3';
/** A reading younger than this is not fetched again. */
export const FRESH_MS = 30 * 60 * 1000;
/** An older reading than this is no longer shown (the TV was off for a long while). */
export const SHOWN_MS = 6 * 60 * 60 * 1000;

const num = (x: unknown): number | null => (typeof x === 'number' && isFinite(x) ? x : null);

/** Coordinates as they go into a web address: plain numbers only, nothing else can get in. */
function coord (n: number): string {
	return (Math.round(n * 1000) / 1000).toFixed(3);
}

export function validCoords (lat: unknown, lon: unknown): boolean {
	return typeof lat === 'number' && typeof lon === 'number' && isFinite(lat) && isFinite(lon) && Math.abs(lat) <= 90 && Math.abs(lon) <= 180;
}

export function forecastUrl (lat: number, lon: number): string {
	return 'https://api.open-meteo.com/v1/forecast?latitude=' + coord(lat) + '&longitude=' + coord(lon)
		+ '&current=temperature_2m,apparent_temperature,weather_code,is_day,wind_speed_10m&hourly=temperature_2m,weather_code,is_day'
		+ '&daily=weather_code,temperature_2m_max,temperature_2m_min&forecast_days=7&wind_speed_unit=ms&timezone=auto';
}

export function geocodeUrl (query: string): string {
	return 'https://geocoding-api.open-meteo.com/v1/search?name=' + encodeURIComponent(query.trim().slice(0, 60)) + '&count=8&language=' + langTag() + '&format=json';
}

/** Reads Open-Meteo's forecast reply; null when it isn't one. */
export function parseWeather (json: string, lat: number, lon: number, now: number): WeatherData | null {
	try {
		const data = JSON.parse(json);
		const current = data && data.current;
		const temp = current ? num(current.temperature_2m) : null;
		const code = current ? num(current.weather_code) : null;
		if (temp === null || code === null) return null;
		const daily = data.daily || {};
		const at = (list: unknown, i: number): number | null => (Array.isArray(list) ? num(list[i]) : null);
		const days: DayPoint[] = [];
		const dates: unknown[] = Array.isArray(daily.time) ? daily.time : [];
		dates.slice(0, 7).forEach((date, i) => {
			const dayCode = at(daily.weather_code, i);
			if (typeof date === 'string' && dayCode !== null) days.push({date, code: dayCode, highC: at(daily.temperature_2m_max, i), lowC: at(daily.temperature_2m_min, i)});
		});
		const hourly: HourPoint[] = [];
		const hours = data.hourly || {};
		const times: unknown[] = Array.isArray(hours.time) ? hours.time : [];
		times.forEach((t, i) => {
			if (typeof t !== 'string') return;
			const day = days.findIndex((d) => t.indexOf(d.date) === 0);
			if (day === -1) return;
			const hour = parseInt(t.slice(11, 13), 10);
			const hourTemp = at(hours.temperature_2m, i);
			const hourCode = at(hours.weather_code, i);
			if (hour % 3 === 0 && hourTemp !== null && hourCode !== null) hourly.push({day, hour, tempC: hourTemp, code: hourCode, isDay: at(hours.is_day, i) !== 0});
		});
		return {
			lat, lon, tempC: temp, highC: at(daily.temperature_2m_max, 0), lowC: at(daily.temperature_2m_min, 0), code, isDay: current.is_day !== 0,
			feelsC: num(current.apparent_temperature), windMs: num(current.wind_speed_10m), hourly, daily: days, at: now
		};
	} catch {
		return null;
	}
}

/** Reads Open-Meteo's place-search reply into a list of cities. */
export function parseCities (json: string): City[] {
	try {
		const data = JSON.parse(json);
		const list = data && Array.isArray(data.results) ? data.results : [];
		const out: City[] = [];
		list.forEach((r: Record<string, unknown>) => {
			if (typeof r.name !== 'string' || !validCoords(r.latitude, r.longitude)) return;
			const detail = [r.admin1, r.country].filter((x) => typeof x === 'string' && x).join(', ');
			out.push({name: r.name, lat: r.latitude as number, lon: r.longitude as number, detail: detail || undefined});
		});
		return out;
	} catch {
		return [];
	}
}

/** Words and a picture for a WMO weather code. `night` swaps the sun for the moon. */
export function describe (code: number, isDay: boolean): {text: string; icon: WeatherIcon} {
	if (code === 0) return {text: tr('Clear', 'Ясно'), icon: isDay ? 'sun' : 'moon'};
	if (code === 1) return {text: tr('Mainly clear', 'Преимущественно ясно'), icon: isDay ? 'sun' : 'moon'};
	if (code === 2) return {text: tr('Partly cloudy', 'Переменная облачность'), icon: isDay ? 'partly' : 'cmoon'};
	if (code === 3) return {text: tr('Overcast', 'Пасмурно'), icon: 'cloud'};
	if (code === 45 || code === 48) return {text: tr('Fog', 'Туман'), icon: 'fog'};
	if (code >= 51 && code <= 55) return {text: tr('Drizzle', 'Морось'), icon: 'rain'};
	if (code === 56 || code === 57) return {text: tr('Freezing drizzle', 'Ледяная морось'), icon: 'rain'};
	if (code === 61) return {text: tr('Light rain', 'Небольшой дождь'), icon: 'rain'};
	if (code === 63) return {text: tr('Rain', 'Дождь'), icon: 'rain'};
	if (code === 65) return {text: tr('Heavy rain', 'Сильный дождь'), icon: 'rain'};
	if (code === 66 || code === 67) return {text: tr('Freezing rain', 'Ледяной дождь'), icon: 'rain'};
	if (code === 71) return {text: tr('Light snow', 'Небольшой снег'), icon: 'snow'};
	if (code === 73) return {text: tr('Snow', 'Снег'), icon: 'snow'};
	if (code === 75) return {text: tr('Heavy snow', 'Сильный снег'), icon: 'snow'};
	if (code === 77) return {text: tr('Snow grains', 'Снежная крупа'), icon: 'snow'};
	if (code === 80) return {text: tr('Light showers', 'Небольшие ливни'), icon: 'rain'};
	if (code === 81) return {text: tr('Showers', 'Ливни'), icon: 'rain'};
	if (code === 82) return {text: tr('Heavy showers', 'Сильные ливни'), icon: 'rain'};
	if (code === 85 || code === 86) return {text: tr('Snow showers', 'Снегопад'), icon: 'snow'};
	if (code === 95) return {text: tr('Thunderstorm', 'Гроза'), icon: 'storm'};
	if (code === 96 || code === 99) return {text: tr('Thunderstorm with hail', 'Гроза с градом'), icon: 'storm'};
	return {text: tr('Cloudy', 'Облачно'), icon: 'cloud'};
}

/** "29°" (always whole degrees, in the chosen unit). */
export function formatTemp (celsius: number, unit: TempUnit): string {
	const value = unit === 'f' ? celsius * 9 / 5 + 32 : celsius;
	const rounded = Math.round(value);
	return (rounded === 0 ? 0 : rounded) + '°';        // no "-0°"
}

export interface HourView {
	/** "09:00" */
	label: string;
	/** "18°" */
	temp: string;
	/** the temperature in °C, for placing the point on the curve */
	tempC: number;
	hour: number;
	icon: WeatherIcon;
}

export interface DayView {
	/** "Today", "Sun 25" */
	label: string;
	icon: WeatherIcon;
	high: string;
	low: string;
	today: boolean;
	/** the day whose hours are drawn */
	selected: boolean;
}

export interface WeatherView {
	icon: WeatherIcon;
	/** "19°" */
	temp: string;
	/** "Partly cloudy" */
	text: string;
	/** "Feels like 18° · wind 14 km/h" */
	detail: string;
	city: string;
	/** the day the curve is drawn for: 0 is today */
	day: number;
	/** its name: "Today", "Sunday, 25 October" */
	dayName: string;
	/** the curve of that day */
	hours: HourView[];
	days: DayView[];
}

/** "2026-10-25" → "Sun 25" (plain date arithmetic: no time zones involved). */
function dayLabel (date: string): string {
	const y = parseInt(date.slice(0, 4), 10), m = parseInt(date.slice(5, 7), 10), d = parseInt(date.slice(8, 10), 10);
	return dayShort(new Date(Date.UTC(y, m - 1, d)).getUTCDay()) + ' ' + d;
}

/** "2026-10-25" → "Sunday, 25 October". */
function pickedTitle (date: string): string {
	return dayTitle(new Date(parseInt(date.slice(0, 4), 10), parseInt(date.slice(5, 7), 10) - 1, parseInt(date.slice(8, 10), 10)));
}

/** "wind 4 m/s" in Russian; in English km/h, or mph with Fahrenheit. */
function windText (ms: number, unit: TempUnit): string {
	if (lang() === 'ru') return 'ветер ' + Math.round(ms) + ' м/с';
	return fmt(tr('wind {0}', 'ветер {0}'), unit === 'f' ? Math.round(ms * 2.237) + ' mph' : Math.round(ms * 3.6) + ' km/h');
}

/** What the weather card shows. */
export function weatherView (data: WeatherData, unit: TempUnit, cityName: string, day = 0): WeatherView {
	const picked = Math.max(0, Math.min(day, data.daily.length - 1));
	const d = describe(data.code, data.isDay);
	const detail: string[] = [];
	if (data.feelsC !== null) detail.push(fmt(tr('Feels like {0}', 'Ощущается как {0}'), formatTemp(data.feelsC, unit)));
	if (data.windMs !== null) detail.push(windText(data.windMs, unit));
	return {
		icon: d.icon,
		temp: formatTemp(data.tempC, unit),
		text: d.text,
		detail: detail.join(' · '),
		city: cityName,
		day: picked,
		dayName: picked === 0 || !data.daily[picked] ? tr('Today', 'Сегодня') : pickedTitle(data.daily[picked].date),
		hours: data.hourly.filter((h) => h.day === picked).map((h) => ({label: (h.hour < 10 ? '0' : '') + h.hour + ':00', temp: formatTemp(h.tempC, unit), tempC: h.tempC, hour: h.hour, icon: describe(h.code, h.isDay).icon})),
		days: data.daily.map((day, i) => ({
			label: i === 0 ? tr('Today', 'Сегодня') : dayLabel(day.date),
			icon: describe(day.code, true).icon,
			high: day.highC === null ? '–' : formatTemp(day.highC, unit),
			low: day.lowC === null ? '–' : formatTemp(day.lowC, unit),
			today: i === 0,
			selected: i === picked
		}))
	};
}

export function isFresh (data: WeatherData | null, lat: number, lon: number, now: number): boolean {
	return !!data && data.lat === lat && data.lon === lon && now - data.at < FRESH_MS && now >= data.at;
}

/** The saved reading, if it is for this place and not too old. */
export function usableCached (data: WeatherData | null, lat: number, lon: number, now: number): WeatherData | null {
	return data && data.lat === lat && data.lon === lon && now - data.at < SHOWN_MS && now >= data.at ? data : null;
}

export function readWeatherCache (): WeatherData | null {
	try {
		const raw = localStorage.getItem(WEATHER_CACHE_KEY);
		if (!raw) return null;
		const d = JSON.parse(raw);
		if (!validCoords(d.lat, d.lon) || num(d.tempC) === null || num(d.code) === null || num(d.at) === null) return null;
		if (!Array.isArray(d.daily) || !Array.isArray(d.hourly)) return null;
		return {lat: d.lat, lon: d.lon, tempC: d.tempC, highC: num(d.highC), lowC: num(d.lowC), code: d.code, isDay: d.isDay !== false, feelsC: num(d.feelsC), windMs: num(d.windMs), hourly: d.hourly, daily: d.daily, at: d.at};
	} catch {
		return null;
	}
}

export function writeWeatherCache (data: WeatherData): void {
	try { localStorage.setItem(WEATHER_CACHE_KEY, JSON.stringify(data)); } catch { /* storage full: it is just fetched again */ }
}
