// Tests for src/lib/uninstall.ts: what may be uninstalled, and the real shell command against a fake TV.   Run: npm test
import {build} from 'esbuild';
import {execSync} from 'child_process';
import {mkdtempSync, writeFileSync, chmodSync, existsSync, readFileSync, rmSync} from 'fs';
import {tmpdir} from 'os';
import {join} from 'path';

const out = new URL('../node_modules/.cache-uninstall-test.mjs', import.meta.url).pathname;
await build({
  entryPoints: [new URL('../src/lib/uninstall.ts', import.meta.url).pathname], bundle: true, format: 'esm', outfile: out, platform: 'node', logLevel: 'error',
  plugins: [{name: 'stub-apps', setup(b) {
    b.onResolve({filter: /\.\/apps$/}, () => ({path: 'apps-stub', namespace: 'stub'}));
    b.onLoad({filter: /.*/, namespace: 'stub'}, () => ({contents: 'export const exec = () => Promise.resolve("");', loader: 'js'}));
  }}]
});
const u = await import(out);
let fail = 0;
const eq = (a, b, m) => { const ok = JSON.stringify(a) === JSON.stringify(b); if (!ok) { fail++; console.log('FAIL', m, JSON.stringify(a), '!=', JSON.stringify(b)); } else console.log('ok  ', m); };
const v = (app) => u.uninstallVerdict(app).ok;
const DEV = '/media/developer/apps/usr/palm/applications/org.example.app/icon.png';
const STORE = '/media/cryptofs/apps/usr/palm/applications/netflix/icon.png';
const SYS = '/usr/palm/applications/some.system.app/icon.png';

// ---- what may be offered for removal ----
eq(v({id: 'tld.my.simplelauncher', iconPath: DEV, removable: true}), false, 'the launcher itself is never offered');
eq(v({id: 'com.webos.app.home', removable: true}), false, 'LG Home is never offered');
eq(v({id: 'org.webosbrew.hbchannel', iconPath: DEV, removable: true}), false, 'the Homebrew Channel is never offered (the launcher needs it)');
eq(v({id: 'com.webos.app.settings', removable: true}), false, 'system apps are never offered, even if the TV says removable');
eq(v({id: 'com.palm.app.browser', iconPath: SYS}), false, 'old Palm system apps are not offered');
eq(v({id: 'netflix', iconPath: STORE}), true, 'an app from the LG Content Store is offered');
eq(v({id: 'org.example.app', iconPath: DEV}), true, 'an app installed in developer mode is offered');
eq(v({id: 'some.system.app', iconPath: SYS}), false, 'an app installed in the system folder is not offered');
eq(v({id: 'x.y', removable: true}), true, 'the TV saying removable is enough');
eq(v({id: 'x.y', iconPath: DEV, removable: false}), false, 'the TV saying not removable wins over the folder');
eq(v({id: 'x.y'}), false, 'no evidence that the user installed it: not offered');
eq(v({id: 'bad id;rm -rf /', removable: true}), false, 'an ID that is not plain text is never offered');
eq(u.uninstallVerdict({id: 'tld.my.simplelauncher'}).reason.length > 0, true, 'a refusal always says why');

// ---- the command is only built from a plain ID ----
let refused = 0;
for (const bad of ["x'; reboot; '", 'a b', '', '$(id)', 'a"b', 'a;b', 'a`b`']) { try { u.uninstallCommand(bad); } catch { refused++; } }
eq(refused, 7, 'anything that is not a plain app ID is refused (no shell injection)');
eq(u.parseUninstall('x\nremoved\n'), 'removed', 'result: removed');
eq(u.parseUninstall('still-installed'), 'still-installed', 'result: still installed');
eq(u.parseUninstall('garbage'), 'failed', 'result: anything else is a failure');

// ---- the real command against a fake luna-send ----
const dir = mkdtempSync(join(tmpdir(), 'fake-uninstall-'));
writeFileSync(dir + '/luna-send', `#!/bin/sh
case "$*" in
  *getAppInfo*) if [ -f "$STATE" ]; then echo '{"returnValue":true,"appInfo":{}}'; else echo '{"returnValue":false}'; fi ;;
  *dev/remove*) echo "dev/remove called" >> "$CALLS"; [ "$MODE" = dev ] && rm -f "$STATE"; echo '{"returnValue":true}' ;;
  */remove*) echo "store remove called" >> "$CALLS"; [ "$MODE" = store ] && rm -f "$STATE"; echo '{"returnValue":true}' ;;
esac
`);
chmodSync(dir + '/luna-send', 0o755);
const run = (mode, installed) => {
  const state = dir + '/installed'; const calls = dir + '/calls'; const log = dir + '/uninstall.log';
  rmSync(calls, {force: true}); rmSync(log, {force: true}); rmSync(state, {force: true});
  if (installed) writeFileSync(state, '1');
  const cmd = u.uninstallCommand('org.example.app').split('/tmp/launcher-uninstall.log').join(log).split('sleep 2').join('sleep 0');
  const result = execSync(cmd, {shell: '/bin/sh', encoding: 'utf8', env: {...process.env, PATH: dir + ':' + process.env.PATH, STATE: state, MODE: mode, CALLS: calls}});
  return {result: u.parseUninstall(result), calls: existsSync(calls) ? readFileSync(calls, 'utf8') : '', log: existsSync(log) ? readFileSync(log, 'utf8') : ''};
};
let r = run('dev', true);
eq([r.result, /dev\/remove/.test(r.calls), /store remove/.test(r.calls)].join(), 'removed,true,false', 'a developer-mode app: removed by the first call, the second is not needed');
r = run('store', true);
eq([r.result, /dev\/remove/.test(r.calls), /store remove/.test(r.calls)].join(), 'removed,true,true', 'a store app: the first call does nothing, the second removes it');
r = run('none', true);
eq(r.result, 'still-installed', 'when the TV refuses both, it says still installed');
r = run('dev', false);
eq([r.result, r.calls].join(), 'removed,', 'an app that is already gone: nothing is called');
eq(/uninstall org\.example\.app/.test(run('dev', true).log), true, 'what happened is written to the log');
rmSync(dir, {recursive: true, force: true});
process.exit(fail ? 1 : 0);
