// Tests for where the cards stand (src/lib/boardlayout.ts).   Run: npm test
import {build} from 'esbuild';
const out = new URL('../node_modules/.cache-boardlayout-test.mjs', import.meta.url).pathname;
await build({entryPoints: [new URL('../src/lib/boardlayout.ts', import.meta.url).pathname], bundle: true, format: 'esm', outfile: out, platform: 'node', logLevel: 'error'});
const b = await import(out);
let fail = 0;
const eq = (a, bb, msg) => { const ok = JSON.stringify(a) === JSON.stringify(bb); if (!ok) { fail++; console.log('FAIL', msg, JSON.stringify(a), '!=', JSON.stringify(bb)); } else console.log('ok  ', msg); };
// 80 % icons, bottom margin 50, names shown, one row: tiles 120 px, names 62 px -> the row starts at 848, the selected icon (x 1.12)
// reaches up to 848 - 14.4 = 833.6; the block is 686 tall, so (833.6 - 686) / 2 = 73.8 is left above and below it
eq(b.BOARD_HEIGHT, 686, 'the block keeps its size');
eq(b.boardTop({iconScale: 80, bottomMargin: 50, showLabels: true, rows: 1}, 14), 74, 'one row, 80 % icons: equal space above and below');
eq(b.boardTop({iconScale: 80, bottomMargin: 50, showLabels: false, rows: 1}, 14) > 74, true, 'without the names there is more room');
eq(b.boardTop({iconScale: 80, bottomMargin: 50, showLabels: true, rows: 2}, 14) < 74, true, 'two rows leave less room');
eq(b.boardTop({iconScale: 80, bottomMargin: 50, showLabels: true, rows: 2}, 1), 74, 'two rows with one tile are one row');
eq(b.boardTop({iconScale: 130, bottomMargin: 200, showLabels: true, rows: 2}, 14), 16, 'never closer than 16 px to the top');
eq(b.boardTop({iconScale: 100, bottomMargin: 50, showLabels: true, rows: 1}, 14) < 74, true, 'bigger icons leave less room');

// ----- the cards -----
const clock = {size: 160, showDate: true, dateSize: 100, period: false};
const on = (o) => Object.assign({weather: false, calendar: false, news: false, quote: false, clock}, o);
const rect = (l, id) => { const c = l.cards[id]; return c ? [c.x, c.y, c.w, c.h] : null; };
const ids = (l) => l.columns.map((c) => c.cards.map((card) => card.id).join('+'));

let l = b.boardLayout(on({weather: true, calendar: true}), 40);
eq([rect(l, 'clock'), rect(l, 'weather'), rect(l, 'calendar')], [[200, 40, 880, 184], [200, 264, 880, 462], [1120, 40, 600, 686]], 'weather and calendar: the two columns of before, to the pixel');
l = b.boardLayout(on({weather: true}), 40);
eq([rect(l, 'clock'), rect(l, 'weather'), l.columns.length], [[520, 40, 880, 184], [520, 264, 880, 462], 1], 'without the calendar the left column is centred, as before');
l = b.boardLayout(on({}), 40);
eq(rect(l, 'clock'), [520, 40, 880, 184], 'the clock alone');
eq(rect(b.boardLayout(on({weather: true, calendar: true}), 74), 'calendar')[1], 74, 'the block moves with its top');

l = b.boardLayout(on({weather: true, calendar: true, quote: true}), 40);
eq([rect(l, 'calendar'), rect(l, 'quote'), rect(l, 'weather')], [[1120, 40, 600, 470], [1120, 550, 600, 176], [200, 264, 880, 462]], 'with the quote: the calendar above it, both ending with the weather');
l = b.boardLayout(on({weather: true, calendar: true}), 40);
eq(rect(l, 'calendar')[3], 686, 'no quote: the calendar takes the whole height');

