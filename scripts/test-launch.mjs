// Tests for opening the TV's own Settings app (src/lib/launch.ts).   Run: npm test
import {build} from 'esbuild';
import {writeFileSync} from 'fs';

const out = new URL('../node_modules/.cache-launch-test.mjs', import.meta.url).pathname;
const stub = new URL('../node_modules/.cache-launch-luna-stub.mjs', import.meta.url).pathname;
// a fake luna: getAppLoadStatus answers from `existing`; every call is recorded
writeFileSync(stub, `export const calls = []; export const state = {existing: [], refuse: false};
export function lunaCall (uri, params) {
  calls.push({uri, params});
  if (uri.indexOf('getAppLoadStatus') !== -1) return state.refuse ? Promise.reject(new Error('refused')) : Promise.resolve({exist: state.existing.indexOf(params.appId) !== -1});
  return Promise.resolve({returnValue: true});
}`);
await build({entryPoints: [new URL('../src/lib/launch.ts', import.meta.url).pathname], bundle: true, format: 'esm', outfile: out, platform: 'node', logLevel: 'error',
  plugins: [{name: 'luna', setup(b) { b.onResolve({filter: /\.\/luna$/}, () => ({path: stub, external: true})); }}]});
const launch = await import(out);
const fake = await import(stub);
let fail = 0;
const eq = (a, b, m) => { const ok = JSON.stringify(a) === JSON.stringify(b); if (!ok) { fail++; console.log('FAIL', m, JSON.stringify(a), '!=', JSON.stringify(b)); } else console.log('ok  ', m); };
const launched = () => fake.calls.filter((c) => c.uri.indexOf('/launch') !== -1).map((c) => c.params.id);
const reset = (existing, refuse = false) => { fake.calls.length = 0; fake.state.existing = existing; fake.state.refuse = refuse; };

reset(['com.palm.app.settings']);
await launch.launchSystemSettings();
eq(launched(), ['com.palm.app.settings'], 'the usual Settings app is opened when it exists');
reset(['com.webos.app.settings']);
await launch.launchSystemSettings();
eq(launched(), ['com.webos.app.settings'], 'the other known ID is used when only that one exists');
reset([], true);
await launch.launchSystemSettings();
eq(launched(), ['com.palm.app.settings'], 'when the existence check is refused, the usual ID is launched anyway');
reset([]);
await launch.launchSystemSettings();
eq(launched(), ['com.palm.app.settings'], 'when neither exists, the usual ID is tried (the TV then says no)');
process.exit(fail ? 1 : 0);
