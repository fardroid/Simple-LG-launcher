// Draws the line icons (the SVG bodies in data/glyphs.json, 24 x 24, drawn with a stroke) into white PNGs with transparency:
// assets/glyphs/<name>.png. The launcher tints them when it draws them. Uses the Google Chrome installed on this Mac, headless.
// Run: node scripts/gen-glyphs.mjs

import {execFileSync} from 'node:child_process';
import {mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync} from 'node:fs';
import {tmpdir} from 'node:os';
import {join} from 'node:path';

const CHROME = '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
const out = new URL('../assets/glyphs/', import.meta.url).pathname;
mkdirSync(out, {recursive: true});

// the weather icons' cloud is a variable in the source
const CLOUD = 'M7 18.5h9.5a3.7 3.7 0 0 0 .4-7.4 5.3 5.3 0 0 0-10.2 1.3A3.1 3.1 0 0 0 7 18.5z';
const SIZE = 192;
const tmp = mkdtempSync(join(tmpdir(), 'glyphs-'));
const all = JSON.parse(readFileSync(new URL('../data/glyphs.json', import.meta.url), 'utf8'));
for (const [name, body0] of Object.entries(all)) {
	const body = body0.replace('d={cloud}', `d="${CLOUD}"`).replace(/^<>|<\/>$/g, '');
	// .tile__glyph: stroke 1.6, round caps and joins; the others (weather, picker) use the same line art
	const svg = `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="${SIZE}" height="${SIZE}" fill="none" stroke="#fff" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round">${body}</svg>`;
	const html = `<html><body style="margin:0;background:transparent">${svg}</body></html>`;
	const file = join(tmp, name + '.html');
	writeFileSync(file, html);
	execFileSync(CHROME, ['--headless=new', '--disable-gpu', '--hide-scrollbars', `--window-size=${SIZE},${SIZE}`,
		'--default-background-color=00000000', `--screenshot=${join(out, name + '.png')}`, 'file://' + file], {stdio: 'ignore'});
}
rmSync(tmp, {recursive: true});
console.log(`assets/glyphs: ${Object.keys(all).sort().join(' ')}`);
