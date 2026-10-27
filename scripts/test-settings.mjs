// Tests for the settings model (src/lib/storage.ts, src/lib/prefs.ts).   Run: npm test
// The texts are checked in Russian (the screens speak the TV's language).
Object.defineProperty(globalThis, 'navigator', {value: {language: 'ru-RU', languages: ['ru-RU']}, configurable: true});
import {build} from 'esbuild';

const out = new URL('../node_modules/.cache-settings-test.mjs', import.meta.url).pathname;
await build({
  entryPoints: [new URL('../src/lib/prefs.ts', import.meta.url).pathname], bundle: true, format: 'esm', outfile: out, platform: 'node', logLevel: 'error',
  plugins: [{name: 'stub-apps', setup(b) {
    b.onResolve({filter: /\.\/apps$/}, () => ({path: 'apps-stub', namespace: 'stub'}));
    b.onLoad({filter: /.*/, namespace: 'stub'}, () => ({contents: 'export const exec = () => Promise.resolve(""); export const shellQuote = (v) => v;', loader: 'js'}));
  }}]
});
const storage = await build({entryPoints: [new URL('../src/lib/storage.ts', import.meta.url).pathname], bundle: true, format: 'esm', write: false, platform: 'node', logLevel: 'error'});
const storageOut = new URL('../node_modules/.cache-storage-test.mjs', import.meta.url).pathname;
(await import('fs')).writeFileSync(storageOut, storage.outputFiles[0].text);

// a tiny localStorage for loadConfig
const store = {};
globalThis.localStorage = {getItem: (k) => (k in store ? store[k] : null), setItem: (k, v) => { store[k] = String(v); }};

const winOut = new URL('../node_modules/.cache-optionwindow-test.mjs', import.meta.url).pathname;
await build({entryPoints: [new URL('../src/lib/optionwindow.ts', import.meta.url).pathname], bundle: true, format: 'esm', outfile: winOut, platform: 'node', logLevel: 'error'});
const ow = await import(winOut);
const p = await import(out);
const s = await import(storageOut);
let fail = 0;
const eq = (a, b, m) => { const ok = JSON.stringify(a) === JSON.stringify(b); if (!ok) { fail++; console.log('FAIL', m, JSON.stringify(a), '!=', JSON.stringify(b)); } else console.log('ok  ', m); };
const KEY = 'customlauncher-config-v1';

// ---- defaults and loading ----
const d = s.loadConfig();
eq([d.theme, d.iconScale, d.sideMargin, d.bottomMargin, d.clockFormat, d.clock.size, d.clock.weight], ['dusk', 100, 80, 50, '24h', 160, 200], 'defaults: evening sky, 24-hour, a 160 px clock');
store[KEY] = JSON.stringify({theme: 'grey', iconScale: 999, sideMargin: 33, bottomMargin: -5, clock: {size: 12, top: 29, weight: 999, color: 'neon', showDate: false}, wallpaper: {mode: 'folder', path: '/x', intervalSec: 2}});
const g = s.loadConfig();
eq([g.theme, g.iconScale, g.sideMargin, g.bottomMargin], ['grey', 140, 30, 0], 'out-of-range numbers are clamped and snapped');
eq([g.clock.size, g.clock.weight, g.clock.color, g.clock.showDate, g.clock.showPeriod], [80, 200, 'white', false, true], 'clock values validated, missing ones default');
eq([g.wallpaper.mode, g.wallpaper.intervalSec], ['folder', 60], 'bad interval falls back');
store[KEY] = '{not json';
eq(s.loadConfig().theme, 'dusk', 'corrupt storage falls back to defaults');
store[KEY] = JSON.stringify({wallpaper: {mode: 'folder', path: ''}});
eq(s.loadConfig().wallpaper.mode, 'gradient', 'folder mode without a path becomes default background');

store[KEY] = JSON.stringify({tileOrder: ['com.webos.app.hdmi1', 'x', 7]});
eq(s.loadConfig().tileOrder, ['com.webos.app.hdmi1', 'x'], 'a saved tile order is loaded (bad entries dropped)');
store[KEY] = JSON.stringify({tileOrder: 'nope'});
eq(s.loadConfig().tileOrder, null, 'a bad tile order becomes none');

// ---- apps: shown unless hidden ----
store[KEY] = JSON.stringify({appIds: ['a', 'b']});
eq(s.loadConfig().hiddenIds, null, 'settings from an older version (a list of apps to show) are flagged to be converted');
store[KEY] = JSON.stringify({});
eq(s.loadConfig().hiddenIds, [], 'new settings: nothing hidden');
store[KEY] = JSON.stringify({appIds: null});
eq(s.loadConfig().hiddenIds, [], 'older settings that showed every app: nothing hidden');
store[KEY] = JSON.stringify({appIds: ['a'], hiddenIds: ['x']});
eq(s.loadConfig().hiddenIds, ['x'], 'a saved hide-list is kept');
store[KEY] = JSON.stringify({rows: 3, showLabels: false});
eq([s.loadConfig().rows, s.loadConfig().showLabels], [1, false], 'rows are 1 or 2 only; the app-names switch is read');
store[KEY] = JSON.stringify({rows: 2});
eq([s.loadConfig().rows, s.loadConfig().showLabels], [2, true], 'two rows are kept; app names default to shown');
eq([s.DEFAULT_CONFIG.hiddenIds, s.DEFAULT_CONFIG.rows, s.DEFAULT_CONFIG.showLabels], [[], 1, true], 'defaults: nothing hidden, one row, names shown');
store[KEY] = '{not json';

