// Tests for the pure helpers in src/lib/wallpaper.ts.   Run: npm test
// The texts are checked in Russian (the screens speak the TV's language).
Object.defineProperty(globalThis, 'navigator', {value: {language: 'ru-RU', languages: ['ru-RU']}, configurable: true});
// Quick logic test of the pure helpers (compiled from TS on the fly with esbuild via vite's dependency)
import {build} from 'esbuild';
// lib/apps.ts pulls in Vite-only code, so swap it for a stub: only the pure helpers are tested here.
const out = new URL('../node_modules/.cache-wallpaper-test.mjs', import.meta.url).pathname;
await build({
  entryPoints: [new URL('../src/lib/wallpaper.ts', import.meta.url).pathname], bundle: true, format: 'esm', outfile: out, platform: 'node', logLevel: 'error',
  plugins: [{name: 'stub-apps', setup(b) {
    b.onResolve({filter: /\.\/apps$/}, () => ({path: 'apps-stub', namespace: 'stub'}));
    b.onLoad({filter: /.*/, namespace: 'stub'}, () => ({contents: "import {execSync} from 'child_process'; export const exec = (c) => Promise.resolve(String(execSync(c, {shell: '/bin/sh', maxBuffer: 100000000}))); export const shellQuote = (v) => \"'\" + v.replace(/'/g, \"'\\\\''\") + \"'\";", loader: 'js'}));
  }}]
});
const w = await import(out);
let fail = 0;
const eq = (a, b, m) => { const ok = JSON.stringify(a) === JSON.stringify(b); if (!ok) { fail++; console.log('FAIL', m, JSON.stringify(a), '!=', JSON.stringify(b)); } else console.log('ok  ', m); };
eq(w.parseListing('Zeta/\nalpha/\n._a.jpg\n.hidden/\nb.JPG\na.png\nnotes.txt\nclip.mp4\nsea.webp\n\n'),
   [{name:'alpha',isDir:true},{name:'Zeta',isDir:true},{name:'a.png',isDir:false},{name:'b.JPG',isDir:false},{name:'sea.webp',isDir:false}], 'listing filters+sorts');
eq(w.parseMounts('rootfs / rootfs rw 0 0\n/dev/sda1 /tmp/usb/sda/sda1 vfat rw 0 0\n/dev/mmcblk0p9 /media/internal ext4 rw 0 0\n/dev/sdb1 /tmp/usb/sdb/My\\040Stick exfat rw 0 0\n'),
   [{label:'USB-накопитель 1',path:'/tmp/usb/sda/sda1'},{label:'USB-накопитель 2',path:'/tmp/usb/sdb/My Stick'}], 'mounts parse');
eq(w.parseMounts('/dev/sda1 /tmp/usb/sda/sda1 vfat rw 0 0\n'), [{label:'USB-накопитель',path:'/tmp/usb/sda/sda1'}], 'single usb label');
eq(w.parentPath('/tmp/usb/sda/sda1/Walls'), '/tmp/usb/sda/sda1', 'parent');
eq(w.parentPath('/a'), '/', 'parent of top');
eq(w.baseName('/tmp/usb/sda/sda1/Walls/'), 'Walls', 'basename');
eq(w.joinPath('/a/b/', 'c.jpg'), '/a/b/c.jpg', 'join');
eq(w.fileUrl("/tmp/usb/sda1/My Pics/it's #1 é.jpg"), "file:///tmp/usb/sda1/My%20Pics/it's%20%231%20%C3%A9.jpg", 'fileUrl encoding');
eq(w.isSafePath('/ok/path'), true, 'safe path');
eq(w.isSafePath('/bad\npath'), false, 'newline path rejected');
eq(w.isSafePath('relative/path'), false, 'relative rejected');
eq(w.isPicture('._x.jpg'), false, 'dotfile not picture');
eq(w.shuffled([1,2,3,4,5]).sort(), [1,2,3,4,5], 'shuffle keeps items');

// ---- reading a file through the shell in chunks (the wallpaper loader) ----
import {writeFileSync, readFileSync} from 'fs';
import {randomBytes, createHash} from 'crypto';
import {execSync} from 'child_process';
const run = (c) => Promise.resolve(String(execSync(c, {shell: '/bin/sh', maxBuffer: 100000000})));
const sha = (b) => createHash('sha256').update(b).digest('hex');
for (const size of [1, 98303, 98304, 98305, 1000003]) {
  const file = '/tmp/wp-chunk-test it\'s.jpg';           // a name with a space and a quote
  const data = randomBytes(size);
  writeFileSync(file, data);
  const outputs = [];
  const parts = await w.readFileChunks(file, (c) => run(c).then((o) => { outputs.push(o.length); return o; }));
  const joined = Buffer.concat(parts.map((p) => Buffer.from(p)));
  eq(sha(joined) === sha(data), true, `file of ${size} bytes comes back identical (${parts.length} chunks)`);
  eq(Math.max(...outputs) < 140000, true, `no single shell output over 140 KB (max ${Math.max(...outputs)})`);
}
let tooBig = null;
writeFileSync('/tmp/wp-big.bin', Buffer.alloc(9 * 1024 * 1024));
try { await w.readFileChunks('/tmp/wp-big.bin', run); } catch (e) { tooBig = e.problem; }
eq(tooBig, 'too-large', 'file over 8 MB is refused before reading');
let missing = null;
try { await w.readFileChunks('/tmp/definitely-missing-file.jpg', (c) => run(c + ' 2>/dev/null; true')); } catch (e) { missing = e.problem; }
eq(missing, 'unreadable', 'missing file reports unreadable');
let bad = null;
try { await w.readFileChunks('relative.jpg', run); } catch (e) { bad = e.problem; }
eq(bad, 'bad-path', 'unsafe path refused');
eq(w.mimeType('/a/b/Photo.PNG'), 'image/png', 'mime png');
eq(w.mimeType('/a/b/x.jpeg'), 'image/jpeg', 'mime jpeg');

