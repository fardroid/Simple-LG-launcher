// Tests for the news feeds (src/lib/feeds.ts, src/lib/webfetch.ts), on the made-up feeds in scripts/fixtures/feeds.   Run: npm test
import fs from 'fs';
import {build} from 'esbuild';
const root = new URL('..', import.meta.url).pathname;
const dir = root + 'node_modules/';
await build({entryPoints: [root + 'src/lib/feeds.ts'], bundle: true, format: 'esm', outfile: dir + '.cache-feeds-test.mjs', platform: 'node', logLevel: 'error'});
await build({define: {'import.meta.glob': '__glob'}, banner: {js: 'const __glob = () => ({});'}, entryPoints: [root + 'src/lib/webfetch.ts'], bundle: true, format: 'esm', outfile: dir + '.cache-webfetch-test.mjs', platform: 'node', logLevel: 'error'});
const f = await import(dir + '.cache-feeds-test.mjs');
const w = await import(dir + '.cache-webfetch-test.mjs');
const F = root + 'scripts/fixtures/feeds/';
const read = (name) => fs.readFileSync(F + name, 'utf8');
let fail = 0;
const eq = (a, b, m) => { const ok = JSON.stringify(a) === JSON.stringify(b); if (!ok) { fail++; console.log('FAIL', m, JSON.stringify(a), '!=', JSON.stringify(b)); } else console.log('ok  ', m); };

// Google News
const g = f.parseFeed(read('google.xml'), 0);
eq(g.length, 3, 'Google News: every item');
eq([g[0].title, g[0].source], ['Il comune apre tre nuove piste ciclabili', 'Giornale di Esempio'], 'the outlet is taken off the end of the headline and kept as the source');
eq([g[0].link, g[0].at], ['https://news.google.com/rss/articles/AAA111?oc=5', Date.parse('Wed, 28 Oct 2026 08:30:00 GMT')], 'link and date');
eq(g[0].summary.indexOf('<') === -1 && g[0].summary.indexOf('Biciclette, la mappa dei percorsi') !== -1, true, 'the escaped HTML of the description becomes plain text');
eq(g[1].summary, '', 'a description that only repeats the headline is dropped');

// RSS with CDATA, Atom, RSS 1.0
const r = f.parseFeed(read('rss-cdata.xml'), 1);
eq([r[0].title, r[0].summary, r[0].source], ['Rail operator adds night trains & sleeper cars', 'New routes start in December.', 'Example News - World'], 'CDATA unwrapped, the channel names items without a source');
eq(r[2].at, Date.parse('2026-10-28T06:15:00Z'), 'dc:date is read');
const a = f.parseFeed(read('atom.xml'), 2);
eq([a[0].title, a[0].link, a[0].at], ['New rules for e-scooters & bikes from Monday', 'https://tech.example/scooters', Date.parse('2026-10-28T07:20:00Z')], 'Atom: the alternate link (not the comments), published before updated');
eq([a[1].link, a[1].at, a[1].summary], ['https://tech.example/undated', 0, 'Body text.'], 'Atom: a link without rel, no date, an XHTML body');
const rdf = f.parseFeed(read('rdf.xml'), 3);
eq([rdf.length, rdf[0].title, rdf[0].source], [1, 'Tram line 14 diverted until Sunday', 'Old Example'], 'RSS 1.0 (RDF)');
eq(f.parseFeed('not a feed', 0), [], 'garbage gives nothing');

// another encoding
const bytes = new Uint8Array(fs.readFileSync(F + 'cp1251.xml'));
eq(w.charsetOf(bytes, ''), 'windows-1251', 'the encoding is read from the XML declaration');
eq(w.charsetOf(bytes, 'text/xml; charset=UTF-8'), 'utf-8', 'the Content-Type wins over the declaration');
const ru = f.parseFeed(w.decodeText({bytes, type: ''}), 0);
eq(ru[0].title, 'Трамвай пойдёт по новому маршруту', 'a windows-1251 feed reads right');
eq(w.decodeText({bytes: new TextEncoder().encode('ok'), type: 'text/plain; charset=x-unknown'}), 'ok', 'an encoding the TV does not know falls back to UTF-8');

