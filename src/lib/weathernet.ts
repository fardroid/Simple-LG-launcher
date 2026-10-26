// Fetching the weather and searching for places. The web view asks the service directly. Some older TVs
// cannot verify modern web certificates, so when that fails and the Homebrew Channel is available, the same
// address is fetched through the TV's own curl or wget instead (the last two attempts skip the certificate
// check: the reply is public weather data that is only ever read as numbers and a place name).

import {exec} from './apps';
import {forecastUrl, geocodeUrl, parseCities, parseWeather, validCoords, type City, type WeatherData} from './weather';

/** Only plain https addresses made of safe characters ever reach a shell. */
const SAFE_URL = /^https:\/\/[A-Za-z0-9._\/?=&,%-]+$/;

function viaWebView (url: string, timeoutMs: number): Promise<string> {
	return new Promise<string>((resolve, reject) => {
		const request = new XMLHttpRequest();
		request.open('GET', url, true);
		request.timeout = timeoutMs;
		request.onload = () => (request.status >= 200 && request.status < 300 ? resolve(request.responseText) : reject(new Error('HTTP ' + request.status)));
		request.onerror = () => reject(new Error('network error'));
		request.ontimeout = () => reject(new Error('timed out'));
		request.send();
	});
}

export function shellFetchCommand (url: string): string {
	return `U='${url}'; curl -s -m 12 "$U" || curl -sk -m 12 "$U" || wget -qO- -T 12 "$U" || wget -qO- -T 12 --no-check-certificate "$U"`;
}

function viaShell (url: string): Promise<string> {
	if (!SAFE_URL.test(url)) return Promise.reject(new Error('unsafe address'));
	return exec(shellFetchCommand(url), 40000);
}

/** The text at `url`: asked directly first, then (if allowed) through the TV's shell. */
export function fetchText (url: string, allowShell: boolean): Promise<string> {
	return viaWebView(url, 10000).catch((error: Error) => (allowShell ? viaShell(url) : Promise.reject(error)));
}

export function fetchWeather (city: City, allowShell: boolean): Promise<WeatherData> {
	if (!validCoords(city.lat, city.lon)) return Promise.reject(new Error('bad place'));
	return fetchText(forecastUrl(city.lat, city.lon), allowShell).then((text) => {
		const data = parseWeather(text, city.lat, city.lon, Date.now());
		if (!data) throw new Error('unreadable reply');
		return data;
	});
}

export function searchCities (query: string, allowShell: boolean): Promise<City[]> {
	if (query.trim().length < 2) return Promise.resolve([]);
	return fetchText(geocodeUrl(query), allowShell).then(parseCities);
}
