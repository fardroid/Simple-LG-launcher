// Runs the launcher's real install/boot-link/remove commands against a fake TV filesystem.   Run: npm test
// The texts are checked in Russian (the screens speak the TV's language).
Object.defineProperty(globalThis, 'navigator', {value: {language: 'ru-RU', languages: ['ru-RU']}, configurable: true});
import {build} from 'esbuild';
import {execSync, spawn} from 'child_process';
import {existsSync, mkdirSync, readFileSync, writeFileSync, mkdtempSync, rmSync} from 'fs';
import {tmpdir} from 'os';
import {join} from 'path';

const out = new URL('../node_modules/.cache-homehookcmd-test.mjs', import.meta.url).pathname;
await build({entryPoints: [new URL('../src/lib/homehookcmd.ts', import.meta.url).pathname], bundle: true, format: 'esm', outfile: out, platform: 'node', logLevel: 'error'});
const h = await import(out);
const here = (p) => new URL(p, import.meta.url).pathname;
let fail = 0;
const eq = (a, b, m) => { const ok = a === b; if (!ok) fail++; console.log(ok ? 'ok  ' : 'FAIL', m, ok ? '' : `(${a} != ${b})`); };

eq([h.parseHookResult('applied\n[exit 0]'), h.parseHookResult('x\nlauncher-missing\n[exit 2]'), h.parseHookResult('garbage'), h.parseHookResult('')].join(','), 'applied,launcher-missing,failed,failed', 'the result word is found in the apply output');
eq([h.parseExit('hello\n[exit 0]'), h.parseExit('boom\n[exit 127]\n'), h.parseExit('no status')].map(String).join(','), '0,127,null', 'exit codes are read');
eq(h.lastLine('base64: invalid option -- d\n[exit 1]'), 'base64: invalid option -- d', 'the last message is what the user is told');
const st = (o) => JSON.stringify(h.parseHookStatus(o));
eq([st('running\nlinked\n'), st('linked\n'), st('running\n'), st('')].join(' '), '{"running":true,"linked":true} {"running":false,"linked":true} {"running":true,"linked":false} {"running":false,"linked":false}', 'status output is read');

const root = mkdtempSync(join(tmpdir(), 'fake-tv-'));
const fake = (cmd) => cmd.split('/media/developer').join(root + '/media/developer').split('/var/lib/webosbrew').join(root + '/var/lib/webosbrew').split('/tmp/launcher').join(root + '/tmp/launcher');
const env = () => ({...process.env, DRY_RUN: '1', LOG: root + '/tmp/log', APP_QML: root + '/usr/palm/qml', HOOK: root + '/media/developer/launcher-homehook', LAUNCHER: root + '/media/developer/apps/usr/palm/applications/tld.my.simplelauncher', PIDFILE: root + '/tmp/launcher-homehook-watch.pid', ALLOW: root + '/tmp/launcher-allow-stock-home'});
const run = (cmd) => execSync(fake(cmd), {shell: '/bin/sh', encoding: 'utf8', env: env()});
const sh = (cmd) => { try { return run(cmd); } catch (e) { return String(e.stdout || ''); } };
mkdirSync(root + '/tmp', {recursive: true});
mkdirSync(root + '/usr/palm/qml', {recursive: true});
mkdirSync(root + '/var/lib/webosbrew/init.d', {recursive: true});
const files = {watchSh: readFileSync(here('../src/homehook/watch.sh'), 'utf8'), applySh: readFileSync(here('../src/homehook/apply.sh'), 'utf8'), stockSh: readFileSync(here('../src/homehook/stock.sh'), 'utf8'), diagnoseSh: readFileSync(here('../src/homehook/diagnose.sh'), 'utf8'), homekeyPy: readFileSync(here('../src/homehook/homekey.py'), 'utf8')};
const hook = root + '/media/developer/launcher-homehook';
const link = root + '/var/lib/webosbrew/init.d/48-launcher-homehook';
const launcherDir = root + '/media/developer/apps/usr/palm/applications/tld.my.simplelauncher';

const runInstall = (boot) => {
  let output = '';
  const steps = h.installSteps(files, boot);
  for (let i = 0; i < steps.length; i++) {
    output = sh(h.withStatus(steps[i].command));
    if (i < steps.length - 1 && h.parseExit(output) !== 0) return {output, failedStep: steps[i].name};
  }
  return {output, failedStep: null};
};

// launcher not installed yet -> refused
eq(h.parseHookResult(runInstall(true).output), 'launcher-missing', 'install is refused while the launcher is not installed');
mkdirSync(launcherDir, {recursive: true});
writeFileSync(launcherDir + '/appinfo.json', '{}');

// install with the boot link
eq(h.parseHookResult(runInstall(true).output), 'applied', 'install works');
eq(['watch.sh', 'apply.sh', 'stock.sh', 'diagnose.sh'].every((f) => readFileSync(hook + '/' + f, 'utf8') === files[f.replace('.sh', 'Sh')]), true, 'all four scripts were written byte for byte');
eq(readFileSync(hook + '/version', 'utf8').trim(), h.HOOK_VERSION, 'the version was recorded');
eq(sh(h.VERSION_COMMAND).trim(), h.HOOK_VERSION, 'the launcher can read the installed version back');
eq(existsSync(link), true, 'the boot link exists');
eq(st(sh(h.STATUS_COMMAND)), '{"running":false,"linked":true}', 'status: linked (the watcher is not started in a dry run)');
eq(h.parseHookResult(sh(h.APPLY_COMMAND)), 'applied', 'applying again works');

