// Tests for the weather widget's pure parts.   Run: npm test
// The texts are checked in Russian (the screens speak the TV's language).
Object.defineProperty(globalThis, 'navigator', {value: {language: 'ru-RU', languages: ['ru-RU']}, configurable: true});
import {build} from 'esbuild';
const dir = new URL('../node_modules/', import.meta.url).pathname;
await build({entryPoints: [new URL('../src/lib/weather.ts', import.meta.url).pathname], bundle: true, format: 'esm', outfile: dir + '.cache-weather-test.mjs', platform: 'node', logLevel: 'error'});
await build({define: {'import.meta.glob': '__glob'}, banner: {js: 'const __glob = () => ({});'}, entryPoints: [new URL('../src/lib/weathernet.ts', import.meta.url).pathname], bundle: true, format: 'esm', outfile: dir + '.cache-weathernet-test.mjs', platform: 'node', logLevel: 'error'});
const w = await import(dir + '.cache-weather-test.mjs');
const n = await import(dir + '.cache-weathernet-test.mjs');
let fail = 0;
const eq = (a, b, m) => { const ok = JSON.stringify(a) === JSON.stringify(b); if (!ok) { fail++; console.log('FAIL', m, JSON.stringify(a), '!=', JSON.stringify(b)); } else console.log('ok  ', m); };

const reply = JSON.stringify({current: {temperature_2m: 29.4, weather_code: 2, is_day: 1}, daily: {temperature_2m_max: [31.2], temperature_2m_min: [25.1]}});
const d = w.parseWeather(reply, 12.97, 77.59, 1000);
eq(d && [d.tempC, d.highC, d.lowC, d.code, d.isDay, d.at], [29.4, 31.2, 25.1, 2, true, 1000], 'parses a forecast reply');
eq(w.parseWeather('not json', 0, 0, 0), null, 'garbage is not a forecast');
eq(w.parseWeather('{"current":{}}', 0, 0, 0), null, 'a reply without a temperature is rejected');
eq(w.parseWeather(JSON.stringify({current: {temperature_2m: 5, weather_code: 0, is_day: 0}}), 0, 0, 5).highC, null, 'missing daily values become null');
eq(w.parseCities('nope'), [], 'garbage gives no cities');
const cities = w.parseCities(JSON.stringify({results: [{name: 'Pune', latitude: 18.52, longitude: 73.85, country: 'India', admin1: 'Maharashtra'}, {name: 'bad'}]}));
eq(cities.length, 1, 'cities without coordinates are dropped');
eq(cities[0].name, 'Pune', 'city name is kept');

eq(w.formatTemp(21.6, 'c'), '22°', 'celsius rounds');
eq(w.formatTemp(0, 'f'), '32°', 'fahrenheit conversion');
eq(w.formatTemp(-0.2, 'c'), '0°', 'no negative zero');
eq(w.describe(0, true).icon, 'sun', 'clear day is a sun');
eq(w.describe(0, false).icon, 'moon', 'clear night is a moon');
eq(w.describe(95, true).icon, 'storm', 'thunderstorm');
eq(w.describe(61, true).icon, 'rain', 'rain');
eq(w.describe(73, true).icon, 'snow', 'snow');
eq(d.hourly, [], 'a reply without hourly values gives no curve');
eq(d.daily, [], 'a reply without a daily list gives no days');

