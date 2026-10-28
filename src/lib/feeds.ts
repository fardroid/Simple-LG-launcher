// The news card: headlines from Google News for the chosen country (in the launcher's language when the country has an edition
// in it, otherwise in the country's own) and from the user's own feeds. RSS 2.0, RSS 1.0 (RDF) and Atom are read; the feeds are
// joined into one list, newest first, without repeats. This file is the pure part (editions, parsing, joining, the saved copy);
// feedsload.ts does the fetching.

import {decodeEntities, htmlToText} from './htmltext';
import type {Lang} from './i18n';

export interface FeedItem {
	title: string;
	link: string;
	/** when it was published, ms (0: not given) */
	at: number;
	/** the outlet or the feed's own name */
	source: string;
	/** the description as plain text ('' when none) */
	summary: string;
	/** which feed it came from (index in the list of sources) */
	feed: number;
}

export interface NewsData {
	/** the sources it was read from, joined (a change of sources makes the saved copy useless) */
	key: string;
	at: number;
	items: FeedItem[];
}

/** One Google News edition: the launcher language it is in, and the hl and ceid of its address. */
type Edition = [Lang | string, string, string];

/** Google News editions by country (checked to return headlines), English name first. */
export const EDITIONS: Record<string, {name: string; editions: Edition[]}> = {
	AR: {name: 'Argentina', editions: [['es', 'es-419', 'AR:es-419']]},
	AU: {name: 'Australia', editions: [['en', 'en-AU', 'AU:en']]},
	BG: {name: 'Bulgaria', editions: [['bg', 'bg', 'BG:bg']]},
	BR: {name: 'Brazil', editions: [['pt', 'pt-BR', 'BR:pt-419']]},
	CA: {name: 'Canada', editions: [['en', 'en-CA', 'CA:en'], ['fr', 'fr-CA', 'CA:fr']]},
	CL: {name: 'Chile', editions: [['es', 'es-419', 'CL:es-419']]},
	CN: {name: 'China', editions: [['zh', 'zh-CN', 'CN:zh-Hans']]},
	CO: {name: 'Colombia', editions: [['es', 'es-419', 'CO:es-419']]},
	CZ: {name: 'Czechia', editions: [['cs', 'cs', 'CZ:cs']]},
	DE: {name: 'Germany', editions: [['de', 'de', 'DE:de']]},
	EG: {name: 'Egypt', editions: [['ar', 'ar', 'EG:ar']]},
	ES: {name: 'Spain', editions: [['es', 'es', 'ES:es']]},
	FR: {name: 'France', editions: [['fr', 'fr', 'FR:fr']]},
	GB: {name: 'United Kingdom', editions: [['en', 'en-GB', 'GB:en']]},
	GR: {name: 'Greece', editions: [['el', 'el', 'GR:el']]},
	HK: {name: 'Hong Kong', editions: [['zh', 'zh-HK', 'HK:zh-Hant']]},
	HU: {name: 'Hungary', editions: [['hu', 'hu', 'HU:hu']]},
	ID: {name: 'Indonesia', editions: [['id', 'id', 'ID:id']]},
	IE: {name: 'Ireland', editions: [['en', 'en-IE', 'IE:en']]},
	IL: {name: 'Israel', editions: [['he', 'he', 'IL:he'], ['en', 'en-IL', 'IL:en']]},
	IN: {name: 'India', editions: [['en', 'en-IN', 'IN:en']]},
	IT: {name: 'Italy', editions: [['it', 'it', 'IT:it']]},
	JP: {name: 'Japan', editions: [['ja', 'ja', 'JP:ja']]},
	KR: {name: 'South Korea', editions: [['ko', 'ko', 'KR:ko']]},
	LT: {name: 'Lithuania', editions: [['lt', 'lt', 'LT:lt']]},
	LV: {name: 'Latvia', editions: [['lv', 'lv', 'LV:lv']]},
	MX: {name: 'Mexico', editions: [['es', 'es-419', 'MX:es-419']]},
	MY: {name: 'Malaysia', editions: [['en', 'en-MY', 'MY:en']]},
	NG: {name: 'Nigeria', editions: [['en', 'en-NG', 'NG:en']]},
	NL: {name: 'Netherlands', editions: [['nl', 'nl', 'NL:nl']]},
	NO: {name: 'Norway', editions: [['no', 'no', 'NO:no']]},
	NZ: {name: 'New Zealand', editions: [['en', 'en-NZ', 'NZ:en']]},
	PE: {name: 'Peru', editions: [['es', 'es-419', 'PE:es-419']]},
	PH: {name: 'Philippines', editions: [['en', 'en-PH', 'PH:en']]},
	PL: {name: 'Poland', editions: [['pl', 'pl', 'PL:pl']]},
	PT: {name: 'Portugal', editions: [['pt', 'pt-PT', 'PT:pt-150']]},
	RO: {name: 'Romania', editions: [['ro', 'ro', 'RO:ro']]},
	RS: {name: 'Serbia', editions: [['sr', 'sr', 'RS:sr']]},
	RU: {name: 'Russia', editions: [['ru', 'ru', 'RU:ru']]},
	SA: {name: 'Saudi Arabia', editions: [['ar', 'ar', 'SA:ar']]},
	AE: {name: 'United Arab Emirates', editions: [['ar', 'ar', 'AE:ar']]},
	SE: {name: 'Sweden', editions: [['sv', 'sv', 'SE:sv']]},
	SG: {name: 'Singapore', editions: [['en', 'en-SG', 'SG:en']]},
	SI: {name: 'Slovenia', editions: [['sl', 'sl', 'SI:sl']]},
	SK: {name: 'Slovakia', editions: [['sk', 'sk', 'SK:sk']]},
	TH: {name: 'Thailand', editions: [['th', 'th', 'TH:th']]},
	TR: {name: 'Türkiye', editions: [['tr', 'tr', 'TR:tr']]},
	TW: {name: 'Taiwan', editions: [['zh', 'zh-TW', 'TW:zh-Hant']]},
	UA: {name: 'Ukraine', editions: [['uk', 'uk', 'UA:uk'], ['ru', 'ru', 'UA:ru']]},
	US: {name: 'United States', editions: [['en', 'en-US', 'US:en'], ['es', 'es-419', 'US:es-419']]},
	VN: {name: 'Vietnam', editions: [['vi', 'vi', 'VN:vi']]},
	ZA: {name: 'South Africa', editions: [['en', 'en-ZA', 'ZA:en']]}
};

