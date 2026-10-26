// Tests for the dock's magnification and the pointer mapping (src/lib/dock.ts).   Run: npm test
import {build} from 'esbuild';
const out = new URL('../node_modules/.cache-dock-test.mjs', import.meta.url).pathname;
await build({entryPoints: [new URL('../src/lib/dock.ts', import.meta.url).pathname], bundle: true, format: 'esm', outfile: out, platform: 'node', logLevel: 'error'});
const d = await import(out);
let fail = 0;
const eq = (a, b, m) => { const ok = JSON.stringify(a) === JSON.stringify(b); if (!ok) { fail++; console.log('FAIL', m, JSON.stringify(a), '!=', JSON.stringify(b)); } else console.log('ok  ', m); };
const near = (a, b, tol, m) => { const ok = Math.abs(a - b) <= tol; if (!ok) { fail++; console.log('FAIL', m, a, 'is not within', tol, 'of', b); } else console.log('ok  ', m); };

eq([d.dockScale(0), d.dockScale(1), d.dockScale(2), d.dockScale(3), d.dockScale(9)], [1.42, 1.2, 1.07, 1, 1], 'tiles shrink with the distance from the magnified spot');
near(d.dockScale(0.5), 1.31, 0.001, 'and smoothly in between');

// Where the tile centres really are for a pointer position (the same sums as Row.tsx)
const tile = 150, step = 166, count = 30;
function centres (pos) {
	const out = []; let running = 0;
	for (let i = 0; i < count; i++) { const e = tile * (d.dockScale(Math.abs(i - pos)) - 1); out.push(i * step + tile / 2 + running + e / 2); running += e; }
	return out;
}
for (const target of [2.0, 5.5, 12.25, 20.0, 27.8]) {
	const c = centres(target);
	const i = Math.floor(target), px = c[i] + (c[i + 1] - c[i]) * (target - i);   // the pointer sits where tile `target` really is
	near(d.pointerIndex(px, count, tile, step), target, 0.05, `the pointer on tile ${target} is mapped to ${target}`);
}
// the naive mapping (untouched layout) is off by up to a whole tile far along the row, which is what this fixes
const c = centres(20), px = c[20];
near((px - tile / 2) / step - 20, 0.43, 0.2, 'the untouched layout would be about half a tile out at tile 20');
eq([d.indexAmong([100, 300, 500], 100), d.indexAmong([100, 300, 500], 200), d.indexAmong([100, 300, 500], 400), d.indexAmong([100, 300, 500], 500)], [0, 0.5, 1.5, 2], 'the tile index is read from where the centres really are');
eq([d.indexAmong([100, 300, 500], -999), d.indexAmong([100, 300, 500], 9999), d.indexAmong([], 5), d.indexAmong([100], 5)], [-0.4, 2.4, 0, 0], 'outside the row it is clamped');
near(d.pointerIndex(-50, count, tile, step), -0.4, 0.001, 'left of the row is clamped');
near(d.pointerIndex(99999, count, tile, step), count - 0.6, 0.001, 'right of the row is clamped');
console.log(fail ? `\n${fail} FAILED` : '\nall passed');
process.exit(fail ? 1 : 0);
