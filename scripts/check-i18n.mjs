// Every text the launcher shows through tr(en, ru) has a translation in each of the other languages (src/i18n/locales/*.json).
// Run by make test.
//   node scripts/check-i18n.mjs

import {readdirSync, readFileSync, statSync} from 'node:fs';
import {join} from 'node:path';

const root = new URL('..', import.meta.url).pathname;
const locales = join(root, 'src/i18n/locales');
const dict = (lang) => JSON.parse(readFileSync(join(locales, lang + '.json'), 'utf8'));
const files = [];
const walk = (d) => { for (const f of readdirSync(d)) { const p = join(d, f); if (statSync(p).isDirectory()) walk(p); else if (/\.(cpp|h)$/.test(f)) files.push(p); } };
walk(join(root, 'src'));
const lit = String.raw`"((?:[^"\\]|\\.)*)"`;
const re = new RegExp(String.raw`\btr\(\s*` + lit + String.raw`\s*,\s*` + lit, 'g');
const keys = new Set();
for (const f of files) for (const m of readFileSync(f, 'utf8').matchAll(re)) keys.add(JSON.parse(`"${m[1]}"`));
let missing = 0;
for (const lang of readdirSync(locales).filter((f) => f.endsWith('.json')).map((f) => f.replace('.json', ''))) {
	const d = dict(lang);
	for (const k of keys) if (!(k in d)) { console.log(`${lang}: no translation of ${JSON.stringify(k)}`); missing++; }
}
if (missing) process.exit(1);
console.log(`check-i18n: ${keys.size} texts, all translated`);