// ---- stepping ----
eq(s.stepValue([1, 2, 3], 3, 1, true), 1, 'OK wraps round');
eq(s.stepValue([1, 2, 3], 3, 1, false), 3, 'arrow stops at the end');
eq(s.stepValue([1, 2, 3], 1, -1, false), 1, 'arrow stops at the start');
eq(s.stepValue([1, 2, 3], 99, 1, true), 2, 'unknown value starts from the first');

// ---- prefs ----
const c0 = s.DEFAULT_CONFIG;
eq(p.applyPref(c0, 'iconScale', 'right').iconScale, 110, 'right arrow: icon size 100 -> 110');
eq(p.applyPref(c0, 'iconScale', 'left').iconScale, 90, 'left arrow: icon size 100 -> 90');
eq(p.applyPref({...c0, iconScale: 140}, 'iconScale', 'right').iconScale, 140, 'right arrow stops at the maximum');
eq(p.applyPref({...c0, iconScale: 140}, 'iconScale', 'ok').iconScale, 50, 'OK wraps to the minimum (50%)');
eq(p.applyPref(c0, 'theme', 'ok').theme, 'black', 'OK steps to the next theme');
eq(p.applyPref(c0, 'showDate', 'ok').clock.showDate, false, 'OK flips a switch');
eq(p.applyPref(c0, 'showDate', 'right') === c0, true, 'arrows leave a switch alone');
eq(p.applyPref(c0, 'clockSize', 'right').clock.size, 170, 'clock size steps by 10');
eq(p.applyPref(c0, 'clockWeight', 'ok').clock.weight, 300, 'thickness: thin -> light');
eq(p.applyPref(c0, 'wallpaper', 'ok') === c0, true, 'the wallpaper row is an action, not a value');
eq(p.applyPref(c0, 'nonsense', 'ok') === c0, true, 'unknown option does nothing');
const messy = {...c0, theme: 'grey', iconScale: 70, sideMargin: 0, bottomMargin: 200, clockFormat: '24h', clock: {...c0.clock, size: 220, color: 'amber'}, appIds: ['a'], wallpaper: {...c0.wallpaper, mode: 'image', path: '/p.jpg'}};
const r = p.resetLook(messy);
eq([r.theme, r.iconScale, r.sideMargin, r.bottomMargin, r.clockFormat, r.clock.size, r.clock.color], ['dusk', 100, 80, 50, '24h', 160, 'white'], 'reset restores the look and clock');
eq([r.appIds, r.wallpaper.mode], [['a'], 'image'], 'reset keeps apps and wallpaper');

// ---- the lists shown in Settings ----
for (const tab of ['general', 'look', 'clock', 'wallpaper', 'home']) {
  const list = p.optionsFor(tab, c0);
  eq(list.length >= 1 && list.length <= 12, true, `${tab} tab has ${list.length} options (a long list scrolls)`);
}
const look = p.optionsFor('look', c0);
eq(look.map((o) => o.valueText), ['100%', undefined, '80 px', '50 px', '100%', 'Авто', undefined], 'Appearance shows current values (switches have no text; no frosted glass without a picture)');
eq(look.map((o) => o.group), ['Ряд приложений', 'Ряд приложений', 'Ряд приложений', 'Ряд приложений', 'Карточки', 'Карточки', 'Карточки'], 'Appearance is in two groups');
eq(p.optionsFor('look', {...c0, wallpaper: {...c0.wallpaper, mode: 'image', path: '/a.jpg'}}).some((o) => o.id === 'glass'), true, 'frosted glass appears with a picture');
eq(look.some((o) => o.id === 'rows'), false, 'no choice of two rows of apps (they leave no room for the cards)');
eq([p.applyPref(c0, 'shadows', 'ok').shadows, c0.shadows], [false, true], 'the card shadow is on by default and OK turns it off');
eq(p.applyPref(c0, 'showLabels', 'ok').showLabels, false, 'Show app names switches off');
eq(p.optionsFor('home', c0).map((o) => o.id).join(','), 'homeStatus,replaceHome', 'Home screen: whether the launcher is in LG Home\'s place, and the helper');
eq(p.optionsFor('general', c0).map((o) => o.id).join(','), 'language,resetLook,about', 'General: language, reset, about');
eq(c0.tileOrder, null, 'a new config has no saved tile order');
eq(p.optionsFor('home', c0)[1].label, 'Фоновый помощник', 'the helper says what it is');
// ---- the scrolling window ----
const H = [128, 128, 128, 128, 128, 100, 100];                 // seven rows, taller than 800 px together
eq(ow.windowEnd(H, 0), 6, 'at the top, six of the seven rows fit');
eq(ow.windowStart(H, 6, 0), 1, 'moving down to the last row scrolls the list so it is visible');
eq(ow.windowEnd(H, 1), 7, 'and then the last row is shown');
eq(ow.windowStart(H, 5, 1), 1, 'moving up inside the window does not scroll');
eq(ow.windowStart(H, 0, 1), 0, 'moving above the window scrolls up');
eq(ow.windowStart(H, 0, 0), 0, 'and the top is the top');
eq(ow.windowStart([100, 100], 1, 0), 0, 'a short list never scrolls');
eq(ow.windowEnd([2000, 100], 0), 1, 'a single very tall row is still shown');
eq([ow.optionHeight('short'), ow.optionHeight('x'.repeat(100)), ow.optionHeight('x'.repeat(200))].join(','), '111,139,167', 'row height grows with the description length');
eq(p.applyPref({...c0, wallpaper: {...c0.wallpaper, intervalSec: 900}}, 'interval', 'ok').wallpaper.intervalSec, 1800, 'the slideshow interval goes 15 minutes -> 30 minutes');
eq(p.applyPref({...c0, wallpaper: {...c0.wallpaper, intervalSec: 1800}}, 'interval', 'ok').wallpaper.intervalSec, 3600, 'and 30 minutes -> 1 hour');
eq(p.intervalText(1800), '30 минут', 'the 30-minute interval reads "30 minutes"');
eq(p.intervalText(60), '1 минута', 'interval text');
eq(p.intervalText(3600), '1 час', 'interval text hour');

