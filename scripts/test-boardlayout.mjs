// Tests for where the widgets block stands (src/lib/boardlayout.ts).   Run: npm test
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
console.log(fail ? `\n${fail} FAILED` : '\nall passed');
process.exit(fail ? 1 : 0);
