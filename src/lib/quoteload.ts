// Fetching the quote of the day (see quote.ts): the editors' quote where the language has one, otherwise (or when there is none
// today) a quote picked by the date from an author's page. Wikimedia's services allow the web view to read them, so this works
// without the Homebrew Channel too; the shell is only a fallback for TVs that cannot verify the certificates.

import {APP_VERSION} from '../config/constants';
import type {Lang} from './i18n';
import {
	DAILY, authorName, authorsUrl, dailyFromFeed, dailyFromPage, dayKey, feedUrl, parseAuthors, parseReply, parseUrl,
	pickAuthor, pickQuote, QUOTE_RULES, quotesFromPage, readAuthors, writeAuthors, type Quote
} from './quote';
import {fetchTextOf} from './webfetch';

/** Wikimedia asks scripts to say who they are; a web page cannot set User-Agent, so it goes in this header. */
const HEADERS = {'Api-User-Agent': `SimpleLGLauncher/${APP_VERSION} (https://github.com/fardroid/Simple-LG-launcher)`};

function get (url: string, allowShell: boolean): Promise<string> {
	return fetchTextOf(url, allowShell, {timeoutMs: 20000, maxBytes: 3 * 1048576, headers: HEADERS});
}

function daily (lang: Lang, allowShell: boolean): Promise<{text: string; author: string} | null> {
	const d = DAILY[lang];
	if (!d) return Promise.resolve(null);
	if (d.feed) return get(feedUrl(lang), allowShell).then((xml) => dailyFromFeed(xml, lang));
	return get(parseUrl(lang, d.page as string), allowShell).then((text) => {
		const html = parseReply(text);
		return html ? dailyFromPage(html, lang) : null;
	});
}

function authors (lang: Lang, allowShell: boolean, now: number): Promise<string[]> {
	const saved = readAuthors(lang, now);
	if (saved) return Promise.resolve(saved);
	// headers on the Wikidata query would make the web view ask first whether it may send them: kept plain
	return fetchTextOf(authorsUrl(lang), allowShell, {timeoutMs: 60000}).then((text) => {
		const list = parseAuthors(text);
		if (!list.length) throw new Error('no authors');
		writeAuthors(lang, list, now);
		return list;
	});
}

/** A quote from the day's author; the next author when a page has nothing that fits (three tries). */
function picked (lang: Lang, allowShell: boolean, now: number): Promise<{text: string; author: string}> {
	return authors(lang, allowShell, now).then((list) => {
		const attempt = (n: number): Promise<{text: string; author: string}> => {
			const title = pickAuthor(list, now, n);
			if (!title || n >= 3) return Promise.reject(new Error('nothing to quote'));
			return get(parseUrl(lang, title), allowShell).then((text) => {
				const html = parseReply(text);
				const quote = html ? pickQuote(quotesFromPage(html, lang, authorName(title)), now) : null;
				return quote ? {text: quote, author: authorName(title)} : attempt(n + 1);
			}, () => attempt(n + 1));
		};
		return attempt(0);
	});
}

/** Today's quote in `lang`. */
export function fetchQuote (lang: Lang, allowShell: boolean, now: number): Promise<Quote> {
	const day = dayKey(now);
	return daily(lang, allowShell).catch(() => null).then((found) => {
		if (found) return {text: found.text, author: found.author, lang, day, kind: 'daily' as const, rules: QUOTE_RULES};
		return picked(lang, allowShell, now).then((p) => ({text: p.text, author: p.author, lang, day, kind: 'picked' as const, rules: QUOTE_RULES}));
	});
}
