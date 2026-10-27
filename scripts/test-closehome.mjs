// Runs the real CLOSE_HOME_COMMAND against look-alike processes.   Run: npm test
import {build} from 'esbuild';
import {execSync, spawn} from 'child_process';
const out = new URL('../node_modules/.cache-system-test.mjs', import.meta.url).pathname;
await build({entryPoints: [new URL('../src/lib/system.ts', import.meta.url).pathname], bundle: true, format: 'esm', outfile: out, platform: 'node', logLevel: 'error',
  plugins: [{name: 'stub', setup(b) { b.onResolve({filter: /\.\/(apps|luna)$/}, (a) => ({path: a.path, namespace: 'stub'})); b.onLoad({filter: /.*/, namespace: 'stub'}, () => ({contents: 'export const exec = () => Promise.resolve(""); export const isWebOS = () => false;', loader: 'js'})); }}]});
const {CLOSE_HOME_COMMAND} = await import(out);
let fail = 0;
const eq = (a, b, m) => { const ok = a === b; if (!ok) fail++; console.log(ok ? 'ok  ' : 'FAIL', m, ok ? '' : `(${a} != ${b})`); };
const start = (name) => { const p = spawn('bash', ['-c', `exec -a "${name}" sleep 60`], {stdio: 'ignore', detached: true}); p.unref(); return p; };
const alive = (p) => { try { process.kill(p.pid, 0); return true; } catch { return false; } };
const home = start('/usr/bin/qml-runner --app com.webos.app.home');
const homeEnd = start('qml-runner com.webos.app.home');
const connect = start('/usr/bin/qml-runner --app com.webos.app.homeconnect');
const other = start('com.webos.app.settings');
const web = start('WebAppMgr --app tld.my.simplelauncher');
await new Promise((r) => setTimeout(r, 400));
const result = execSync(CLOSE_HOME_COMMAND, {shell: '/bin/sh'}).toString();   // runs as `sh -c`, like the TV's exec
await new Promise((r) => setTimeout(r, 400));
eq(result.trim(), 'done', 'the command itself survives and finishes (it does not kill its own shell)');
eq(alive(home), false, 'LG Home process ended');
eq(alive(homeEnd), false, 'LG Home ended when the id is the last word');
eq(alive(connect), true, 'com.webos.app.homeconnect left alone');
eq(alive(other), true, 'other system apps left alone');
eq(alive(web), true, 'our own launcher left alone');
// ---- LG Home is not ended while it is the app in front (that would leave a black screen) ----
import {mkdtempSync, writeFileSync as wf, chmodSync, rmSync as rm} from 'fs';
import {tmpdir} from 'os';
const bin = mkdtempSync(tmpdir() + '/fakeluna-');
wf(bin + '/luna-send', '#!/bin/sh\necho "$FAKE_FRONT"\n'); chmodSync(bin + '/luna-send', 0o755);
const front = (appId) => JSON.stringify({returnValue: true, appId, processId: '', windowId: ''});
const run2 = (appId) => execSync(CLOSE_HOME_COMMAND, {shell: '/bin/sh', env: {...process.env, PATH: bin + ':' + process.env.PATH, FAKE_FRONT: front(appId)}}).toString().trim();
const homeA = start('/usr/bin/qml-runner --app com.webos.app.home');
await new Promise((r) => setTimeout(r, 300));
eq(run2('com.webos.app.home'), 'skipped', 'LG Home in front: it is left running');
await new Promise((r) => setTimeout(r, 200));
eq(alive(homeA), true, 'and its process is still there');
eq(run2('tld.my.simplelauncher'), 'done', 'the launcher in front: LG Home is ended');
await new Promise((r) => setTimeout(r, 300));
eq(alive(homeA), false, 'and its process is gone');
rm(bin, {recursive: true, force: true});

for (const p of [home, homeEnd, connect, other, web]) { try { process.kill(p.pid); } catch {} }
process.exit(fail ? 1 : 0);
