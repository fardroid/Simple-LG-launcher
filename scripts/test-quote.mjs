// Tests for the quote of the day (src/lib/quote.ts, src/lib/htmltext.ts), on pages of Wikiquote saved in scripts/fixtures/quote.   Run: npm test
import fs from 'fs';
import {build} from 'esbuild';
const root = new URL('..', import.meta.url).pathname;
const out = root + 'node_modules/.cache-quote-test.mjs';
await build({entryPoints: [root + 'src/lib/quote.ts'], bundle: true, format: 'esm', outfile: out, platform: 'node', logLevel: 'error'});
const q = await import(out);
const F = root + 'scripts/fixtures/quote/';
const read = (name) => fs.readFileSync(F + name, 'utf8');
let fail = 0;
const eq = (a, b, m) => { const ok = JSON.stringify(a) === JSON.stringify(b); if (!ok) { fail++; console.log('FAIL', m, JSON.stringify(a), '!=', JSON.stringify(b)); } else console.log('ok  ', m); };

// the editors' quote of the day on the main pages
const daily = (l) => q.dailyFromPage(read('main-' + l + '.html'), l);
eq(daily('en'), {text: 'I speak about universal evolution and teleological evolution, because I think the process of evolution reflects the wisdom of nature. I see the need for wisdom to become operative. We need to try to put all of these things together in what I call an evolutionary philosophy of our time.', author: 'Jonas Salk'}, 'en: the quote and its author under the heading');
eq(daily('es'), {text: 'Mis ventanas ya no dan a los álamos y los ríos de España', author: 'Rafael Alberti'}, 'es: quote marks drawn as pictures are left out');
eq(daily('pl'), {text: 'Dla chcącego nie ma nic trudnego.', author: 'Erazm z Rotterdamu'}, 'pl: the author on the next line without a dash');
eq(daily('zh').author, '歌德', 'zh: the author after —— at the end of the quote');
eq(daily('zh').text.slice(-4), '人宣傳。', 'zh: the dash and the author are cut from the quote');
eq(daily('ko'), {text: '내일은 믿지 마라. 오늘을 즐겨라.(carpe diem)', author: '호라티우스, 《송시》'}, 'ko');
eq(daily('tr').author, 'Bob Marley', 'tr: the author after a dash on its own line');
eq([daily('pt').author, daily('pt').text.charAt(0)], ['Bill Gates', 'D'], 'pt: " - Author" at the end, the quote marks taken off');
eq(daily('uk'), null, 'uk: a box that says no quote was chosen today gives none');
eq(q.dailyFromFeed(read('feed-it.xml'), 'it'), {text: 'La realtà è ciò che si rifiuta di sparire anche quando smetti di crederci.', author: 'Philip K. Dick'}, 'it: the newest entry of the feed, „ taken off');
eq(q.dailyFromPage('<p>nothing here</p>', 'en'), null, 'a page without the box gives none');
eq(q.dailyFromPage('<h2>Quote of the day</h2><p>Hi.</p><p>— X</p>', 'de'), null, 'a language without an editors\' quote gives none');

