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
const rowsOf = (info) => p.optionsFor('options', f.DEFAULT_CONFIG, info);
eq(rowsOf().filter((o) => o.disabled).length, 0, 'nothing is greyed out when nothing is known');
eq(rowsOf({homeApp: null, homeHook: null, root: 'yes'}).filter((o) => o.disabled).length, 0, 'nothing is greyed out on a rooted TV');
f.noteExec(new Error('Service does not exist: org.webosbrew.hbchannel.service'));
const grey = rowsOf({homeApp: null, homeHook: null, root: 'no', rootDetail: 'Service does not exist'});
eq(grey.filter((o) => o.disabled).map((o) => o.id).sort(), p.ROOT_OPTIONS.slice().sort(), 'every row that needs root is greyed out without it');
eq(grey.filter((o) => !o.disabled).map((o) => o.id), [], 'every Options row needs root, so all are greyed out');
eq(p.optionsFor('look', f.DEFAULT_CONFIG, {root: 'no'}).some((o) => o.id === 'resetLook' && !o.disabled), true, 'Reset look and clock (on the Look tab) stays usable without root');
eq(grey[0].disabled.indexOf('Нужен рутованный ТВ') === 0 && grey[0].disabled.indexOf('ответ ТВ: Service does not exist') > 0, true, 'the reason says what the TV said');
eq(p.disabledReason('theme', {root: 'no'}), null, 'a look setting is never greyed out');
eq(p.optionsFor('look', f.DEFAULT_CONFIG, {root: 'no'}).filter((o) => o.disabled).length, 0, 'the Look tab has no greyed rows');

// the LG logo setting
const sys = f.system;
eq(sys.parseLogo('{"category":"other","settings":{"lgLogoDisplay":"off"},"returnValue":true}'), 'off', 'logo off is read');
eq(sys.parseLogo('{"settings":{"lgLogoDisplay":"on"},"returnValue":true}'), 'on', 'logo on is read');
eq(sys.parseLogo('{"returnValue":true,"settings":{}}'), 'unsupported', 'an answer without the key means the TV has no such setting');
eq(sys.parseLogo('{"returnValue":false,"errorText":"x"}'), null, 'an error is not an answer');
eq(sys.parseLogo(''), null, 'no output is not an answer');
eq(sys.setLogoCommand(false).indexOf('"lgLogoDisplay":"off"') > 0 && sys.setLogoCommand(true).indexOf('"lgLogoDisplay":"on"') > 0, true, 'set command carries the wanted value');
eq(sys.GET_LOGO_COMMAND.indexOf('"category":"other"') > 0, true, 'read command uses the other category');
const logoRow = (info) => p.optionsFor('options', f.DEFAULT_CONFIG, info).find((o) => o.id === 'lgLogo');
eq([logoRow({logo: 'off'}).valueText, logoRow({logo: 'on'}).valueText, logoRow({logo: null}).valueText], ['Выкл.', 'Вкл.', 'Неизвестно'], 'the row shows the real state');
eq(logoRow({logo: 'unsupported', root: 'yes'}).disabled.indexOf('нет настройки') > 0, true, 'greyed out with a reason when this TV has no such setting');
eq(logoRow({logo: 'off', root: 'yes'}).disabled, undefined, 'usable on a rooted TV that has it');
process.exit(fail ? 1 : 0);
