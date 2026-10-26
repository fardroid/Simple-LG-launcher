// A short list of cities to pick from without typing (the picker can also search any place by name).
// Coordinates are approximate city centres; the weather service does not need more.

import type {City} from './weather';
import {tr} from './i18n';

function city (en: string, ru: string, countryEn: string, countryRu: string, lat: number, lon: number): City {
	return {name: tr(en, ru), lat, lon, detail: tr(countryEn, countryRu)};
}

/** The list, named in the current language. */
export function presetCities (): City[] {
	return [
		city('Baku', 'Баку', 'Azerbaijan', 'Азербайджан', 40.4093, 49.8671),
		city('Ganja', 'Гянджа', 'Azerbaijan', 'Азербайджан', 40.6828, 46.3606),
		city('Sumgait', 'Сумгаит', 'Azerbaijan', 'Азербайджан', 40.5855, 49.6317),
		city('Moscow', 'Москва', 'Russia', 'Россия', 55.7558, 37.6173),
		city('Saint Petersburg', 'Санкт-Петербург', 'Russia', 'Россия', 59.9343, 30.3351),
		city('Kazan', 'Казань', 'Russia', 'Россия', 55.7961, 49.1064),
		city('Yekaterinburg', 'Екатеринбург', 'Russia', 'Россия', 56.8389, 60.6057),
		city('Novosibirsk', 'Новосибирск', 'Russia', 'Россия', 55.0084, 82.9357),
		city('Krasnodar', 'Краснодар', 'Russia', 'Россия', 45.0355, 38.9753),
		city('Sochi', 'Сочи', 'Russia', 'Россия', 43.5855, 39.7231),
		city('Kyiv', 'Киев', 'Ukraine', 'Украина', 50.4501, 30.5234),
		city('Minsk', 'Минск', 'Belarus', 'Беларусь', 53.9006, 27.559),
		city('Tbilisi', 'Тбилиси', 'Georgia', 'Грузия', 41.7151, 44.8271),
		city('Yerevan', 'Ереван', 'Armenia', 'Армения', 40.1792, 44.4991),
		city('Astana', 'Астана', 'Kazakhstan', 'Казахстан', 51.1694, 71.4491),
		city('Almaty', 'Алматы', 'Kazakhstan', 'Казахстан', 43.2389, 76.8897),
		city('Tashkent', 'Ташкент', 'Uzbekistan', 'Узбекистан', 41.2995, 69.2401),
		city('Bishkek', 'Бишкек', 'Kyrgyzstan', 'Киргизия', 42.8746, 74.5698),
		city('Istanbul', 'Стамбул', 'Turkey', 'Турция', 41.0082, 28.9784),
		city('Ankara', 'Анкара', 'Turkey', 'Турция', 39.9334, 32.8597),
		city('Dubai', 'Дубай', 'UAE', 'ОАЭ', 25.2048, 55.2708),
		city('Berlin', 'Берлин', 'Germany', 'Германия', 52.52, 13.405),
		city('Paris', 'Париж', 'France', 'Франция', 48.8566, 2.3522),
		city('London', 'Лондон', 'United Kingdom', 'Великобритания', 51.5074, -0.1278),
		city('Madrid', 'Мадрид', 'Spain', 'Испания', 40.4168, -3.7038),
		city('Rome', 'Рим', 'Italy', 'Италия', 41.9028, 12.4964),
		city('Milan', 'Милан', 'Italy', 'Италия', 45.4642, 9.19),
		city('Warsaw', 'Варшава', 'Poland', 'Польша', 52.2297, 21.0122),
		city('Prague', 'Прага', 'Czechia', 'Чехия', 50.0755, 14.4378),
		city('Vienna', 'Вена', 'Austria', 'Австрия', 48.2082, 16.3738),
		city('New York', 'Нью-Йорк', 'USA', 'США', 40.7128, -74.006),
		city('Toronto', 'Торонто', 'Canada', 'Канада', 43.6532, -79.3832)
	];
}