// a full 7-day reply: hourly values for two days (only today's every 3rd hour is kept)
const times = [], temps = [], codes = [], isDay = [];
for (let day = 0; day < 2; day++) for (let h = 0; h < 24; h++) { times.push('2026-10-2' + (4 + day) + 'T' + (h < 10 ? '0' : '') + h + ':00'); temps.push(10 + h / 2); codes.push(h < 6 ? 0 : 2); isDay.push(h >= 6 && h < 19 ? 1 : 0); }
const full = JSON.stringify({
  current: {temperature_2m: 19.2, apparent_temperature: 17.8, weather_code: 2, is_day: 1, wind_speed_10m: 3.6},
  hourly: {time: times, temperature_2m: temps, weather_code: codes, is_day: isDay},
  daily: {time: ['2026-10-24', '2026-10-25', '2026-10-26', '2026-10-27', '2026-10-28', '2026-10-29', '2026-10-30'], weather_code: [2, 0, 3, 61, 63, 3, 0], temperature_2m_max: [22.1, 21, 19, 18, 17, 19, 21], temperature_2m_min: [14, 14, 13, 12, 11, 12, 13]}
});
const f = w.parseWeather(full, 40.4, 49.8, 5000);
eq([f.hourly.length, f.hourly[0], f.hourly[7], f.hourly[8]], [16, {day: 0, hour: 0, tempC: 10, code: 0, isDay: false}, {day: 0, hour: 21, tempC: 20.5, code: 2, isDay: false}, {day: 1, hour: 0, tempC: 10, code: 0, isDay: false}], 'every day has 8 points, one every 3 hours');
eq([f.daily.length, f.daily[0], f.daily[6].date], [7, {date: '2026-10-24', code: 2, highC: 22.1, lowC: 14}, '2026-10-30'], 'seven days are read');
eq([f.feelsC, f.windMs], [17.8, 3.6], 'feels-like and wind are read');
const v = w.weatherView(f, 'c', 'Баку');
eq([v.temp, v.text, v.detail, v.city], ['19°', 'Переменная облачность', 'Ощущается как 18° · ветер 4 м/с', 'Баку'], 'the card text, in Russian');
eq(v.hours.map((h) => h.label).join(' '), '00:00 03:00 06:00 09:00 12:00 15:00 18:00 21:00', 'hour labels');
eq([v.hours[0].icon, v.hours[3].icon, v.hours[7].icon], ['moon', 'partly', 'cmoon'], 'night hours get the moon, day hours the sun or cloud');
eq(v.days.map((x) => x.label).join(', '), 'Сегодня, Вс 25, Пн 26, Вт 27, Ср 28, Чт 29, Пт 30', 'day labels');
eq([v.days[3].icon, v.days[3].high, v.days[3].low], ['rain', '18°', '12°'], 'a day has an icon and its high and low');
const tomorrow = w.weatherView(f, 'c', 'Баку', 1);
eq([tomorrow.day, tomorrow.dayName, tomorrow.hours.length, tomorrow.days.map((x) => x.selected).join()], [1, 'Воскресенье, 25 октября', 8, 'false,true,false,false,false,false,false'], 'another day: its own name, curve and selected tile');
eq([tomorrow.temp, tomorrow.text, tomorrow.detail], [v.temp, v.text, v.detail], 'the top row (now) does not change with the chosen day');
eq(w.weatherView(f, 'c', '', 99).day, 6, 'a day past the forecast is clamped');
eq(v.dayName, 'Сегодня', 'today is called Сегодня');
eq(w.weatherView(f, 'f', '').temp, '67°', 'fahrenheit');
eq(w.describe(95, true).text, 'Гроза', 'descriptions are in Russian');

eq(w.isFresh(d, 12.97, 77.59, 1000 + 60000), true, 'a minute old is fresh');
eq(w.isFresh(d, 12.97, 77.59, 1000 + w.FRESH_MS + 1), false, 'older than 30 minutes is not fresh');
eq(w.isFresh(d, 1, 1, 2000), false, 'a different place is not fresh');
eq(!!w.usableCached(d, 12.97, 77.59, 1000 + w.SHOWN_MS - 1), true, 'under 6 hours is still shown');
eq(w.usableCached(d, 12.97, 77.59, 1000 + w.SHOWN_MS + 1), null, 'over 6 hours is hidden');
eq(w.usableCached(d, 12.97, 77.59, 500), null, 'a reading from the future is ignored');
eq(w.validCoords(91, 0), false, 'latitude out of range');
eq(w.validCoords(10, 20), true, 'good coordinates');
eq(w.forecastUrl(12.9716, 77.5946).indexOf('latitude=12.972&longitude=77.595') > 0, true, 'coordinates are rounded in the URL');
eq(w.geocodeUrl('São Paulo & co').indexOf('name=S%C3%A3o%20Paulo%20%26%20co') > 0, true, 'search text is encoded');

const cmd = n.shellFetchCommand('https://x.test/a?b=1&c=2');
eq([cmd.indexOf("U='https://x.test/a?b=1&c=2';") === 0, cmd.indexOf('curl') > 0, cmd.indexOf('wget') > 0], [true, true, true], 'shell fallback tries curl then wget');
// The same card in English: a TV set to English gets English words, and the wind in km/h (mph with Fahrenheit).
Object.defineProperty(globalThis, 'navigator', {value: {language: 'en-GB', languages: ['en-GB']}, configurable: true});
await build({entryPoints: [new URL('../src/lib/weather.ts', import.meta.url).pathname], bundle: true, format: 'esm', outfile: dir + '.cache-weather-en-test.mjs', platform: 'node', logLevel: 'error'});
const we = await import(dir + '.cache-weather-en-test.mjs');
const ve = we.weatherView(f, 'c', 'Baku');
eq([ve.text, ve.detail, ve.days[0].label, ve.days[1].label], ['Partly cloudy', 'Feels like 18° · wind 14 km/h', 'Today', 'Sun 25'], 'the card text, in English');
eq(we.weatherView(f, 'f', 'Baku').detail, 'Feels like 64° · wind 9 mph', 'mph with Fahrenheit');
eq(we.geocodeUrl('Milan').indexOf('language=en') > 0, true, 'the city search answers in English');

process.exit(fail ? 1 : 0);