// the boot link can be reversed on its own
sh(h.LINK_OFF_COMMAND);
eq([existsSync(link), existsSync(hook + '/watch.sh')].join(','), 'false,true', 'removing the boot link leaves the helper installed');
eq(h.parseExit(sh(h.LINK_ON_COMMAND)), 0, 'the boot link can be added again');
eq(existsSync(link), true, 'and it exists');
eq(h.parseHookResult(runInstall(false).output), 'applied', 'installing without boot works');
eq(existsSync(link), false, 'and removes an existing boot link');

// the boot link can't be added when nothing is installed
rmSync(hook, {recursive: true, force: true});
eq(/files-missing/.test(sh(h.LINK_ON_COMMAND)) && !existsSync(link), true, 'no boot link is made without the helper files');

// stock home request
runInstall(true);
sh(h.STOCK_COMMAND);
eq(existsSync(root + '/tmp/launcher-allow-stock-home'), true, 'the LG Home tile makes the watcher stand down');

// stopping an older watcher: a real process whose pid is in the pid file
const child = spawn('sleep', ['60'], {stdio: 'ignore', detached: true}); child.unref();
writeFileSync(root + '/tmp/launcher-homehook-watch.pid', String(child.pid));
const stopStep = h.installSteps(files, true).find((s) => s.name === 'остановка старого наблюдателя');
sh(h.withStatus(stopStep.command));
await new Promise((r) => setTimeout(r, 300));
let alive = true; try { process.kill(child.pid, 0); } catch { alive = false; }
eq([alive, existsSync(root + '/tmp/launcher-homehook-watch.pid')].join(','), 'false,false', 'an older watcher is stopped and its pid file removed');

// a watcher that ignores a polite stop (as a shell busy with a long command does) is still stopped
const stubborn = spawn('sh', ['-c', 'trap : TERM; while true; do sleep 1; done'], {stdio: 'ignore', detached: true}); stubborn.unref();
writeFileSync(root + '/tmp/launcher-homehook-watch.pid', String(stubborn.pid));
sh(h.withStatus(stopStep.command));
await new Promise((r) => setTimeout(r, 400));
let stubbornAlive = true; try { process.kill(stubborn.pid, 0); } catch { stubbornAlive = false; }
eq(stubbornAlive, false, 'a watcher that ignores a polite stop is stopped for sure');
// a watcher whose pid file was lost is found by its command line
const lost = spawn('bash', ['-c', 'exec -a "sh /media/developer/launcher-homehook/watch.sh" sleep 60'], {stdio: 'ignore', detached: true}); lost.unref();
await new Promise((r) => setTimeout(r, 300));
sh(h.withStatus(stopStep.command));
await new Promise((r) => setTimeout(r, 400));
let lostAlive = true; try { process.kill(lost.pid, 0); } catch { lostAlive = false; }
eq(lostAlive, false, 'a watcher with no pid file is found by its command line and stopped');
// the files are replaced by moving, never written in place
eq(h.installSteps(files, true).filter((st) => /watch\.sh/.test(st.command) && />+ '/.test(st.command)).every((st) => /watch\.sh\.new/.test(st.command)), true, 'watch.sh is written under a temporary name first');
eq(existsSync(hook + '/watch.sh.new') || existsSync(hook + '/apply.sh.new'), false, 'no temporary files are left after an install');

// removal
sh(h.REMOVE_COMMAND);
eq([existsSync(hook), existsSync(link), existsSync(root + '/tmp/launcher-allow-stock-home')].join(','), 'false,false,false', 'remove deletes the folder, the boot link and the stand-down flag');
eq(st(sh(h.STATUS_COMMAND)), '{"running":false,"linked":false}', 'status: off after removal');
eq(h.parseHookResult(sh(h.APPLY_COMMAND)), 'files-missing', 'apply after removal does nothing');

// the commands themselves
const steps = h.installSteps(files, true);
eq(steps.some((s) => s.command.includes(files.watchSh.slice(0, 60))), false, 'file contents are carried as base64, never raw');
eq(steps.some((s) => s.name.indexOf('homekey.py') !== -1), true, 'the Home key helper is written to the TV');
eq(/homekey\.py/.test(steps.find((s) => s.name === 'размещение файлов').command) && /chmod \+x[^;]*homekey\.py/.test(steps.find((s) => s.name === 'права на запуск скриптов').command), true, 'and put in place and made executable');
eq(/\[l\]auncher-homehook\/homekey\.py/.test(steps.find((s) => s.name === 'остановка старого наблюдателя').command), true, 'stopping the watcher stops the Home key helper too');
eq(Math.max(...steps.map((s) => s.command.length)) < 4300, true, `every install step is a small command (largest ${Math.max(...steps.map((s) => s.command.length))} characters)`);
// a failing step says which one and why
mkdirSync(root + '/media/developer', {recursive: true});
writeFileSync(hook, 'a file where the folder should be');
const broken = runInstall(true);
eq(broken.failedStep, 'создание папки', 'a failing step is named');
eq(h.lastLine(broken.output).length > 0, true, 'and the TV\'s own error text comes back with it');
rmSync(hook, {force: true});
rmSync(root, {recursive: true, force: true});
process.exit(fail ? 1 : 0);