export const COUNTRIES: readonly string[] = Object.keys(EDITIONS).sort();

/** The country of a language tag ("pt-BR" → BR) when Google News has it, else the United States. */
export function countryOfTag (tag: string): string {
	const m = /[-_]([A-Za-z]{2})\b/.exec(String(tag || ''));
	const code = m ? m[1].toUpperCase() : '';
	return EDITIONS[code] ? code : 'US';
}

/** The country in use: the chosen one, or the one of the TV's language tag. */
export function countryOf (chosen: string, tag: string): string {
	return EDITIONS[chosen] ? chosen : countryOfTag(tag);
}

/** The country's name in the screen's language (the English one where the TV cannot name it). */
export function countryName (code: string, langTag: string): string {
	try {
		const Names = (Intl as unknown as {DisplayNames?: new (l: string[], o: {type: string}) => {of: (c: string) => string | undefined}}).DisplayNames;
		if (Names) {
			const name = new Names([langTag], {type: 'region'}).of(code);
			if (name && name !== code) return name;
		}
	} catch { /* an older TV */ }
	return EDITIONS[code] ? EDITIONS[code].name : code;
}

/** The edition of a country for the launcher's language: in that language when there is one, else the country's own. */
export function editionFor (country: string, lang: Lang): Edition {
	const c = EDITIONS[country] || EDITIONS.US;
	return c.editions.find((e) => e[0] === lang) || c.editions[0];
}

/** The Google News feed of a country's top stories. The colon of ceid is written %3A: the TV's shell takes no other. */
export function googleNewsUrl (country: string, lang: Lang): string {
	const code = EDITIONS[country] ? country : 'US';
	const e = editionFor(code, lang);
	return `https://news.google.com/rss?hl=${e[1]}&gl=${code}&ceid=${e[2].replace(':', '%3A')}`;
}

