// The TV's keep-alive relaunch of its Home app is told apart from the Home key.   Run: npm test
import {build} from 'esbuild';
const out = new URL('../node_modules/.cache-ashome-test.mjs', import.meta.url).pathname;
await build({entryPoints: [new URL('../src/lib/ashome.ts', import.meta.url).pathname], bundle: true, format: 'esm', outfile: out, platform: 'node', logLevel: 'error',
  plugins: [{name: 'stub', setup(b) { b.onResolve({filter: /\.\/(apps|luna)$/}, (a) => ({path: a.path, namespace: 'stub'})); b.onLoad({filter: /.*/, namespace: 'stub'}, () => ({contents: 'export const exec = () => Promise.resolve(""); export const shellQuote = (s) => s; export const isWebOS = () => false;', loader: 'js'})); }}]});
const {isKeepAlive} = await import(out);
let fail = 0;
const eq = (a, b, m) => { const ok = a === b; if (!ok) fail++; console.log(ok ? 'ok  ' : 'FAIL', m, ok ? '' : `(${a} != ${b})`); };
eq(isKeepAlive('{    "keepAlive": true}'), true, 'the TV\'s keep-alive relaunch is recognised');
eq(isKeepAlive(undefined), false, 'no parameters: the Home key');
eq(isKeepAlive('{}'), false, 'empty parameters: the Home key');
eq(isKeepAlive('{"keepAlive": false}'), false, 'keepAlive false is not a keep-alive');
eq(isKeepAlive('not json'), false, 'parameters that are not JSON are not a keep-alive');
if (fail) { console.log(`${fail} failed`); process.exit(1); }
