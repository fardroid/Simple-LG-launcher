// The picture the TV shows while the launcher starts (appinfo.json's splashBackground) is the wallpaper on screen, as in
// Luma (ecklf/lg-luma, tv/sync-splash.cjs): whenever the launcher keeps a screen-sized copy of its wallpaper for its own
// fast start (writeWallpaperCache), the same JPEG is written over splash-wallpaper.jpg in the installed app's folder through
// the Homebrew Channel's root shell. Pressing Home after the TV has unloaded the launcher then shows the wallpaper at once,
// not the logo card. A reinstall brings back the packaged default (the logo card, also kept as splash-default.jpg), and
// the default background puts that back too.
//
// Settings → Wallpaper → Start-up picture chooses it: the wallpaper (the default), the logo card, or plain black.

import {exec} from './apps';
import {APP_ID} from '../config/constants';
import {isWebOS} from './luna';

export const SPLASH_FILE = `/media/developer/apps/usr/palm/applications/${APP_ID}/splash-wallpaper.jpg`;
const DEFAULT_FILE = `/media/developer/apps/usr/palm/applications/${APP_ID}/splash-default.jpg`;
const DONE_KEY = 'launcher-splash-written';
/** base64 characters per command: a few calls for a screen-sized JPEG, and each piece decodes on its own (a multiple of 4) */
const PIECE = 32000;

/** A short fingerprint of a data URL, to skip writing the same picture again. */
export function splashKey (data: string): string {
	return `${data.length}:${data.slice(-48)}`;
}

/** Commands that write base64 `encoded` to `path`, through a temporary file moved into place at the end, so the TV
 *  never reads a half-written picture. */
export function splashCommands (encoded: string, path: string): string[] {
	const pieces: string[] = [];
	for (let i = 0; i < encoded.length; i += PIECE) pieces.push(encoded.slice(i, i + PIECE));
	const commands = pieces.map((piece, n) => `echo '${piece}' | base64 -d ${n === 0 ? '>' : '>>'} '${path}.new'`);
	commands.push(`[ -s '${path}.new' ] && mv -f '${path}.new' '${path}'`);
	return commands;
}

function remembered (): string | null {
	try { return localStorage.getItem(DONE_KEY); } catch { return null; }
}

function remember (key: string): void {
	try { localStorage.setItem(DONE_KEY, key); } catch { /* the next change just writes it again */ }
}

let running: Promise<void> = Promise.resolve();

/** Only on the TV (and not in the tests, which have no window). */
const onTv = (): boolean => typeof window !== 'undefined' && isWebOS();

export type SplashMode = 'wallpaper' | 'logo' | 'black';
let mode: SplashMode = 'wallpaper';

/** The launcher saved a screen-sized copy of its wallpaper: with the wallpaper as start-up picture, it is written too. */
export function wallpaperSaved (dataUrl: string): void {
	if (mode === 'wallpaper') writeSplash(dataUrl);
}

/** The wallpaper is the default background now (no picture of its own). */
export function wallpaperCleared (): void {
	if (mode === 'wallpaper') resetSplash();
}

/** Notes the setting (cheap: called while drawing, so it is right before the wallpaper saves anything). */
export function noteSplashMode (next: SplashMode): void {
	mode = next;
}

/** Applies the setting now: `wallpaper` is the screen-sized copy on screen (null when there is none). */
export function applySplashMode (next: SplashMode, wallpaper: string | null): void {
	mode = next;
	if (next === 'logo' || (next === 'wallpaper' && !wallpaper)) resetSplash();
	else if (next === 'wallpaper' && wallpaper) writeSplash(wallpaper);
	else if (next === 'black') { const black = blackJpeg(); if (black) writeSplash(black); }
}

/** A screen-sized black JPEG (it compresses to a few kB). */
function blackJpeg (): string | null {
	try {
		const canvas = document.createElement('canvas');
		canvas.width = 1920;
		canvas.height = 1080;
		const context = canvas.getContext('2d');
		if (!context) return null;
		context.fillStyle = '#000';
		context.fillRect(0, 0, 1920, 1080);
		return canvas.toDataURL('image/jpeg', 0.9);
	} catch { return null; }
}

/** Writes this JPEG data URL as the start-up picture (in the background, one at a time; failures are left alone). */
function writeSplash (dataUrl: string): void {
	if (!onTv()) return;
	const comma = dataUrl.indexOf(',');
	if (dataUrl.indexOf('data:image/jpeg;base64,') !== 0 || comma < 0) return;
	const key = splashKey(dataUrl);
	if (remembered() === key) return;
	const commands = splashCommands(dataUrl.slice(comma + 1), SPLASH_FILE);
	running = running.then(() => commands.reduce<Promise<unknown>>((done, command) => done.then(() => exec(command, 15000)), Promise.resolve())
		.then(() => remember(key), () => undefined));
}

/** The packaged start-up picture back (the default background has no picture of its own). */
function resetSplash (): void {
	if (!onTv() || remembered() === 'default') return;
	running = running.then(() => exec(`cp -f '${DEFAULT_FILE}' '${SPLASH_FILE}.new' && mv -f '${SPLASH_FILE}.new' '${SPLASH_FILE}'`, 8000)
		.then(() => remember('default'), () => undefined));
}
