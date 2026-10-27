// Tests for the plates behind the clock and the cards (src/lib/plates.ts).   Run: npm test
import {build} from 'esbuild';
const out = new URL('../node_modules/.cache-plates-test.mjs', import.meta.url).pathname;
await build({entryPoints: [new URL('../src/lib/plates.ts', import.meta.url).pathname], bundle: true, format: 'esm', outfile: out, platform: 'node', logLevel: 'error'});
const p = await import(out);
let fail = 0;
const eq = (a, b, m) => { const ok = JSON.stringify(a) === JSON.stringify(b); if (!ok) { fail++; console.log('FAIL', m, JSON.stringify(a), '!=', JSON.stringify(b)); } else console.log('ok  ', m); };

// a 64 x 36 picture: left half white, right half black
const px = new Uint8ClampedArray(64 * 36 * 4);
for (let y = 0; y < 36; y++) for (let x = 0; x < 64; x++) { const v = x < 32 ? 255 : 0, i = (y * 64 + x) * 4; px[i] = px[i + 1] = px[i + 2] = v; px[i + 3] = 255; }
const map = p.mapFromPixels(px);
eq([map.length, map[0], map[7], map[8], map[15]], [144, 1, 1, 0, 0], 'the picture becomes a 16 x 9 grid of brightness values');
const r = p.plateRects(true, true);
eq([r.clock, r.weather, r.calendar], [[200, 40, 880, 184], [200, 264, 880, 462], [1120, 40, 600, 686]], 'plates sit where the board puts them');
eq(p.plateRects(true, false).clock[0], 520, 'without the calendar the left column is centred');
eq([p.regionBrightness(map, r.weather) > 0.8, p.regionBrightness(map, r.calendar)], [true, 0], 'the weather plate is mostly over the bright half, the calendar over the dark half');
eq([p.lookFor(0).bg, p.lookFor(0.1).bg], ['rgba(255, 255, 255, 0.11)', 'rgba(255, 255, 255, 0.11)'], 'a dark background gets a light plate');
eq(p.lookFor(1).bg, 'rgba(14, 18, 40, 0.66)', 'a bright background gets a dark plate');
eq([p.lookFor(0.5).bg, p.lookFor(0.34).bg, p.lookFor(0.35).bg], ['rgba(14, 18, 40, 0.44)', 'rgba(255, 255, 255, 0.11)', 'rgba(14, 18, 40, 0.31)'], 'in between, the dark plate grows stronger with the brightness');
const looks = p.plateLooks('auto', 'dusk', map, true, true);
eq([looks.weather.bg.indexOf('14, 18, 40') > 0, looks.calendar.bg === looks.weather.bg, looks.clock.bg === looks.weather.bg], [true, true, true], 'auto: one plate over a bright part makes all of them dark, the same colour');
const darkLeft = map.map((v, i) => (i % 16 < 8 ? 0 : v));      // now the left half is dark too: everything is dark
eq(p.plateLooks('auto', 'dusk', darkLeft, true, true).calendar.bg, 'rgba(255, 255, 255, 0.11)', 'auto: all over dark, all plates light');
eq(p.plateLooks('auto', 'dusk', map, true, false).calendar.bg, p.plateLooks('auto', 'dusk', map, true, false).clock.bg, 'auto: the same with the calendar off');
eq(p.plateLooks('light', 'dusk', map, true, true).weather.bg, 'rgba(255, 255, 255, 0.11)', 'light: always the light plate');
eq(p.plateLooks('dark', 'dusk', map, true, true).calendar.bg, 'rgba(12, 18, 46, 0.55)', 'dark: always the dark plate');
const black = p.themeMap('black');
eq([black.length, Math.max.apply(null, black)], [144, 0], 'the black theme is dark everywhere');
eq(p.plateLooks('auto', 'black', null, true, true).clock.bg, 'rgba(255, 255, 255, 0.11)', 'on the black theme the plates are light');
const dusk = p.themeMap('dusk');
eq(dusk[0] < dusk[143], true, 'the dusk theme gets brighter towards the bottom right');
eq(p.plateLooks('auto', 'dusk', null, true, true).clock.bg, 'rgba(255, 255, 255, 0.11)', 'on the dusk theme the plates are light');

// frosted glass: the plates get the blurred wallpaper and their own place on the screen
const glass = p.plateLooks('auto', 'dusk', map, true, true, 'data:image/jpeg;base64,AA==');
eq([glass.clock.frost, glass.clock.x, glass.clock.y, glass.calendar.x, glass.calendar.y], ['data:image/jpeg;base64,AA==', 200, 40, 1120, 40], 'with glass each plate knows where it is on the screen');
eq(p.plateLooks('auto', 'dusk', map, true, true, null).clock.frost, undefined, 'without a blurred picture there is no glass');
eq(p.plateLooks('light', 'dusk', map, true, true, 'x').weather.y, 264, 'the glass also works with fixed light plates');
const vars = p.plateVars(glass.weather);
eq([vars['--frost'], vars['--fx'], vars['--fy']], [undefined, '200px', '264px'], 'the card gets the place of the glass as CSS variables (the picture is shown by <Frost>)');
eq(Object.keys(p.plateVars(p.plateLooks('auto', 'dusk', map, true, true).weather)).sort(), ['--pb', '--pf'], 'no glass: only the colours');
eq(p.plateVars(undefined), {}, 'no plate: nothing');
console.log(fail ? `\n${fail} FAILED` : '\nall passed');
process.exit(fail ? 1 : 0);
