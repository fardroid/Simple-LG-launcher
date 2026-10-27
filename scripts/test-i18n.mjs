// Tests for the languages.   Run: npm test
// Every text written with tr() must have an entry in every language's dictionary, with the same {0} placeholders.
import fs from 'fs';
import path from 'path';
Object.defineProperty(globalThis, 'navigator', {value: {language: 'en-US', languages: ['en-US']}, configurable: true});
import {build} from 'esbuild';
const root = new URL('..', import.meta.url).pathname;
const dir = root + 'node_modules/';
await build({entryPoints: [root + 'src/lib/i18n.ts'], bundle: true, format: 'esm', outfile: dir + '.cache-i18n-test.mjs', platform: 'node', logLevel: 'error'});
const i = await import(dir + '.cache-i18n-test.mjs');
let fail = 0;
const eq = (a, b, m) => { const ok = JSON.stringify(a) === JSON.stringify(b); if (!ok) { fail++; console.log('FAIL', m, JSON.stringify(a), '!=', JSON.stringify(b)); } else console.log('ok  ', m); };

// the texts written in the sources
const Q = "('(?:[^'\\\\]|\\\\.)*'|\"(?:[^\"\\\\]|\\\\.)*\")";
const un = (q) => q.slice(1, -1).replace(/\\(.)/g, '$1');
const trRe = new RegExp('\\btr\\(\\s*' + Q + '\\s*,\\s*' + Q + '(?:\\s*,\\s*' + Q + ')?', 'g');
const countRe = new RegExp('\\bcount(?:Of)?\\([^,]+,\\s*\\[\\s*' + Q + '\\s*,\\s*' + Q + '\\s*\\]', 'g');
const keys = new Set();
(function walk (d) {
	for (const f of fs.readdirSync(d)) {
		const p = path.join(d, f);
		if (fs.statSync(p).isDirectory()) { if (f !== 'locales') walk(p); continue; }
		if (!/\.(ts|tsx)$/.test(p) || p.endsWith('i18n.ts')) continue;
		const s = fs.readFileSync(p, 'utf8');
		let m;
		while ((m = trRe.exec(s))) keys.add(un(m[3] || m[1]));
		while ((m = countRe.exec(s))) keys.add(un(m[2]));
	}
})(root + 'src');
const placeholders = (s) => (s.match(/\{\d+\}/g) || []).sort().join();

const others = i.LANG_SETTINGS.filter((l) => l !== 'auto' && l !== 'en' && l !== 'ru');
eq(others.length > 0 && keys.size > 100, true, 'found the texts to translate (' + keys.size + ') and the other languages (' + others.join(' ') + ')');
for (const code of others) {
	i.setLang(code);
	const missing = [], broken = [];
	const file = fs.readFileSync(root + 'src/lib/locales/' + code + '.ts', 'utf8');
	for (const key of keys) {
		if (file.indexOf("'" + key.replace(/\\/g, '\\\\').replace(/'/g, "\\'") + "':") < 0) missing.push(key);
		const got = i.tr(key, '');
		if (placeholders(got) !== placeholders(key) && !/^[a-z]+$/.test(key)) broken.push(key);
	}
	eq(missing, [], code + ' has every text');
	eq(broken, [], code + ' keeps the {0} placeholders');
}

// language detection
eq(['pt-BR', 'zh-Hans-CN', 'ja-JP', 'ar', '', 'ru_RU', 'UK'].map(i.fromTag), ['pt', 'zh', 'ja', 'en', 'en', 'ru', 'uk'], 'a TV language tag picks the launcher language');

// plurals
i.setLang('ru'); eq([1, 2, 5, 21].map((n) => i.count(n, ['day', 'days'], ['день', 'дня', 'дней'])), ['1 день', '2 дня', '5 дней', '21 день'], 'Russian plurals');
i.setLang('uk'); eq([1, 3, 11, 22].map((n) => i.count(n, ['day', 'days'], ['', '', ''])), ['1 день', '3 дні', '11 днів', '22 дні'], 'Ukrainian plurals');
i.setLang('de'); eq([1, 2].map((n) => i.count(n, ['day', 'days'], ['', '', ''])), ['1 Tag', '2 Tage'], 'German plurals');
i.setLang('fr'); eq([0, 1, 2].map((n) => i.count(n, ['day', 'days'], ['', '', ''])), ['0 jour', '1 jour', '2 jours'], 'French: 0 and 1 are singular');
i.setLang('ja'); eq(i.count(3, ['day', 'days'], ['', '', '']), '3日', 'Japanese has no plural and no space');
i.setLang('en'); eq(i.count(1, ['day', 'days'], ['', '', '']), '1 day', 'English singular');

// formatting and fallbacks
i.setLang('es'); eq(i.fmt(i.tr('Uninstall {0}?', ''), 'Netflix'), '¿Desinstalar Netflix?', 'a text with a name in it');
eq(i.tr('A text nobody translated', 'Непереведённый'), 'A text nobody translated', 'a missing text falls back to English');
eq(i.tr('Light', '', 'Light (weight)'), 'Ligera', 'a second meaning of the same English word has its own entry');
i.setLang('ru'); eq(i.tr('Today', 'Сегодня'), 'Сегодня', 'Russian is read from the pair');
eq(i.fmt('{1} / {0}', 'a', 'b'), 'b / a', 'placeholders fill in any order');

if (fail) { console.log(fail + ' failed'); process.exit(1); }
console.log('all passed');
