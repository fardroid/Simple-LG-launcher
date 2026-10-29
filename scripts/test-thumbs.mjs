// Tests for the previews of the wallpaper gallery (src/lib/thumbs.ts): the small preview found in a photo's EXIF block.   Run: npm test
import fs from 'fs';
import {build} from 'esbuild';
const root = new URL('..', import.meta.url).pathname;
const out = root + 'node_modules/.cache-thumbs-test.mjs';
// lib/apps.ts pulls in Vite-only code: a stub stands in for it (only the pure part is tested here)
await build({
	entryPoints: [root + 'src/lib/thumbs.ts'], bundle: true, format: 'esm', outfile: out, platform: 'node', logLevel: 'error',
	plugins: [{name: 'stub-apps', setup (b) {
		b.onResolve({filter: /\.\/apps$/}, () => ({path: 'apps-stub', namespace: 'stub'}));
		b.onLoad({filter: /.*/, namespace: 'stub'}, () => ({contents: 'export const exec = () => Promise.reject(new Error("no shell")); export const shellQuote = (v) => v;', loader: 'js'}));
	}}]
});
const t = await import(out);
let fail = 0;
const eq = (a, b, m) => { const ok = JSON.stringify(a) === JSON.stringify(b); if (!ok) { fail++; console.log('FAIL', m, JSON.stringify(a), '!=', JSON.stringify(b)); } else console.log('ok  ', m); };

// a photo as a camera writes it: SOI, an APP1 "Exif" block holding a small JPEG, then the picture's own tables
const inner = [0xFF, 0xD8, 0xFF, 0xDB, 0x00, 0x04, 1, 2, 0xFF, 0xD9];
const exifBody = [0x45, 0x78, 0x69, 0x66, 0, 0, 0x4D, 0x4D, 0, 0x2A, 0, 0, 0, 8, ...inner, 0, 0];
const app1 = [0xFF, 0xE1, (exifBody.length + 2) >> 8, (exifBody.length + 2) & 255, ...exifBody];
const photo = new Uint8Array([0xFF, 0xD8, 0xFF, 0xE0, 0, 4, 0x4A, 0x46, ...app1, 0xFF, 0xDB, 0, 4, 9, 9, 0xFF, 0xDA, 0, 2, 0xFF, 0xD9]);
eq(Array.from(t.exifThumbnail(photo) || []), inner, 'the small JPEG inside the EXIF block, after a JFIF block');
eq(t.exifThumbnail(photo.subarray(0, 30)), null, 'a head cut before the preview ends gives none');
const plain = new Uint8Array(fs.readFileSync(root + 'wallpapers/aurora.jpg')).subarray(0, 98304);
eq(t.exifThumbnail(plain), null, 'a picture without an EXIF preview gives none');
eq(t.exifThumbnail(new Uint8Array([0x89, 0x50, 0x4E, 0x47])), null, 'not a JPEG');
eq([t.THUMB.width / t.THUMB.height, t.cachedThumb('/nowhere.jpg')], [16 / 9, undefined], '16:9 previews; none made yet');

console.log(fail ? `\n${fail} FAILED` : '\nall passed');
process.exit(fail ? 1 : 0);
