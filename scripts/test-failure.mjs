// Tests for src/lib/failure.ts and the greyed-out rows in src/lib/prefs.ts.   Run: npm test
// The texts are checked in Russian (the screens speak the TV's language).
Object.defineProperty(globalThis, 'navigator', {value: {language: 'ru-RU', languages: ['ru-RU']}, configurable: true});
import {build} from 'esbuild';
const dir = new URL('../node_modules/', import.meta.url).pathname;
const opts = {define: {'import.meta.glob': '__glob'}, banner: {js: 'const __glob = () => ({});'}, bundle: true, format: 'esm', platform: 'node', logLevel: 'error'};
const src = new URL('../src/lib/', import.meta.url).pathname;
await build({...opts, stdin: {contents: `export * from './failure'; export * as prefs from './prefs'; export * as system from './system'; export {DEFAULT_CONFIG} from './storage';`, resolveDir: src, loader: 'ts'}, outfile: dir + '.cache-failure-test.mjs'});
const f = await import(dir + '.cache-failure-test.mjs');
const p = f.prefs;
let fail = 0;
const eq = (a, b, m) => { const ok = JSON.stringify(a) === JSON.stringify(b); if (!ok) { fail++; console.log('FAIL', m, JSON.stringify(a), '!=', JSON.stringify(b)); } else console.log('ok  ', m); };
eq(f.classifyFailure(new Error('Not running on webOS')), 'not-webos', 'not on a TV');
eq(f.classifyFailure(new Error('Timed out: luna://x/exec')), 'timeout', 'a timeout');
eq(f.classifyFailure(new Error('Service does not exist: org.webosbrew.hbchannel.service')), 'no-service', 'service missing');
eq(f.classifyFailure(new Error('Denied method call "exec" for category "/"')), 'denied', 'denied by the TV');
eq(f.classifyFailure(new Error('exit 3')), 'failed', 'any other error');
eq(f.classifyFailure(undefined), 'failed', 'no error object');
eq(f.failureText(new Error('Timed out: x'), 'прочитать память').indexOf('Не удалось прочитать память. ') === 0, true, 'text starts with what was attempted');
eq(f.failureText(new Error('Service does not exist: s'), 'x').indexOf('рутован') > 0, true, 'a missing service mentions root');

eq(f.rootState(), 'unknown', 'unknown at first');
f.noteExec(new Error('Timed out: x'));
eq(f.rootState(), 'unknown', 'a timeout proves nothing');
f.noteExec(new Error('exit 1'));
eq(f.rootState(), 'yes', 'an error from the command itself means the service answered');
f.noteExec(new Error('Service does not exist: s'));
eq([f.rootState(), f.lastFailure()], ['no', 'no-service'], 'a missing service means no root');
eq(f.whySuffix().indexOf('. Служба Homebrew Channel не запущена') === 0, true, 'the suffix explains it');
f.noteExec();
eq([f.rootState(), f.lastFailure(), f.whySuffix()], ['yes', null, ''], 'a good call clears it');

// rows
// the rows about how the launcher opens (Home screen also has the language, the reset and About)
const rowsOf = (info) => p.optionsFor('home', f.DEFAULT_CONFIG, info).filter((o) => o.id === 'homeMethod');
eq(rowsOf().filter((o) => o.disabled).length, 0, 'nothing is greyed out when nothing is known');
eq(rowsOf({homeHook: null, root: 'yes'}).filter((o) => o.disabled).length, 0, 'nothing is greyed out on a rooted TV');
f.noteExec(new Error('Service does not exist: org.webosbrew.hbchannel.service'));
const grey = rowsOf({homeHook: null, root: 'no', rootDetail: 'Service does not exist'});
eq(grey.filter((o) => o.disabled).map((o) => o.id).sort(), ['homeMethod'], 'every row that needs root is greyed out without it');
eq(grey.find((o) => o.id === 'homeMethod').disabled.indexOf('Нужен рутованный ТВ') === 0 && grey.find((o) => o.id === 'homeMethod').disabled.indexOf('ответ ТВ: Service does not exist') > 0, true, 'the reason says what the TV said');
eq(p.optionsFor('home', f.DEFAULT_CONFIG, {root: 'no'}).some((o) => o.id === 'resetLook' && !o.disabled), true, 'Reset look and clock stays usable without root');
eq(p.disabledReason('theme', {root: 'no'}), null, 'a look setting is never greyed out');
eq(p.optionsFor('look', f.DEFAULT_CONFIG, {root: 'no'}).filter((o) => o.disabled).length, 0, 'Appearance has no greyed rows');

// Home screen: one row says which way is on
eq(rowsOf({homeHook: null, homeMode: {mode: 'default', canDefault: true, release: '10.3.2'}})[0].valueText, 'Штатно', 'the official way is named');
eq(rowsOf({homeHook: null, homeMode: {mode: 'mount', canDefault: true, release: '10.3.2'}})[0].valueText, 'Подмена', 'the swap is named');
eq(rowsOf({homeHook: null, homeMode: null})[0].valueText, 'Неизвестно', 'unknown while it is being read');
process.exit(fail ? 1 : 0);