// quotes picked from an author's page
const fromPage = (l) => q.quotesFromPage(read('author-' + l + '.html'), l);
const ru = fromPage('ru'), de = fromPage('de'), fr = fromPage('fr'), ja = fromPage('ja');
eq([ru.length > 50, de.length > 200, fr.length > 15, ja.length > 20], [true, true, true, true], 'every page gives quotes (ru ' + ru.length + ', de ' + de.length + ', fr ' + fr.length + ', ja ' + ja.length + ')');
eq(ru.indexOf('Благородный муж — не орудие.') !== -1, true, 'ru: a short saying is kept');
eq(de.some((t) => /^Licht war seine letzte Forderung/.test(t)) || fr.some((t) => /^HUGO \(Victor\)/.test(t)), false, 'quotes about the author (de: "Zitate mit Bezug auf", fr: "Citations le concernant") are left out');
eq(de.some((t) => t.indexOf('\n') !== -1) && de.every((t) => t.indexOf('//') === -1), true, 'de: lines of verse written with // become lines');
eq(de.every((t) => !/–\s*Vers\b/.test(t)), true, 'de: the source after the quote ("– Vers 5") is dropped');
eq(ja.every((t) => t.indexOf('--') === -1 && !/（\s*[ぁ-ゖ]+\s*）/.test(t)), true, 'ja: no source after -- and no readings in brackets');
eq(ja.every((t) => t.length >= 6 && t.length <= 90), true, 'ja: short in characters (no spaces in the language)');
eq([...ru, ...de, ...fr].every((t) => t.length >= 25 && t.length <= 220 && !/https?:|ISBN/.test(t)), true, 'other languages: 25 to 220 characters, no links or book references');
// Russian pages that keep their quotes in boxes of the "Q" template (table rows), with a "см. …" line in a list (Lomonosov)
const box = q.quotesFromPage(read('author-ru-q.html'), 'ru');
eq(box.length > 20, true, 'ru: the quotes in the boxes are read (' + box.length + ')');
eq(box.indexOf('Неусыпный труд все препятствия преодолевает.') !== -1, true, 'ru: a quote from a box');
eq(box.some((t) => /^см\./.test(t) || /Шувалову»/.test(t)), false, 'ru: "см. «К Ивану Ивановичу Шувалову», 1752—1753" points elsewhere: not a quote');
eq(box.some((t) => /^А математику уже затем/.test(t)), false, 'ru: the boxes under "Приписываемое" are left out');
eq(box.some((t) => /^Лучи от нас склонились прочь;/.test(t) && t.indexOf('\n') !== -1), true, 'ru: a poem in a box keeps its lines');
const page = (section, item) => '<h2>' + section + '</h2><ul><li>' + item + '</li></ul>';
eq(q.quotesFromPage(page('Ссылки', 'x') + page('Цитаты', 'Категория:Произведения Альберта Эйнштейна, одна длинная строка'), 'ru'), [], 'ru: a category link is not a quote');
eq([q.skippedSection('Общие источники'), q.skippedSection('Статьи о произведениях'), q.skippedSection('О Льве Толстом'), q.skippedSection('Про Тараса Шевченка'), q.skippedSection('О любви и дружбе')], [true, true, true, true, false], 'ru, uk: sources, articles and quotes about a person are skipped; the author\'s own "О любви" is not');
eq(q.dropSource('Поступать несправедливо хуже, чем терпеть несправедливость. — «Горгий», 473a'), 'Поступать несправедливо хуже, чем терпеть несправедливость.', 'the source after a dash is dropped');
eq(q.dropSource('Я серьезно убежден, что миром правят совсем сумасшедшие. (Дневник: 12 июля 1900 г.)'), 'Я серьезно убежден, что миром правят совсем сумасшедшие.', 'a source in brackets with a date is dropped');
eq(q.dropSource('— На том стою, и не могу иначе. Аминь. (Защитная речь в Вормсе (18 апреля 1521 года))'), 'На том стою, и не могу иначе. Аминь.', 'a dash before a line of speech, and a source with brackets inside brackets, are dropped');
eq([q.skippedSection('О Достоевском / О произведениях', 'Фёдор Михайлович Достоевский'), q.skippedSection('О России', 'Фёдор Михайлович Достоевский'), q.skippedSection('О Достоевском')], [true, false, false], 'a heading about the author (any case ending) is skipped; one on a theme is not; without the author\'s name it cannot be told');
eq(q.quotesFromPage(page('Цитаты', 'Основная категория: христианин по обязанности, ну и так далее,'), 'ru'), [], 'a line cut off mid-sentence is not a quote');
eq(q.dropSource('Кто ясно мыслит — ясно излагает.'), 'Кто ясно мыслит — ясно излагает.', 'a dash inside the sentence stays');
eq(q.cleanQuote('Я вижу что могло бы быть и спрашиваю «почему нет?»', 'ru'), 'Я вижу что могло бы быть и спрашиваю «почему нет?»', 'a closing » of a quote inside the text stays');

eq(q.skippedSection('Überprüft / Gedichte'), false, '"Überprüft" (checked) is not "über" (about)');
eq([q.skippedSection('Citations le concernant / Sainte-Beuve'), q.skippedSection('孔子に関する引用'), q.skippedSection('See also')], [true, true, true], 'about-the-author and link sections are skipped at any depth');