// ---- picture sizing and the startup cache ----
eq(w.coverSize(4000, 3000), {width: 1920, height: 1440}, 'a 12 MP photo is drawn at cover size');
eq(w.coverSize(1920, 1080), {width: 1920, height: 1080}, 'a 1080p picture is left as is');
eq(w.coverSize(800, 600), {width: 800, height: 600}, 'a small picture is never enlarged');
eq(w.coverSize(6000, 2000), {width: 3240, height: 1080}, 'a panorama keeps its shape');
eq([w.isOversized(1920, 1080), w.isOversized(2800, 1500), w.isOversized(3840, 2160), w.isOversized(4000, 3000)], [false, false, true, true], 'only pictures well above the screen size are shrunk');
const mem = {}; globalThis.localStorage = {getItem: (k) => (k in mem ? mem[k] : null), setItem: (k, v) => { mem[k] = String(v); }, removeItem: (k) => { delete mem[k]; }};
const id = w.wallpaperCacheId('image', '/tmp/usb/a.jpg');
eq(w.readWallpaperCache(id), null, 'empty cache reads as null');
eq(w.writeWallpaperCache(id, 'data:image/jpeg;base64,AAAA'), true, 'cache write works');
eq(w.readWallpaperCache(id), 'data:image/jpeg;base64,AAAA', 'cache returns the saved picture');
eq(w.readWallpaperCache(w.wallpaperCacheId('image', '/tmp/usb/other.jpg')), null, 'a different choice does not get the old picture');
eq(w.readWallpaperCache(w.wallpaperCacheId('folder', '/tmp/usb/a.jpg')), null, 'mode is part of the key');
mem['customlauncher-wallpaper-data-v1'] = 'javascript:alert(1)';
eq(w.readWallpaperCache(id), null, 'a cache entry that is not an image is ignored');
w.clearWallpaperCache();
eq(w.readWallpaperCache(id), null, 'clear empties the cache');
globalThis.localStorage = {getItem: () => null, setItem: () => { throw new Error('quota'); }, removeItem: () => {}};
eq(w.writeWallpaperCache(id, 'data:image/jpeg;base64,AAAA'), false, 'a full storage reports failure instead of throwing');
// Every photo once per round, including reopening between every displayed photo.
const deckMemory = {};
const deckKey = 'customlauncher-wallpaper-deck-v1';
const storage = {
  getItem: (key) => deckMemory[key] || null,
  setItem: (key, value) => { deckMemory[key] = value; }
};
globalThis.localStorage = storage;
const photos = Array.from({length: 33}, (_, i) => `/photos/${i}.jpg`);
let previous = '';
for (let round = 0; round < 3; round++) {
  const seen = [];
  for (let i = 0; i < photos.length; i++) {
    const deck = w.createShuffleOrder(photos, 'family');
    const photo = deck.take();
    if (i === 0 && previous) eq(photo !== previous, true, 'round boundary avoids immediate repeat');
    seen.push(photo);
    deck.shown(photo);
    previous = photo;
  }
  eq([...new Set(seen)].sort(), [...photos].sort(), 'every photo appears once per round across reopen');
}
const saved = JSON.parse(deckMemory[deckKey]);
for (const state of [
  '{broken',
  JSON.stringify({...saved, remaining: [photos[0], photos[0]]}),
  JSON.stringify({...saved, remaining: ['/removed.jpg']}),
  JSON.stringify({...saved, remaining: [saved.last]}),
  JSON.stringify({...saved, remaining: [42]})
]) {
  deckMemory[deckKey] = state;
  const deck = w.createShuffleOrder(photos, 'family');
  const seen = Array.from({length: photos.length}, () => {
    const photo = deck.take();
    deck.shown(photo);
    return photo;
  });
  eq([...new Set(seen)].sort(), [...photos].sort(), 'invalid persisted state recovers a complete valid round');
}
let deck = w.createShuffleOrder(['/other/only.jpg'], 'other');
eq(deck.take(), '/other/only.jpg', 'changed folder rejects stale paths');
deck.shown('/other/only.jpg');
eq(w.createShuffleOrder(['/other/only.jpg'], 'other').take(), '/other/only.jpg', 'single photo remains usable after reopen');
eq(w.createShuffleOrder([], 'empty').take(), undefined, 'empty folder is total');
const changed = ['/new.jpg', photos[0]];
deck = w.createShuffleOrder(changed, 'family');
eq([deck.take(), deck.take()].sort(), [...changed].sort(), 'collection changes invalidate previous paths');
// Merely selecting a photo must not advance persisted progress before it is shown.
deck = w.createShuffleOrder(photos, 'family');
const displayed = deck.take();
deck.shown(displayed);
const unshown = deck.take();
eq(w.createShuffleOrder(photos, 'family').take(), unshown, 'interrupted load remains next after reopen');
for (const unreadable of [false, true]) {
  globalThis.localStorage = {
    getItem: () => { if (unreadable) throw new Error('blocked'); return null; },
    setItem: () => { throw new Error('quota'); }
  };
  deck = w.createShuffleOrder(photos, 'full');
  const seen = [];
  for (let i = 0; i < photos.length; i++) {
    const photo = deck.take();
    seen.push(photo);
    deck.shown(photo);
  }
  eq(new Set(seen).size, photos.length, 'storage failure preserves the in-memory round');
}
process.exit(fail ? 1 : 0);
