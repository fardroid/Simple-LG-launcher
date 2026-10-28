// How the launcher opens (lib/homemode.ts), reloading after an update (lib/update.ts), the pixel shift (lib/pixelshift.ts) and the start-up picture (lib/splash.ts).   Run: npm test
import {build} from 'esbuild';
const stub = {name: 'stub', setup(b) { b.onResolve({filter: /\.\/(apps|luna)$/}, (a) => ({path: a.path, namespace: 'stub'})); b.onLoad({filter: /.*/, namespace: 'stub'}, () => ({contents: 'export const exec = () => Promise.resolve(""); export const shellQuote = (s) => s; export const isWebOS = () => false; export const lunaCall = () => Promise.reject(new Error("no TV"));', loader: 'js'})); }};
const load = async (name) => {
	const out = new URL(`../node_modules/.cache-${name}-test.mjs`, import.meta.url).pathname;
	await build({entryPoints: [new URL(`../src/lib/${name}.ts`, import.meta.url).pathname], bundle: true, format: 'esm', outfile: out, platform: 'node', logLevel: 'error', plugins: [stub]});
	return import(out);
};
const hm = await load('homemode');
const ps = await load('pixelshift');
const sp = await load('splash');
const pn = await load('panel');
const up = await load('update');
let fail = 0;
const eq = (a, b, m) => { const ok = JSON.stringify(a) === JSON.stringify(b); if (!ok) fail++; console.log(ok ? 'ok  ' : 'FAIL', m, ok ? '' : `(${JSON.stringify(a)} != ${JSON.stringify(b)})`); };

eq(hm.parseModeStatus('mode=default\ncan_default=yes\nrelease=10.3.2\n'), {mode: 'default', canDefault: true, release: '10.3.2'}, 'the status of the TV is read');
eq(hm.parseModeStatus('mode=mount\ncan_default=no\nrelease=\n'), {mode: 'mount', canDefault: false, release: ''}, 'an unknown release stays empty');
eq(hm.parseModeStatus('sh: homemode.sh: not found'), null, 'no script: unknown, not "off"');
eq(hm.parseResult('ok default\n'), {ok: true, mode: 'default', why: ''}, 'a switch that worked');
eq(hm.parseResult('failed mount: the mount did not take'), {ok: false, mode: 'mount', why: 'the mount did not take'}, 'a switch that failed keeps its reason');
eq(hm.parseResult(''), null, 'no result yet: still running');
eq(hm.releaseMajor('10.3.2'), 10, 'webOS 25 is release 10');
eq(hm.modeNote('default', {mode: 'none', canDefault: false, release: '10.3.2'}).blocked, true, 'the official way is blocked where the TV cannot choose a Home app');
eq(hm.modeNote('mount', {mode: 'none', canDefault: true, release: '6.4.0'}), {blocked: false, text: 'Tried only on webOS 25 so far'}, 'the swap on another webOS carries a caution, not a block');
eq(hm.HOME_MODES.map((m) => hm.modeText(m).pros.length > 0 && hm.modeText(m).cons.length > 0), [true, true, true, true], 'every way has pros and cons');

eq(up.isOutdated('1761650000000', '1761640000000'), true, 'another build on disk: the running copy loads itself again');
eq(up.isOutdated('1761640000000', '1761640000000'), false, 'the same build: nothing happens');
eq([up.isOutdated(null, '1761640000000'), up.isOutdated('', '1761640000000'), up.isOutdated('<html>', '1761640000000')], [false, false, false], 'nothing readable on disk: never a reload');
eq(up.isOutdated('1761650000000', ''), false, 'the desktop preview (no build id) never reloads itself');

const steps = new Set();
for (let m = 0; m < 8; m++) { const s = ps.shiftAt(new Date(m * 60000)); steps.add(`${s.x},${s.y}`); eq(Number.isInteger(s.x) && Number.isInteger(s.y) && Math.abs(s.x) <= 8 && Math.abs(s.y) <= 4, true, `minute ${m}: a whole-pixel step within 8 px`); }
eq(steps.size, 8, 'eight different places in eight minutes');
eq(ps.shiftAt(new Date(8 * 60000)), ps.shiftAt(new Date(0)), 'and round again');
eq(ps.shiftAt(new Date(59000)), ps.shiftAt(new Date(0)), 'the same place for the whole minute');

const encoded = 'A'.repeat(70000);
const cmds = sp.splashCommands(encoded, '/x/splash.jpg');
eq(cmds.length, 4, 'a large picture goes in three pieces and one move into place');
eq(cmds[0].indexOf("base64 -d > '/x/splash.jpg.new'") > 0 && cmds[1].indexOf('>>') > 0, true, 'the first piece creates the temporary file, the next ones append');
eq(cmds.slice(0, 3).map((c) => c.split("'")[1].length % 4), [0, 0, 0], 'every piece decodes on its own');
eq(cmds[3], "[ -s '/x/splash.jpg.new' ] && mv -f '/x/splash.jpg.new' '/x/splash.jpg'", 'the TV never reads a half-written picture');
eq(sp.splashKey('data:abc') === sp.splashKey('data:abd'), false, 'another picture is written again');
eq(['OLED65C1PUB', 'oled55b4', '43QNED84A6C', '55NANO776RA', '50UT8050'].map(pn.isOledModel), [true, true, false, false, false], 'an OLED is told by its model name');
eq(pn.modelOf('{"returnValue":true,"modelName":"OLED48C1KNB"}'), 'OLED48C1KNB', 'the model name is read from the answer');
if (fail) { console.log(`${fail} failed`); process.exit(1); }