// every language of the launcher: the editors' quote where there is one, else picked by the date (with saved pages to test on)
const langs = ['en', 'ru', 'es', 'fr', 'de', 'it', 'pt', 'tr', 'uk', 'zh', 'ja', 'ko', 'pl'];
const pickedOnly = langs.filter((l) => !q.DAILY[l]);
eq(pickedOnly, ['ru', 'fr', 'de', 'ja'], 'ru, fr, de and ja always pick by the date; the other nine have an editors\' quote');
eq(pickedOnly.filter((l) => !fs.existsSync(F + 'author-' + l + '.html')), [], 'each language that picks by the date has an author\'s page to test on');
eq(langs.filter((l) => q.DAILY[l] && !q.DAILY[l].feed && !fs.existsSync(F + 'main-' + l + '.html')), [], 'each language with an editors\' quote on its main page has that page to test on');

// cleaning
eq(q.cleanQuote('„Edel sei der Mensch, // Hülfreich und gut!“ – Vers 1–6', 'de'), 'Edel sei der Mensch,\nHülfreich und gut!', 'quote marks, the source after them and // handled');
eq(q.cleanQuote('«Texte entre guillemets»', 'fr'), 'Texte entre guillemets', 'French quote marks');
eq(q.cleanQuote('A sentence.[1]', 'en'), 'A sentence.', 'note numbers dropped');
eq(q.authorName('Confucius (philosopher)'), 'Confucius', 'the author without the disambiguation in brackets');

// picking by the date
const authors = q.parseAuthors(read('authors-ru.json'));
eq([authors.length, authors[0], authors.indexOf('Конфуций') !== -1], [30, 'Уильям Шекспир', true], 'the Wikidata reply gives the page titles, best known first');
eq(q.parseAuthors('nope'), [], 'a broken reply gives none');
const day = new Date(2026, 9, 28, 9, 0).getTime(), later = new Date(2026, 9, 28, 23, 50).getTime(), next = new Date(2026, 9, 29, 0, 10).getTime();
eq(q.pickAuthor(authors, day), q.pickAuthor(authors, later), 'the same author all day');
eq(q.pickAuthor(authors, day) !== q.pickAuthor(authors, next), true, 'another author the next day');
eq(q.pickAuthor(authors, day, 1) !== q.pickAuthor(authors, day, 0), true, 'the next try picks another author');
eq(q.pickQuote(ru, day), q.pickQuote(ru, later), 'the same quote all day');
eq([q.pickAuthor([], day), q.pickQuote([], day)], [null, null], 'nothing to pick from');
eq(q.dayKey(day), '2026-10-28', 'the local day');
eq(q.authorsQuery('ja').indexOf('<https://ja.wikiquote.org/>') !== -1, true, 'the query asks for the language\'s Wikiquote');
eq(q.parseUrl('ru', 'Конфуций').indexOf('origin=*') !== -1 && q.parseUrl('ru', 'Конфуций').indexOf('page=%D0%9A') !== -1, true, 'the page address lets the web view read it and encodes the title');

// the saved copy
const store = {};
globalThis.localStorage = {getItem: (k) => (k in store ? store[k] : null), setItem: (k, v) => { store[k] = String(v); }};
const quote = {text: 'T', author: 'A', lang: 'ru', day: '2026-10-28', kind: 'picked'};
q.writeQuoteCache(quote);
eq([q.todaysQuote(q.readQuoteCache(), 'ru', day), q.todaysQuote(q.readQuoteCache(), 'ru', next), q.todaysQuote(q.readQuoteCache(), 'en', day)], [quote, null, null], 'the saved quote counts for its own day and language');
eq([q.shownQuote(q.readQuoteCache(), 'ru'), q.shownQuote(q.readQuoteCache(), 'en')], [quote, null], 'yesterday\'s stays on screen until today\'s is in, in the same language');
q.writeAuthors('ru', ['X', 'Y'], day);
eq([q.readAuthors('ru', day + 86400000), q.readAuthors('ru', day + 31 * 86400000), q.readAuthors('de', day)], [['X', 'Y'], null, null], 'the list of authors is kept for a month, per language');

console.log(fail ? `\n${fail} FAILED` : '\nall passed');
process.exit(fail ? 1 : 0);
