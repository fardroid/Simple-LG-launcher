// Tests for src/lib/tileorder.ts (one order for apps and inputs).   Run: npm test
import {build} from 'esbuild';
const out = new URL('../node_modules/.cache-tileorder-test.mjs', import.meta.url).pathname;
await build({entryPoints: [new URL('../src/lib/tileorder.ts', import.meta.url).pathname], bundle: true, format: 'esm', outfile: out, platform: 'node', logLevel: 'error'});
const t = await import(out);
let fail = 0;
const eq = (a, b, m) => { const ok = JSON.stringify(a) === JSON.stringify(b); if (!ok) { fail++; console.log('FAIL', m, JSON.stringify(a), '!=', JSON.stringify(b)); } else console.log('ok  ', m); };
const id = (x) => x;

eq(t.orderTiles(['a', 'b', 'c'], id, null), ['a', 'b', 'c'], 'no saved order: the usual order');
eq(t.orderTiles(['a', 'b', 'c'], id, []), ['a', 'b', 'c'], 'an empty saved order: the usual order');
eq(t.orderTiles(['a', 'b', 'c', 'h1'], id, ['h1', 'c']), ['h1', 'c', 'a', 'b'], 'listed tiles first, the rest after in their usual order');
eq(t.orderTiles(['a', 'b'], id, ['gone', 'b', 'b', 'a']), ['b', 'a'], 'unknown and repeated keys are ignored');
eq(t.orderTiles([{k: 'x'}, {k: 'y'}], (o) => o.k, ['y']).map((o) => o.k), ['y', 'x'], 'works on objects through keyOf');

eq(t.movesFor(['a', 'b', 'c'], 'a'), {start: false, left: false, right: true, end: true}, 'first tile: can only go right or to the end');
eq(t.movesFor(['a', 'b', 'c'], 'b'), {start: true, left: true, right: true, end: true}, 'middle tile: all moves');
eq(t.movesFor(['a', 'b', 'c'], 'c'), {start: true, left: true, right: false, end: false}, 'last tile: no move right or to the end');
eq(t.movesFor(['a'], 'a'), {start: false, left: false, right: false, end: false}, 'a single tile cannot move');
eq(t.movesFor(['a'], 'zzz'), {start: false, left: false, right: false, end: false}, 'an unknown key cannot move');

eq(t.moveKey(['a', 'b', 'c', 'd'], 'c', 'start'), ['c', 'a', 'b', 'd'], 'move to start');
eq(t.moveKey(['a', 'b', 'c', 'd'], 'c', 'left'), ['a', 'c', 'b', 'd'], 'move left');
eq(t.moveKey(['a', 'b', 'c', 'd'], 'c', 'right'), ['a', 'b', 'd', 'c'], 'move right');
eq(t.moveKey(['a', 'b'], 'a', 'left'), ['a', 'b'], 'left from the start does nothing');
eq(t.moveKey(['a', 'b'], 'b', 'right'), ['a', 'b'], 'right from the end does nothing');
eq(t.moveKey(['a', 'b'], 'a', 'start'), ['a', 'b'], 'start from the start does nothing');
eq(t.moveKey(['a', 'b'], 'q', 'start'), ['a', 'b'], 'an unknown key does nothing');
eq(t.moveKey(['a', 'b', 'c', 'd'], 'b', 'end'), ['a', 'c', 'd', 'b'], 'move to end');
eq(t.moveKey(['a', 'b', 'c'], 'c', 'end'), ['a', 'b', 'c'], 'end from the end does nothing');
eq(t.moveKey(['a', 'b'], 'q', 'end'), ['a', 'b'], 'an unknown key does not move to the end');
const orig = ['a', 'b', 'c']; t.moveKey(orig, 'c', 'start');
eq(orig, ['a', 'b', 'c'], 'the original list is never changed');

eq(t.reslot(['a', 'h1', 'b', 'h2'], ['b', 'a']), ['b', 'h1', 'a', 'h2'], 'only the apps swap; the inputs keep their places');
eq(t.reslot(['a', 'h1', 'b', 'h2'], ['h2', 'h1']), ['a', 'h2', 'b', 'h1'], 'only the inputs swap');
eq(t.reslot(['a', 'b'], ['b', 'a', 'ghost']), ['b', 'a'], 'keys that are not on the row are ignored');

const u = t.orderUpdate(['h1', 'b', 'a'], ['a', 'b'], ['h1'], ['a', 'b', 'hiddenApp'], ['h1', 'h9']);
eq(u, {tileOrder: ['h1', 'b', 'a'], appIds: ['b', 'a', 'hiddenApp'], sourceIds: ['h1', 'h9']}, 'saving the order updates the app and input orders and keeps entries that are not on the row');
eq(t.orderUpdate(['a'], ['a'], [], null, []).appIds, null, 'old app settings (not converted yet) are left alone');
eq(t.orderUpdate(['h2', 'h1'], [], ['h1', 'h2'], [], ['h1', 'h2']).sourceIds, ['h2', 'h1'], 'inputs follow the new order');
process.exit(fail ? 1 : 0);