/** The address as it is fetched: "feed://" is https; spaces from typing are dropped. */
export function normalizeFeedUrl (raw: string): string {
	return raw.replace(/\s+/g, '').replace(/^feed:\/\//i, 'https://').replace(/^feed:/i, '');
}

export function validFeedUrl (url: string): boolean {
	return /^https?:\/\/[^\s'"`<>\\]+$/i.test(url) && url.length <= 600;
}

// ----- parsing -----

function inner (block: string, names: string[]): string | null {
	for (const name of names) {
		const re = new RegExp('<' + name + '(?:\\s[^>]*)?>([\\s\\S]*?)</' + name + '>', 'i');
		const m = re.exec(block);
		if (m) return m[1];
	}
	return null;
}

/** The text inside an element: CDATA unwrapped, escaped HTML (a description) turned to plain text. */
function textOf (raw: string | null): string {
	if (!raw) return '';
	let s = raw.replace(/<!\[CDATA\[([\s\S]*?)\]\]>/g, '$1');
	// a feed escapes the HTML of its descriptions: decode once, then read the tags
	if (/&lt;|&#60;/.test(s) && !/<[a-z]/i.test(s)) s = decodeEntities(s);
	return htmlToText(s).replace(/\s*\n\s*/g, ' ').replace(/\s+/g, ' ').trim();
}

function linkOf (block: string): string {
	const rss = inner(block, ['link']);
	if (rss && rss.trim() && !/^\s*</.test(rss)) return decodeEntities(rss.replace(/<!\[CDATA\[([\s\S]*?)\]\]>/g, '$1').trim());
	// Atom: <link rel="alternate" href="…"/> (or the first link without a rel)
	const links = block.match(/<link\b[^>]*>/gi) || [];
	let best = '';
	for (const l of links) {
		const href = /\bhref\s*=\s*["']([^"']+)["']/i.exec(l);
		if (!href) continue;
		const rel = /\brel\s*=\s*["']([^"']+)["']/i.exec(l);
		if (!rel || rel[1] === 'alternate') return decodeEntities(href[1]);
		if (!best) best = decodeEntities(href[1]);
	}
	const guid = inner(block, ['guid']);
	return best || (guid && /^https?:/.test(guid.trim()) ? guid.trim() : '');
}

function dateOf (block: string): number {
	const raw = inner(block, ['pubDate', 'dc:date', 'published', 'updated', 'a10:updated']);
	const t = raw ? Date.parse(raw.trim()) : NaN;
	return isFinite(t) ? t : 0;
}

/** The headlines of one feed (RSS 2.0, RSS 1.0 or Atom). */
export function parseFeed (xml: string, feed: number): FeedItem[] {
	const start = xml.search(/<(item|entry)[\s>]/i);
	const head = start === -1 ? xml : xml.slice(0, start);
	const channel = textOf(inner(head, ['title']));
	const blocks = xml.match(/<(item|entry)(?:\s[^>]*)?>[\s\S]*?<\/\1>/gi) || [];
	const out: FeedItem[] = [];
	for (const block of blocks) {
		let title = textOf(inner(block, ['title']));
		if (!title) continue;
		let source = textOf(inner(block, ['source']));
		// Google News writes the outlet after the headline too: "Headline - Outlet"
		if (source && title.length > source.length + 3 && title.slice(-(source.length + 3)) === ' - ' + source) title = title.slice(0, -(source.length + 3));
		if (!source) source = channel;
		const summary = textOf(inner(block, ['description', 'summary', 'content:encoded', 'content']));
		out.push({title, link: linkOf(block), at: dateOf(block), source, summary: summary === title ? '' : summary.slice(0, 600), feed});
	}
	return out;
}

/** Headlines older than this are left out. */
export const NEWS_MAX_AGE_MS = 48 * 3600000;
export const NEWS_MAX_ITEMS = 50;
/** a copy younger than this is not fetched again */
export const NEWS_FRESH_MS = 25 * 60000;

function sameStory (title: string): string {
	return title.toLowerCase().replace(/[\s.,:;!?'"“”‘’«»()\[\]\-–—…|]+/g, ' ').trim();
}

/** All the feeds' headlines in one list: recent ones only (those without a date after them), newest first, no repeats. */
export function joinFeeds (lists: FeedItem[][], now: number): FeedItem[] {
	const seen: Record<string, boolean> = {};
	const all: FeedItem[] = [];
	lists.forEach((list) => list.forEach((item) => {
		if (item.at && (now - item.at > NEWS_MAX_AGE_MS || item.at - now > 3600000)) return;
		const keys = [item.link, sameStory(item.title)].filter((k) => !!k);
		if (keys.some((k) => seen[k])) return;
		keys.forEach((k) => { seen[k] = true; });
		all.push(item);
	}));
	all.sort((a, b) => (b.at || 0) - (a.at || 0));
	return all.slice(0, NEWS_MAX_ITEMS);
}

// ----- the card -----

/** How many headlines fit on a page of a card `height` px tall at text size `k` (two lines each in a column 600 px or narrower). */
export function headlinesPerPage (height: number, k: number): number {
	const head = 44 * k;
	const item = 16 + 15 * k * 1.35 + 6 + 2 * 22 * k * 1.3 + 14;
	return Math.max(1, Math.floor((height - 44 - head - 10) / item));
}

/** "12 min ago"-style age of a headline: minutes under an hour, then hours (the words come from the caller, translated). */
export function ageOf (at: number, now: number): {n: number; unit: 'min' | 'h' | 'd'} | null {
	if (!at) return null;
	const minutes = Math.max(1, Math.round((now - at) / 60000));
	if (minutes < 60) return {n: minutes, unit: 'min'};
	if (minutes < 48 * 60) return {n: Math.round(minutes / 60), unit: 'h'};
	return {n: Math.round(minutes / 1440), unit: 'd'};
}

// ----- the saved copy -----

const NEWS_KEY = 'simplelauncher-news';

export function readNewsCache (): NewsData | null {
	try {
		const d = JSON.parse(localStorage.getItem(NEWS_KEY) || 'null') as NewsData | null;
		return d && typeof d.key === 'string' && Array.isArray(d.items) ? d : null;
	} catch { return null; }
}

export function writeNewsCache (d: NewsData): void {
	try { localStorage.setItem(NEWS_KEY, JSON.stringify(d)); } catch { /* fetched again */ }
}

/** The saved copy when it is of these sources. */
export function usableNews (d: NewsData | null, key: string): NewsData | null {
	return d && d.key === key ? d : null;
}

export function newsIsFresh (d: NewsData | null, key: string, now: number): boolean {
	return !!d && d.key === key && now - d.at < NEWS_FRESH_MS;
}
