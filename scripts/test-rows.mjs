// Tests for src/lib/rows.ts (one or two rows of apps).   Run: npm test
import {build} from 'esbuild';
const out = new URL('../node_modules/.cache-rows-test.mjs', import.meta.url).pathname;
await build({entryPoints: [new URL('../src/lib/rows.ts', import.meta.url).pathname], bundle: true, format: 'esm', outfile: out, platform: 'node', logLevel: 'error'});
const r = await import(out);
let fail = 0;
const eq = (a, b, m) => { const ok = JSON.stringify(a) === JSON.stringify(b); if (!ok) { fail++; console.log('FAIL', m, JSON.stringify(a), '!=', JSON.stringify(b)); } else console.log('ok  ', m); };

eq(r.splitRows(5, 1), [5], 'one row takes everything');
eq(r.splitRows(5, 2), [3, 2], 'two rows: the top row gets the extra tile');
eq(r.splitRows(4, 2), [2, 2], 'two rows: an even split');
eq(r.splitRows(1, 2), [1], 'one tile never makes a second row');
eq(r.splitRows(0, 2), [0], 'no tiles');
eq(r.rowOffsets([3, 2]), [0, 3], 'where each row starts in the list');
const sizes = [3, 2];
let p = {row: 0, cols: [2, 0]};
eq(r.movePosition(sizes, p, 'right'), {row: 0, cols: [2, 0]}, 'right stops at the end of the row');
eq(r.movePosition(sizes, {row: 0, cols: [0, 0]}, 'left'), {row: 0, cols: [0, 0]}, 'left stops at the start');
eq(r.movePosition(sizes, p, 'down'), {row: 1, cols: [2, 1]}, 'down goes to the next row, keeping the column where there is one (and the nearest where there is not)');
eq(r.movePosition(sizes, {row: 1, cols: [2, 1]}, 'up'), {row: 0, cols: [1, 1]}, 'up goes back, to the same column');
eq(r.movePosition(sizes, {row: 0, cols: [1, 0]}, 'up'), {row: 0, cols: [1, 0]}, 'up on the top row does nothing');
eq(r.movePosition(sizes, {row: 1, cols: [0, 0]}, 'down'), {row: 1, cols: [0, 0]}, 'down on the last row does nothing');
eq(r.movePosition([5], {row: 0, cols: [2, 0]}, 'down'), {row: 0, cols: [2]}, 'with one row, up and down do nothing');
eq(r.clampPosition([1], {row: 1, cols: [0, 3]}), {row: 0, cols: [0]}, 'a position is made valid when the rows change (two rows -> one)');
eq(r.clampPosition([4, 3], {row: 0, cols: [9, 9]}), {row: 0, cols: [3, 2]}, 'and columns are kept inside their rows');
eq(r.movePosition([0], r.initialPosition(), 'right'), {row: 0, cols: [0]}, 'an empty row does not break');
process.exit(fail ? 1 : 0);