l = b.boardLayout(on({weather: true, news: true}), 40);
eq([ids(l), rect(l, 'news')], [['clock+weather', 'news'], [1120, 40, 600, 686]], 'the news without the calendar: two columns, the news in the calendar\'s place, full height');
l = b.boardLayout(on({weather: true, news: true, quote: true}), 40);
eq([rect(l, 'news'), rect(l, 'quote')], [[1120, 40, 600, 470], [1120, 550, 600, 176]], 'the news with the quote under it');

l = b.boardLayout(on({weather: true, calendar: true, news: true}), 40);
eq([l.narrow, ids(l), l.columns.map((c) => [c.x, c.w])], [true, ['clock+weather', 'calendar', 'news'], [[80, 560], [680, 560], [1280, 560]]], 'the news and the calendar: three columns of 560');
eq([rect(l, 'news'), rect(l, 'calendar')], [[1280, 40, 560, 686], [680, 40, 560, 686]], 'three columns, no quote: the news and the calendar take the whole height');
l = b.boardLayout(on({weather: true, calendar: true, news: true, quote: true}), 40);
eq([rect(l, 'news'), rect(l, 'quote'), rect(l, 'calendar')[3]], [[1280, 40, 560, 470], [1280, 550, 560, 176], 686], 'three columns with the quote: under the news');
const narrowClock = rect(l, 'clock')[3];
eq([narrowClock, rect(l, 'weather')[1], rect(l, 'weather')[1] + rect(l, 'weather')[3]], [b.narrowClockSize(clock) + 24 + 38 + 8, 40 + narrowClock + 40, 726], 'narrow clock: the date under the time, the weather takes the rest');
eq([b.narrowClockSize({...clock, size: 220}), b.narrowClockSize({...clock, size: 220, period: true}), b.narrowClockSize({...clock, size: 120})], [181, 157, 120], 'the digits shrink only when the time would not fit 560 px');
eq(b.clockHeight({...clock, showDate: false}, true), 184, 'narrow without the date: as tall as the digits');

l = b.boardLayout(on({quote: true}), 40);
eq(rect(l, 'quote'), [1120, 40, 600, 686], 'a quote alone in its column takes it whole');
l = b.boardLayout(on({calendar: true, news: true}), 40);
eq(ids(l), ['clock', 'calendar', 'news'], 'three columns without the weather: the clock alone on the left');

// the remote
l = b.boardLayout(on({weather: true, calendar: true, news: true, quote: true}), 40);
eq([b.columnAt(l, 100), b.columnAt(l, 960), b.columnAt(l, 1900)], [0, 1, 2], 'up from the app row: the column above the selected icon');
eq([b.entryCard(l, 0), b.entryCard(l, 1), b.entryCard(l, 2)], ['weather', 'calendar', 'news'], 'the lowest card that can be selected (not the quote, not the clock)');
eq([b.nextInColumn(l, 'weather', -1), b.nextInColumn(l, 'news', 1), b.nextInColumn(l, 'news', -1)], [null, null, null], 'the clock and the quote are skipped');
const two = b.boardLayout(on({calendar: true, quote: true}), 40);
eq([b.columnAt(two, 100), b.entryCard(two, b.columnAt(two, 100))], [1, 'calendar'], 'a column with only the clock is passed over for the nearest one with a card');
eq(b.columnAt(b.boardLayout(on({quote: true}), 40), 500), -1, 'nothing to step onto');
eq([b.besideCard(l, 'weather', 1), b.besideCard(l, 'calendar', 1), b.besideCard(l, 'news', -1), b.besideCard(l, 'weather', -1), b.besideCard(l, 'news', 1)], ['calendar', 'news', 'calendar', null, null], 'left and right: the card in the next column that way');
const noCal = b.boardLayout(on({weather: true, news: true, quote: true}), 40);
eq([b.besideCard(noCal, 'weather', 1), b.besideCard(noCal, 'news', -1)], ['news', 'weather'], 'without the calendar the weather and the news are side by side');
eq(b.besideCard(b.boardLayout(on({calendar: true, news: true}), 40), 'calendar', -1), null, 'the clock alone on the left is passed over: nothing there');

console.log(fail ? `\n${fail} FAILED` : '\nall passed');
process.exit(fail ? 1 : 0);
