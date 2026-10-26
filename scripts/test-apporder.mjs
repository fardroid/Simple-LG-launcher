// Tests for src/lib/apporder.ts: apps show unless hidden.   Run: npm test
import {build} from 'esbuild';
const out = new URL('../node_modules/.cache-apporder-test.mjs', import.meta.url).pathname;
await build({entryPoints: [new URL('../src/lib/apporder.ts', import.meta.url).pathname], bundle: true, format: 'esm', outfile: out, platform: 'node', logLevel: 'error'});
const o = await import(out);
let fail = 0;
const eq = (a, b, m) => { const ok = JSON.stringify(a) === JSON.stringify(b); if (!ok) { fail++; console.log('FAIL', m, JSON.stringify(a), '!=', JSON.stringify(b)); } else console.log('ok  ', m); };
const app = (id) => ({id});
const ids = (list) => list.map((a) => a.id);
const A = ['a', 'b', 'c', 'd'].map(app);

eq(ids(o.visibleApps(A, null, [])), ['a', 'b', 'c', 'd'], 'no settings: every app, in the TV order');
eq(ids(o.visibleApps(A, null, ['b'])), ['a', 'c', 'd'], 'a hidden app is left out');
eq(ids(o.visibleApps(A, ['c', 'a'], [])), ['c', 'a', 'b', 'd'], 'the saved order comes first, other apps follow');
eq(ids(o.visibleApps(A, ['c', 'a'], ['a'])), ['c', 'b', 'd'], 'hidden wins over the saved order');
const withNew = A.concat(app('new'));
eq(ids(o.visibleApps(withNew, ['c', 'a'], ['b'])), ['c', 'a', 'd', 'new'], 'an app installed later shows up by itself, at the end (this used to stay hidden)');
eq(ids(o.visibleApps(A, ['gone', 'a'], [])), ['a', 'b', 'c', 'd'], 'an uninstalled app in the saved order is ignored');
eq(ids(o.visibleApps(A, ['a', 'a', 'b'], [])), ['a', 'b', 'c', 'd'], 'a repeated id is shown once');
eq(ids(o.visibleApps(A, ['b', 'a'], null)), ['b', 'a'], 'old settings (not converted yet): only the saved list shows, as before');
eq(ids(o.visibleApps(A, null, null)), ['a', 'b', 'c', 'd'], 'old settings with no list: everything');

// converting old settings: what was not on the list stays hidden
eq(o.hiddenAfterUpgrade(A, ['b', 'a']), ['c', 'd'], 'apps that were not on the old list become the hide-list');
eq(o.hiddenAfterUpgrade(A, null), [], 'no old list: nothing was hidden');
const converted = o.hiddenAfterUpgrade(A, ['b', 'a']);
eq(ids(o.visibleApps(A.concat(app('later')), ['b', 'a'], converted)), ['b', 'a', 'later'], 'after converting, a newly installed app is visible');

eq(o.toggleHidden(['a'], 'b'), ['a', 'b'], 'hiding adds to the list');
eq(o.toggleHidden(['a', 'b'], 'a'), ['b'], 'showing takes it off');
process.exit(fail ? 1 : 0);
