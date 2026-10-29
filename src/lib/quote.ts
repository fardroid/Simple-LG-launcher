// The quote of the day, in the launcher's language, from Wikiquote. Where the language's Wikiquote has an editors' quote of the day
// on its main page (or, in Italian, a feed of it), that one is shown. Elsewhere, and on a day the editors left empty, a quote is
// picked by the date: an author from a list of about 400 well-known writers, poets, thinkers and scientists born before 1920 who
// have a page there (asked of Wikidata once a month), then one quote from that author's page. The whole day shows the same quote.
// This file is the pure part (the page rules, the picking, the saved copy); quoteload.ts does the fetching.

import {htmlLines, textOfClass, topLevelItems, decodeEntities, type ListItem} from './htmltext';
import type {Lang} from './i18n';

export interface Quote {
	text: string;
	author: string;
	lang: Lang;
	/** the local day it is for, YYYY-MM-DD */
	day: string;
	/** 'daily': the editors' quote of the day; 'picked': picked by the date */
	kind: 'daily' | 'picked';
	/** the rules it was picked by (QUOTE_RULES) */
	rules?: number;
}

/** Goes up when the way quotes are read from the pages changes: a quote saved by older rules is fetched again
 *  (2: the quote boxes of Russian and Ukrainian pages, without "см. …" lines). */
export const QUOTE_RULES = 2;

interface Daily {
	/** the main page, or the feed of the quote of the day */
	page?: string;
	feed?: boolean;
	/** the heading of the quote's box on the main page */
	marker?: string;
	/** a box that says no quote was chosen for today */
	empty?: RegExp;
}

/** Where each language's quote of the day is. Languages without one always pick by the date. */
export const DAILY: Partial<Record<Lang, Daily>> = {
	en: {page: 'Main Page', marker: 'Quote of the day'},
	es: {page: 'Portada', marker: 'Cita del día'},
	pl: {page: 'Strona główna', marker: 'Cytat dnia'},
	zh: {page: 'Wikiquote:首页', marker: '每日名言'},
	ko: {page: '위키인용집:대문', marker: '오늘의 명언'},
	tr: {page: 'Ana Sayfa', marker: 'Günün Sözü'},
	pt: {page: 'Página principal', marker: 'Citação do Dia'},
	uk: {page: 'Головна сторінка', marker: 'Цитата дня', empty: /ще не обрано/},
	it: {feed: true}
};

/** Languages written without spaces, where a quote is short in characters. */
const DENSE: readonly Lang[] = ['zh', 'ja'];

/** Headings of the parts of an author's page that hold quotes about the author, doubtful or wrongly attributed ones, links or
 *  sources, in any of the languages. Each heading an item is under is tested on its own. */
const SKIP_SECTION = new RegExp('^(' + [
	'quotes? about', 'about ', 'misattributed', 'disputed', 'attributed', 'see also', 'external links', 'sources', 'references',
	'о нём', 'о ней', 'об ', 'высказывания о', 'цитаты о', 'сомнительн', 'ошибочно', 'приписыва', 'см\\. также', 'ссылки', 'источники', 'литература',
	'общие источники', 'статьи о',
	'zitate über', 'über ', 'zitate mit bezug', 'fälschlich', 'zugeschrieben', 'unbelegt', 'siehe auch', 'weblinks', 'quellen', 'literatur',
	'citations le concernant', 'citations sur', 'à propos', 'attribu', 'apocryph', 'voir aussi', 'liens', 'références', 'bibliographie',
	'.*について', '.*に関する', '.*誤って', '関連', '外部リンク', '出典', '参考',
	'про ', 'цитати про', 'сумнівн', 'приписуван', 'див\\. також', 'посилання', 'джерела', 'загальні джерела', 'статті про',
	'sobre ', 'citas sobre', 'atribuid', 'véase', 'enlaces', 'fuentes', 'veja também', 'ligações', 'citações sobre',
	'.*hakkında', 'ayrıca', 'kaynak', 'o nim', 'o niej', 'przypisywane', 'zobacz też', 'linki', 'źródła',
	'.*에 대한', '관련', '외부 링크', '.*的名言', '关于', '出处', '相关', '外部链接',
	'su di', 'citazioni su', 'attribuit', 'voci correlate', 'collegamenti', 'fonti'
].join('|') + ')', 'i');

/** "О Льве Толстом", "Про Тараса Шевченка": about a person (two capitalised words), unlike the author's own "О любви и дружбе".
 *  Case matters here, so it is apart from SKIP_SECTION. */
