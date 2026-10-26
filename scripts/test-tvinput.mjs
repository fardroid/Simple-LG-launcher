// Tests for src/lib/tvinput.ts (the command that adds the launcher as a TV input).   Run: npm test
// The texts are checked in Russian (the screens speak the TV's language).
Object.defineProperty(globalThis, 'navigator', {value: {language: 'ru-RU', languages: ['ru-RU']}, configurable: true});
import {build} from 'esbuild';
import {execFileSync} from 'node:child_process';
import {mkdtempSync, writeFileSync, chmodSync, readFileSync, existsSync, rmSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
const out = new URL('../node_modules/.cache-tvinput-test.mjs', import.meta.url).pathname;
await build({
	entryPoints: [new URL('../src/lib/tvinput.ts', import.meta.url).pathname], bundle: true, format: 'esm', outfile: out, platform: 'node', logLevel: 'error',
	plugins: [{name: 'stub-apps', setup (b) { b.onResolve({filter: /^\.\/apps$/}, () => ({path: 'apps-stub', namespace: 'stub'})); b.onLoad({filter: /.*/, namespace: 'stub'}, () => ({contents: 'export const exec = () => Promise.resolve("");', loader: 'js'})); }}]
});
const t = await import(out);
let fail = 0;
const eq = (a, b, m) => { const ok = JSON.stringify(a) === JSON.stringify(b); if (!ok) { fail++; console.log('FAIL', m, JSON.stringify(a), '!=', JSON.stringify(b)); } else console.log('ok  ', m); };

eq(t.replyOk('{"returnValue":true}'), true, 'a true reply counts as done');
eq(t.replyOk('{"returnValue": true, "x": 1}'), true, 'spacing does not matter');
eq(t.replyOk('{"returnValue":false,"errorText":"nope"}'), false, 'a false reply is not done');
eq(t.replyOk(''), false, 'no reply is not done');
eq(t.replyOk('luna-send: command not found'), false, 'an error message is not done');

// run the real command against a fake luna-send-pub (and again with only the plain luna-send)
const dir = mkdtempSync(join(tmpdir(), 'tvinput-'));
const fake = (name, reply) => { const p = join(dir, name); writeFileSync(p, `#!/bin/sh\nprintf '%s\\n' "$*" >> "${dir}/calls"\necho '${reply}'\n`); chmodSync(p, 0o755); };
const run = (cmd) => execFileSync('/bin/sh', ['-c', cmd], {env: {PATH: `${dir}:/usr/bin:/bin`}, encoding: 'utf8'});
fake('luna-send-pub', '{"returnValue":true}');
const added = run(t.addInputCommand());
eq(t.replyOk(added), true, 'the add command prints the TV\'s reply');
const calls = readFileSync(join(dir, 'calls'), 'utf8');
eq(calls.includes('luna://com.webos.service.eim/addDevice'), true, 'it calls addDevice on the External Input Manager');
eq(calls.includes('"appId":"tld.my.simplelauncher"'), true, 'with the launcher\'s app id');
eq(calls.startsWith('-n 1 '), true, 'as a one-shot call');
eq(calls.includes('"label":"Главный экран"'), true, 'with a label for the TV to show');
eq(calls.includes('"pigImage":""'), true, 'and the pigImage field the TV needs');
eq(calls.includes('getAllInputStatus'), true, 'it also reads the TV\'s input list, for the log');
eq(JSON.parse(readFileSync(new URL('../public/appinfo.json', import.meta.url).pathname, 'utf8')).supportGIP, true, 'appinfo.json has supportGIP, which the TV needs before it lists the launcher as an input');
eq(existsSync('/tmp/launcher-input.log') && readFileSync('/tmp/launcher-input.log', 'utf8').includes('addDevice'), true, 'and notes the result in the log');
run(t.removeInputCommand());
eq(readFileSync(join(dir, 'calls'), 'utf8').includes('deleteDevice'), true, 'the remove command calls deleteDevice');
rmSync(join(dir, 'luna-send-pub'));
fake('luna-send', '{"returnValue":true}');
eq(t.replyOk(run(t.addInputCommand())), true, 'without luna-send-pub it falls back to luna-send');
eq(run(t.addInputCommand()).trim().split('\n').length, 1, 'and prints a single reply line');
rmSync(dir, {recursive: true});
process.exit(fail ? 1 : 0);