// joining
const now = Date.parse('2026-10-28T09:00:00Z');
const all = f.joinFeeds([g, r, a, rdf], now);
eq(all.some((i) => i.title.indexOf('Vecchia notizia') === 0), false, 'headlines older than two days are left out');
eq(all.filter((i) => i.title === 'Il comune apre tre nuove piste ciclabili').length, 1, 'the same story from two feeds shows once');
eq(all.map((i) => i.at).slice(0, -1).every((t, i, list) => i === 0 || list[i - 1] >= t), true, 'newest first');
eq(all[all.length - 1].title, 'A headline without a date', 'headlines without a date come last');
eq(f.joinFeeds([[{title: 'Future', link: 'x', at: now + 5 * 3600000, source: '', summary: '', feed: 0}]], now), [], 'a date far in the future is not believed');

// Google News addresses
eq(f.googleNewsUrl('IT', 'ru'), 'https://news.google.com/rss?hl=it&gl=IT&ceid=IT%3Ait', 'Italy in Russian: Italy has no Russian edition, so the Italian one');
eq(f.googleNewsUrl('UA', 'ru'), 'https://news.google.com/rss?hl=ru&gl=UA&ceid=UA%3Aru', 'Ukraine in Russian: its Russian edition');
eq(f.googleNewsUrl('UA', 'en'), 'https://news.google.com/rss?hl=uk&gl=UA&ceid=UA%3Auk', 'Ukraine in English: its first edition');
eq(f.googleNewsUrl('BR', 'pt'), 'https://news.google.com/rss?hl=pt-BR&gl=BR&ceid=BR%3Apt-419', 'Brazil in Portuguese');
eq(f.googleNewsUrl('XX', 'en'), 'https://news.google.com/rss?hl=en-US&gl=US&ceid=US%3Aen', 'an unknown country is the United States');
eq(/^https:\/\/[A-Za-z0-9._\/?=&,%-]+$/.test(f.googleNewsUrl('TW', 'zh')), true, 'the address passes the shell\'s check of safe characters');
eq([f.countryOfTag('pt-BR'), f.countryOfTag('ru'), f.countryOfTag('en_GB'), f.countryOfTag('xx-ZZ')], ['BR', 'US', 'GB', 'US'], 'the country of the TV\'s language tag');
eq([f.countryOf('DE', 'ru-RU'), f.countryOf('', 'ru-RU')], ['DE', 'RU'], 'the chosen country, or the TV\'s');
eq(f.COUNTRIES.indexOf('AT'), -1, 'countries whose edition gives no headlines are not offered');
eq(f.countryName('IT', 'xx'), f.countryName('IT', 'xx') ? f.countryName('IT', 'xx') : 'Italy', 'a country has a name');

// addresses typed in
eq([f.normalizeFeedUrl(' feed://site.example/rss '), f.normalizeFeedUrl('feed:https://site.example/rss')], ['https://site.example/rss', 'https://site.example/rss'], 'feed:// and spaces');
eq([f.validFeedUrl('https://site.example/rss.xml'), f.validFeedUrl('http://192.168.1.2:8080/feed'), f.validFeedUrl('javascript:alert(1)'), f.validFeedUrl("https://a.example/'x")], [true, true, false, false], 'only web addresses');
eq([w.fetchableUrl('https://a.example/x?y=1&z=2'), w.fetchableUrl('https://a.example/$(reboot)'), w.fetchableUrl('https://a.example/a b')], [true, true, false], 'the shell gets any address without spaces or quotes (it is quoted there)');
eq(w.shellSaveCommand("https://a.example/$(x)", '/tmp/f', 100, 5).indexOf("'https://a.example/$(x)'") !== -1, true, 'the address is quoted for the shell');

// the card
eq([f.headlinesPerPage(686, 1), f.headlinesPerPage(470, 1), f.headlinesPerPage(686, 1.5)], [5, 3, 3], 'headlines per page follow the card\'s height and text size');
eq([f.ageOf(now - 5 * 60000, now), f.ageOf(now - 3 * 3600000, now), f.ageOf(0, now)], [{n: 5, unit: 'min'}, {n: 3, unit: 'h'}, null], 'the age of a headline');

console.log(fail ? `\n${fail} FAILED` : '\nall passed');
process.exit(fail ? 1 : 0);