const ABOUT_PERSON = /^(?:О|Об|Про) [А-ЯЁІЇЄҐ]\S* [А-ЯЁІЇЄҐ]/;

/** A heading "about" someone ("О Достоевском", "Про Шевченка", "Über Goethe") that names the author: the first five letters
 *  of a word in it are those of a word of the author's name, whatever its case ending. */
const ABOUT = /^(?:о|об|обо|про|about|über|sur|sobre|su|o)\s+(.+)$/i;
function aboutAuthor (part: string, author: string): boolean {
	const m = ABOUT.exec(part);
	if (!m || !author) return false;
	const stems = author.toLowerCase().split(/[\s,.-]+/).filter((w) => w.length >= 5).map((w) => w.slice(0, 5));
	return m[1].toLowerCase().split(/[\s,.-]+/).some((w) => stems.indexOf(w.slice(0, 5)) !== -1);
}

/** The item lies in a part of the page to leave out (`author`: the page's author, for the headings about them). */
export function skippedSection (section: string, author: string = ''): boolean {
	return section.split(' / ').some((part) => SKIP_SECTION.test(part.trim()) || ABOUT_PERSON.test(part.trim()) || aboutAuthor(part.trim(), author));
}

export const WIKIQUOTE_API = (lang: Lang): string => `https://${lang}.wikiquote.org/w/api.php`;

/** The address of a page's HTML (origin=* lets the web view read the reply). */
export function parseUrl (lang: Lang, page: string): string {
	return `${WIKIQUOTE_API(lang)}?action=parse&format=json&formatversion=2&prop=text&redirects=1&origin=*&page=${encodeURIComponent(page)}`;
}

/** The address of the Italian quote-of-the-day feed. */
export function feedUrl (lang: Lang): string {
	return `${WIKIQUOTE_API(lang)}?action=featuredfeed&feed=qotd&feedformat=atom&origin=*`;
}

/** The HTML out of an action=parse reply (null when it is not one). */
export function parseReply (text: string): string | null {
	try {
		const data = JSON.parse(text) as {parse?: {text?: string}};
		return data && data.parse && typeof data.parse.text === 'string' ? data.parse.text : null;
	} catch { return null; }
}

