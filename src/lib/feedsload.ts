// Fetching the news feeds (see feeds.ts), one after another. A feed that cannot be read is left out; it is an error only when
// none can. Most news sites send no CORS headers, so on a TV the feeds come through the Homebrew Channel's shell.

import {joinFeeds, parseFeed, type FeedItem, type NewsData} from './feeds';
import {fetchTextOf} from './webfetch';

export function fetchNews (sources: string[], key: string, allowShell: boolean): Promise<NewsData> {
	const now = Date.now();
	const lists: FeedItem[][] = [];
	let failed = 0;
	const one = (url: string, index: number): Promise<void> => fetchTextOf(url, allowShell, {timeoutMs: 15000}).then(
		(text) => { const items = parseFeed(text, index); if (items.length) lists.push(items); else failed++; },
		() => { failed++; }
	);
	return sources.reduce((chain, url, i) => chain.then(() => one(url, i)), Promise.resolve()).then(() => {
		if (!lists.length) throw new Error(failed ? 'nothing could be read' : 'no feeds');
		return {key, at: now, items: joinFeeds(lists, now)};
	});
}