// ---- the clock font ----
eq(d.clock.font, 'inter', 'default clock font is Inter');
store[KEY] = JSON.stringify({clock: {font: 'oswald'}});
eq(s.loadConfig().clock.font, 'oswald', 'a saved clock font is loaded');
store[KEY] = JSON.stringify({clock: {font: 'comic-sans'}});
eq(s.loadConfig().clock.font, 'inter', 'an unknown clock font falls back to Inter');
store[KEY] = JSON.stringify({clock: {size: 100}});
eq(s.loadConfig().clock.font, 'inter', 'settings from an older version (no font) get Inter');
const fonts = [];
let cf = c0;
for (let i = 0; i < 6; i++) { cf = p.applyPref(cf, 'clockFont', 'ok'); fonts.push(cf.clock.font); }
eq(fonts, ['tv', 'poppins', 'oswald', 'mono', 'serif', 'inter'], 'OK steps through the six fonts and wraps');
eq([p.applyPref(c0, 'clockFont', 'left').clock.font, p.applyPref(c0, 'clockFont', 'right').clock.font], ['inter', 'tv'], 'right goes to the next font; left stops at the first');
eq(p.optionsFor('clock', {...c0, clock: {...c0.clock, font: 'mono'}}).filter((o) => o.id === 'clockFont')[0].valueText, 'Roboto Mono (цифровой)', 'the font option reads its name');
eq(p.resetLook({...c0, clock: {...c0.clock, font: 'serif'}}).clock.font, 'inter', 'Reset look and clock puts the font back');


// widget colour, opacity, ranges, reset row
const wall = p.optionsFor('wallpaper', c0).map((o) => o.id).join(',');
eq(wall, 'wallpaper,theme', 'Wallpaper without a slideshow: the picture and the colour theme');
eq(p.optionsFor('wallpaper', {...c0, wallpaper: {...c0.wallpaper, mode: 'folder', path: '/w/'}}).map((o) => o.id).join(','), 'wallpaper,interval,shuffle,theme', 'a slideshow adds its interval and order');
eq(p.optionsFor('clock', c0).map((o) => o.id).join(','), 'clockFormat,clockSize,clockFont,clockWeight,clockColor,showDate,dateSize', 'Clock: the time, then the date');
eq(p.optionsFor('clock', {...c0, clock: {...c0.clock, showDate: false}}).some((o) => o.id === 'dateSize'), false, 'no date size without a date');

eq(p.applyPref({...c0, sideMargin: 290}, 'sideMargin', 'right').sideMargin, 300, 'side margin goes up to 300 px');
const colour = p.applyPref(c0, 'clockColor', 'ok').clock.color;
eq(colour, 'warm', 'OK steps to the next colour');
eq(p.optionsFor('clock', {...c0, clock: {...c0.clock, color: 'ocean'}}).find((o) => o.id === 'clockColor').valueText.indexOf('Градиент') === 0, true, 'a gradient shows its name');
eq(p.resetLook({...c0, clock: {...c0.clock, color: 'black', opacity: 30}}).clock.opacity, 100, 'reset puts the opacity back');
eq(p.resetLook({...c0, clock: {...c0.clock, color: 'black', opacity: 30}}).clock.color, 'white', 'and the colour');
process.exit(fail ? 1 : 0);