/** The local day, YYYY-MM-DD. */
export function dayKey (now: number): string {
	const d = new Date(now);
	const pad = (n: number) => (n < 10 ? '0' : '') + n;
	return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())}`;
}

/** A number that is the same all day and different every day. */
export function dayNumber (now: number): number {
	const d = new Date(now);
	return Math.floor(Date.UTC(d.getFullYear(), d.getMonth(), d.getDate()) / 86400000);
}

const MARKS = /^[\s“”"„«»‘’‚‹›'「」『』〝〞⹂]*$/;
const DASH_TAIL = /\s*(?:——|—|–|~|\s-)\s*([^—–~\n]{2,80})$/;

/** Quote marks around the whole text taken off; a source after the closing mark ("…" – Vers 5) dropped. */
export function cleanQuote (raw: string, lang: Lang): string {
	let text = raw.replace(/\s*\/\/\s*/g, '\n').replace(/\[\d+\]/g, '').replace(/[ \t]+/g, ' ').trim();
	if (lang === 'ja') text = text.replace(/\s*[（(]\s*[ぁ-ゖー\s]+\s*[）)]/g, '');
	text = text.replace(/\s*(?:--|——)[^\n]{0,40}$/, '');
	const quoted = /^[„"“«‚‹「『]([\s\S]+?)[“"”»‘›„」』]\s*(?:[–—-]\s*[^\n]{0,80})?$/.exec(text);
	if (quoted) text = quoted[1].trim();
	text = text.replace(/^[„"“«「『]+|[“"”»„」』]+$/g, '').trim();
	// a mark taken off the end that closed a quote inside the text ("… спрашиваю «почему нет?»") goes back
	const count = (re: RegExp) => (text.match(re) || []).length;
	if (count(/«/g) > count(/»/g)) text += '»';
	if (count(/「/g) > count(/」/g)) text += '」';
	return text;
}

/** A line that only points elsewhere ("см. «К Шувалову», 1752", "see also …", a category link), not a quote. */
const CROSS_REF = /^(?:см\.|смотри|див\.|дивись|see |cf\.|vgl\.|siehe |voir |véase |vedi |zob\.|категория:|категорія:|category:|kategorie:|catégorie:)/i;

/** Where a quote on an author's page is followed by its source or a remark ("… меньшее. — «Протагор», 358d",
 *  "… сумасшедшие. (Дневник: 12 июля 1900 г.)"): the end of the sentence, then a dash, or a bracket (one more may be inside it)
 *  with a date or a number in it. */
const DASH_SOURCE = /([.!?…»])\s+—\s+[^\n]+$/;
const BRACKET_SOURCE = /([.!?…»])\s+(\((?:[^()\n]|\([^()\n]*\))*\))$/;

/** The quote without the source after it, and without a dash before it (a line of speech). */
export function dropSource (text: string): string {
	const bracket = BRACKET_SOURCE.exec(text);
	if (bracket && /\d/.test(bracket[2])) text = text.slice(0, bracket.index + 1);
	return text.replace(DASH_SOURCE, '$1').replace(/^—\s+/, '');
}

function fits (text: string, lang: Lang): boolean {
	const dense = DENSE.indexOf(lang) !== -1;
	const n = text.length;
	if (dense ? n < 6 || n > 90 : n < 25 || n > 220) return false;
	if (CROSS_REF.test(text) || /[,:;]$/.test(text)) return false;      // a pointer elsewhere, or a line cut off mid-sentence
	return !/https?:|ISBN|\bp\.\s?\d|\bS\.\s?\d|\bed\.|éd\.|trad\.|\(\d{4}\)|^\d{4}\b/i.test(text);
}

/** The author's name from a page title: "Confucius (philosopher)" → "Confucius". */
export function authorName (title: string): string {
	return title.replace(/\s*\([^)]*\)\s*$/, '').trim();
}

/** The editors' quote out of a main page's box: the first line of text after the heading, and the author after it (or after a
 *  dash at the end of the quote). Null when there is none today. */
export function dailyFromLines (lines: string[], daily: Daily, lang: Lang): {text: string; author: string} | null {
	let i = 0;
	if (daily.marker) {
		const marker = daily.marker.toLowerCase();
		i = lines.findIndex((l) => l.toLowerCase() === marker || (l.toLowerCase().indexOf(marker) === 0 && l.length <= marker.length + 3));
		if (i === -1) return null;
		i++;
	}
	const next = () => {
		while (i < lines.length && (MARKS.test(lines[i]) || /^\.mw-parser-output/.test(lines[i]))) i++;
		return i < lines.length ? lines[i++] : '';
	};
	let text = next();
	if (!text || (daily.empty && daily.empty.test(text))) return null;
	let author = '';
	const tail = DASH_TAIL.exec(text);
	if (tail && text.length - tail[0].length > 3) {
		author = tail[1].trim();
		text = text.slice(0, text.length - tail[0].length);
	} else {
		const line = next();
		author = line.replace(/^[\s—–~-]+/, '').trim();
	}
	text = cleanQuote(text, lang);
	if (text.length < 3 || text.length > 600 || author.length > 120 || !author) return null;
	return {text, author};
}

/** The editors' quote of the day from a main page's HTML. */
export function dailyFromPage (html: string, lang: Lang): {text: string; author: string} | null {
	const daily = DAILY[lang];
	return daily ? dailyFromLines(htmlLines(html, true), daily, lang) : null;
}

/** The newest entry of the Italian quote-of-the-day feed. */
export function dailyFromFeed (xml: string, lang: Lang): {text: string; author: string} | null {
	const entries = xml.split(/<entry>/i);
	const last = entries[entries.length - 1];
	const summary = /<summary[^>]*>([\s\S]*?)<\/summary>/i.exec(last) || /<content[^>]*>([\s\S]*?)<\/content>/i.exec(last);
	if (entries.length < 2 || !summary) return null;
	return dailyFromLines(htmlLines(decodeEntities(summary[1]), true), {}, lang);
}

/** The quotes on an author's page that suit the card: the items of its lists and its quote boxes (French pages keep them in "citation" blocks),
 *  outside sections about the author or of doubtful ones, cleaned and of a fitting length. */
export function quotesFromPage (html: string, lang: Lang, author: string = ''): string[] {
	// Russian and Ukrainian pages keep many quotes in boxes of the "Q" template (tables: the quote in a "poem" block, in Russian in
	// a row of class q-text; the original language under it in a grey italic cell, left out here), not in lists
	const boxes = lang === 'ru' || lang === 'uk';
	const noOriginals = boxes ? html.replace(/<td[^>]*font-style:\s*italic[^>]*>[\s\S]*?<\/td>/gi, '') : '';
	const items: ListItem[] = lang === 'fr' ? textOfClass(html, 'citation')
		: boxes ? topLevelItems(html).concat(textOfClass(html, 'q-text', true), textOfClass(noOriginals, 'poem', true))
		: topLevelItems(html);
	const out: string[] = [];
	items.forEach((item) => {
		if (item.section && skippedSection(item.section, author)) return;
		const text = dropSource(cleanQuote(item.text, lang));
		if (fits(text, lang) && out.indexOf(text) === -1) out.push(text);
	});
	return out;
}

/** The Wikidata query for the authors of a language's Wikiquote: writers, poets, philosophers, scientists and the like, born
 *  before 1920, the best known first (by how many Wikipedias have an article on them). */
export function authorsQuery (lang: Lang): string {
	const jobs = 'wd:Q36180 wd:Q49757 wd:Q4964182 wd:Q6625963 wd:Q214917 wd:Q901 wd:Q169470 wd:Q1028181 wd:Q36834';
	return 'SELECT DISTINCT ?title ?n WHERE { ?a schema:isPartOf <https://' + lang + '.wikiquote.org/>; schema:about ?p; schema:name ?title. ' +
		'?p wdt:P31 wd:Q5; wikibase:sitelinks ?n; wdt:P569 ?born; wdt:P106 ?job. VALUES ?job { ' + jobs + ' } FILTER(YEAR(?born) < 1920) } ORDER BY DESC(?n) LIMIT 400';
}

export function authorsUrl (lang: Lang): string {
	return 'https://query.wikidata.org/sparql?format=json&query=' + encodeURIComponent(authorsQuery(lang));
}

/** The page titles out of the Wikidata reply. */
export function parseAuthors (text: string): string[] {
	try {
		const data = JSON.parse(text) as {results?: {bindings?: Array<{title?: {value?: string}}>}};
		const rows = data && data.results && data.results.bindings ? data.results.bindings : [];
		const out: string[] = [];
		rows.forEach((r) => { const t = r.title && r.title.value; if (t && out.indexOf(t) === -1) out.push(t); });
		return out;
	} catch { return []; }
}

/** The author for a day: the list is walked in steps of a large prime, so neighbouring days get unrelated authors. `attempt`
 *  moves on to the next one when a page has nothing that fits. */
export function pickAuthor (authors: string[], now: number, attempt: number = 0): string | null {
	if (!authors.length) return null;
	return authors[(dayNumber(now) * 7919 + attempt * 104729) % authors.length];
}

/** The quote of the day among an author's quotes. */
export function pickQuote (quotes: string[], now: number): string | null {
	return quotes.length ? quotes[(dayNumber(now) * 2671) % quotes.length] : null;
}

// ----- the saved copies -----

const QUOTE_KEY = 'simplelauncher-quote';
const AUTHORS_KEY = 'simplelauncher-quote-authors';
/** the list of authors is asked for again after a month */
export const AUTHORS_FRESH_MS = 30 * 86400000;

export function readQuoteCache (): Quote | null {
	try {
		const q = JSON.parse(localStorage.getItem(QUOTE_KEY) || 'null') as Quote | null;
		return q && typeof q.text === 'string' && typeof q.author === 'string' && typeof q.day === 'string' && q.rules === QUOTE_RULES ? q : null;
	} catch { return null; }
}

export function writeQuoteCache (q: Quote): void {
	try { localStorage.setItem(QUOTE_KEY, JSON.stringify(q)); } catch { /* it is fetched again */ }
}

/** The saved quote, when it is today's and in this language. */
export function todaysQuote (q: Quote | null, lang: Lang, now: number): Quote | null {
	return q && q.lang === lang && q.day === dayKey(now) ? q : null;
}

/** The saved quote even when it is from another day (shown until today's is in), as long as the language is right. */
export function shownQuote (q: Quote | null, lang: Lang): Quote | null {
	return q && q.lang === lang ? q : null;
}

export function readAuthors (lang: Lang, now: number): string[] | null {
	try {
		const saved = JSON.parse(localStorage.getItem(AUTHORS_KEY) || 'null') as {lang: Lang; at: number; titles: string[]} | null;
		return saved && saved.lang === lang && now - saved.at < AUTHORS_FRESH_MS && Array.isArray(saved.titles) && saved.titles.length ? saved.titles : null;
	} catch { return null; }
}

export function writeAuthors (lang: Lang, titles: string[], now: number): void {
	try { localStorage.setItem(AUTHORS_KEY, JSON.stringify({lang, at: now, titles})); } catch { /* asked for again */ }
}
