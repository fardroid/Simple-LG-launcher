// Finding and loading wallpaper pictures: USB drives, folder listings and image loading.
//
// A sideloaded web app on webOS cannot list directories, and its web view cannot load
// pictures from file:// paths. So both go through the Homebrew Channel's root shell, with
// fixed commands only (every path is checked and shell-quoted first):
//   - folders are listed with `ls -1p`;
//   - a picture is read in chunks with `dd | base64` (the same proven route the app list
//     uses for icons), put back together in memory as a Blob, and shown from a blob: URL.
//     Chunks are small because the shell service caps how much one call may print.

import {WALLPAPER} from '../config/constants';
import {exec, shellQuote} from './apps';
import {isWebOS} from './luna';
import {mapFromPixels} from './plates';
import {fmt, tr} from './i18n';

export interface DirEntry {
	name: string;
	isDir: boolean;
}

export interface StorageRoot {
	label: string;
	path: string;
}

const IMAGE_NAME = /\.(jpe?g|png|webp|bmp)$/i;
/** Control characters (including newline) can never be part of a path we pass to a shell. */
const CONTROL_CHARS = /[\u0000-\u001f\u007f]/;

export const INTERNAL_ROOT: StorageRoot = {get label () { return tr('TV storage', 'Память ТВ'); }, path: '/media/developer'};

// ---------- path helpers ----------

export function isSafePath (path: string): boolean {
	return path.charAt(0) === '/' && !CONTROL_CHARS.test(path);
}

export function isPicture (name: string): boolean {
	return name.charAt(0) !== '.' && IMAGE_NAME.test(name);
}

export function joinPath (base: string, name: string): string {
	return `${base.replace(/\/+$/, '')}/${name}`;
}

export function parentPath (path: string): string {
	const trimmed = path.replace(/\/+$/, '');
	const i = trimmed.lastIndexOf('/');
	return i <= 0 ? '/' : trimmed.slice(0, i);
}

export function baseName (path: string): string {
	const trimmed = path.replace(/\/+$/, '');
	return trimmed.slice(trimmed.lastIndexOf('/') + 1) || trimmed;
}

export function fileUrl (path: string): string {
	const encoded = path.split('/').map(encodeURIComponent).join('/');
	// the desktop preview on Vite's dev server can't open file:// pictures; it serves the project's files under /@fs/
	return import.meta.env && import.meta.env.DEV ? '/@fs' + encoded : 'file://' + encoded;
}

export function shuffled<T> (list: T[]): T[] {
	const out = list.slice();
	for (let i = out.length - 1; i > 0; i--) {
		const j = Math.floor(Math.random() * (i + 1));
		const tmp = out[i]; out[i] = out[j]; out[j] = tmp;
	}
	return out;
}

// One persisted deck per selected folder, consumed only as photos are loaded.
const SHUFFLE_KEY = 'customlauncher-wallpaper-deck-v1';

export function createShuffleOrder (files: string[], folderId: string) {
	const id = JSON.stringify([folderId, files]);
	let remaining: string[] = [];
	let last = '';
	try {
		const saved: unknown = JSON.parse(localStorage.getItem(SHUFFLE_KEY) || 'null');
		if (saved && typeof saved === 'object' && 'id' in saved && saved.id === id &&
			'remaining' in saved && Array.isArray(saved.remaining) &&
			'last' in saved && typeof saved.last === 'string' && files.indexOf(saved.last) >= 0) {
			const valid = saved.remaining.filter((path): path is string => typeof path === 'string' && files.indexOf(path) >= 0);
			if (valid.length === saved.remaining.length && new Set(valid).size === valid.length && valid.indexOf(saved.last) < 0) {
				remaining = valid;
				last = saved.last;
			}
		}
	} catch { /* unavailable or corrupt storage: start a fresh deck */ }
	return {
		take (): string | undefined {
			if (!remaining.length) {
				remaining = shuffled(files);
				if (remaining.length > 1 && remaining[0] === last) {
					const swap = 1 + Math.floor(Math.random() * (remaining.length - 1));
					const first = remaining[0];
					remaining[0] = remaining[swap];
					remaining[swap] = first;
				}
			}
			return remaining.shift();
		},
		shown (path: string) {
			last = path;
			try { localStorage.setItem(SHUFFLE_KEY, JSON.stringify({id, remaining, last})); }
			catch { /* keep the in-memory deck if storage is full */ }
		}
	};
}

// ---------- listing ----------

/** Turns the output of `ls -1p` into entries: folders first, then pictures, each A–Z.
 *  Hidden files (including the "._" files macOS leaves on USB drives) and other file
 *  types are dropped. */
export function parseListing (output: string): DirEntry[] {
	const dirs: DirEntry[] = [];
	const pictures: DirEntry[] = [];
	for (const line of output.split('\n')) {
		const name = line.replace(/\r$/, '');
		if (!name || name.charAt(0) === '.') continue;
		if (name.charAt(name.length - 1) === '/') dirs.push({name: name.slice(0, -1), isDir: true});
		else if (isPicture(name)) pictures.push({name, isDir: false});
	}
	const byName = (a: DirEntry, b: DirEntry) => a.name.toLowerCase() < b.name.toLowerCase() ? -1 : a.name.toLowerCase() > b.name.toLowerCase() ? 1 : 0;
	return dirs.sort(byName).concat(pictures.sort(byName));
}

export function listDir (path: string): Promise<DirEntry[]> {
	if (!isWebOS()) return Promise.resolve(mockListing(path));
	if (!isSafePath(path)) return Promise.reject(new Error('Unsafe path'));
	return exec(`ls -1p -- ${shellQuote(path)} 2>/dev/null`).then(parseListing);
}

/** Full paths of the pictures directly inside `path` (not its sub-folders). */
export function listImages (path: string): Promise<string[]> {
	return listDir(path).then((entries) =>
		entries.filter((e) => !e.isDir).slice(0, WALLPAPER.maxFiles).map((e) => joinPath(path, e.name))
	);
}

/** Reads /proc/mounts and returns the mounted USB drives, plus the TV's own developer
 *  storage (where pictures can also be put with scp). */
export function parseMounts (output: string): StorageRoot[] {
	const roots: StorageRoot[] = [];
	for (const line of output.split('\n')) {
		const parts = line.split(' ');
		if (parts.length < 3) continue;
		const mount = parts[1].replace(/\\040/g, ' ');
		const isUsb = /^\/dev\/sd[a-z]/.test(parts[0]) || /\/usb\//i.test(mount);
		if (!isUsb || !isSafePath(mount) || roots.some((r) => r.path === mount)) continue;
		roots.push({label: tr('USB drive', 'USB-накопитель'), path: mount});
	}
	if (roots.length > 1) roots.forEach((r, i) => { r.label = fmt(tr('USB drive {0}', 'USB-накопитель {0}'), i + 1); });
	return roots;
}

export function listRoots (): Promise<StorageRoot[]> {
	if (!isWebOS()) return Promise.resolve([{label: tr('USB drive', 'USB-накопитель'), path: '/tmp/usb/sda/sda1'}, INTERNAL_ROOT]);
	// Rejects when the shell service isn't reachable, so the picker can say why it is empty.
	return exec('cat /proc/mounts').then((out) => parseMounts(out).concat(INTERNAL_ROOT));
}

// ---------- loading a picture ----------

export type PictureProblem = 'bad-path' | 'unreadable' | 'too-large' | 'too-many-pixels' | 'not-a-picture';

export interface PictureError extends Error {
	problem: PictureProblem;
}

function pictureError (problem: PictureProblem, message: string): PictureError {
	const error = new Error(message) as PictureError;
	error.problem = problem;
	return error;
}

const MIME_TYPES: Record<string, string> = {jpg: 'image/jpeg', jpeg: 'image/jpeg', png: 'image/png', webp: 'image/webp', bmp: 'image/bmp'};

export function mimeType (path: string): string {
	const extension = (/\.([A-Za-z0-9]+)$/.exec(path) || [])[1];
	return (extension && MIME_TYPES[extension.toLowerCase()]) || 'image/jpeg';
}

type Run = (command: string, timeoutMs?: number) => Promise<string>;

function decodeBase64 (text: string): Uint8Array {
	const binary = atob(text);
	const bytes = new Uint8Array(binary.length);
	for (let i = 0; i < binary.length; i++) bytes[i] = binary.charCodeAt(i);
	return bytes;
}

/** Reads a file through the shell, `chunkBytes` per call, a few calls at a time.
 *  Resolves with the pieces in order. `run` is the shell (injectable for tests). */
export function readFileChunks (path: string, run: Run = exec): Promise<Uint8Array[]> {
	if (!isSafePath(path)) return Promise.reject(pictureError('bad-path', tr('This file name cannot be used', 'Это имя файла нельзя использовать')));
	const quoted = shellQuote(path);
	const chunk = WALLPAPER.chunkBytes;

	return run(`wc -c < ${quoted}`).then((out) => {
		const size = parseInt(out.trim(), 10);
		if (!(size > 0)) throw pictureError('unreadable', tr('The picture cannot be read', 'Не удаётся прочитать картинку'));
		if (size > WALLPAPER.maxBytes) {
			throw pictureError('too-large', fmt(tr('The picture is {0} MB; the limit is {1} MB', 'Картинка весит {0} МБ, предел — {1} МБ'), (size / 1048576).toFixed(1), Math.round(WALLPAPER.maxBytes / 1048576)));
		}
		const count = Math.ceil(size / chunk);
		const parts: Uint8Array[] = new Array(count);

		const readBatch = (first: number): Promise<Uint8Array[]> => {
			if (first >= count) return Promise.resolve(parts);
			const indexes: number[] = [];
			for (let i = first; i < Math.min(count, first + WALLPAPER.parallelChunks); i++) indexes.push(i);
			return Promise.all(indexes.map((i) =>
				run(`dd if=${quoted} bs=${chunk} skip=${i} count=1 2>/dev/null | base64 | tr -d '\\n'`).then((text) => {
					const b64 = text.trim();
					if (!b64 || !/^[A-Za-z0-9+\/=]+$/.test(b64)) throw pictureError('unreadable', tr('The picture cannot be read', 'Не удаётся прочитать картинку'));
					return decodeBase64(b64);
				})
			)).then((got) => {
				got.forEach((bytes, k) => { parts[indexes[k]] = bytes; });
				return readBatch(first + WALLPAPER.parallelChunks);
			});
		};
		return readBatch(0);
	});
}

export interface LoadedPicture {
	/** what to show: a blob: URL (give it back with releasePicture) */
	url: string;
	/** makes a screen-sized JPEG (data URL) for the startup cache when called (encoding it takes a good half second on the TV, so it is
	 *  only done when the launcher goes to the background); null when it couldn't be made */
	snapshot: (() => string | null) | null;
	/** how bright the picture is, in a 16 x 9 grid (for the plates behind the clock and the cards); null when it couldn't be measured */
	map: number[] | null;
	/** the picture blurred, for the frosted glass of the plates (a small 16:9 data URL); null when it couldn't be made */
	frost: string | null;
}

const SCREEN = {width: 1920, height: 1080};

/** The size to draw a picture at so it just covers the screen (never enlarged). */
export function coverSize (width: number, height: number): {width: number; height: number} {
	const scale = Math.min(1, Math.max(SCREEN.width / width, SCREEN.height / height));
	return {width: Math.max(1, Math.round(width * scale)), height: Math.max(1, Math.round(height * scale))};
}

/** Pictures much bigger than the screen are shrunk before showing: the TV would otherwise hold
 *  the whole full-size bitmap in memory for a 1080p screen. */
export function isOversized (width: number, height: number): boolean {
	return width > SCREEN.width * 1.5 || height > SCREEN.height * 1.5;
}

/** Resolves with the picture to show, or rejects with a PictureError whose message says what
 *  went wrong. */
export function loadPicture (path: string): Promise<LoadedPicture> {
	if (!isSafePath(path)) return Promise.reject(pictureError('bad-path', tr('This file name cannot be used', 'Это имя файла нельзя использовать')));
	const source: Promise<string> = isWebOS()
		? readFileChunks(path).then((parts) => URL.createObjectURL(new Blob(parts as unknown as BlobPart[], {type: mimeType(path)})))
		: Promise.resolve(fileUrl(path));        // desktop preview: file:// works there
	return source.then((url) => decode(url).then(
		(img) => prepare(url, img),
		(error: Error) => { releasePicture(url); throw error; }
	));
}

export function releasePicture (url: string): void {
	if (url.indexOf('blob:') === 0) URL.revokeObjectURL(url);
}

/** Draws the picture at screen size for the startup cache, and, when it is huge, shows that
 *  smaller copy instead of the original. */
function prepare (url: string, img: HTMLImageElement): Promise<LoadedPicture> {
	const size = coverSize(img.naturalWidth, img.naturalHeight);
	let canvas: HTMLCanvasElement | null = null;
	let snapshot: (() => string | null) | null = null;
	let map: number[] | null = null;
	try {
		canvas = document.createElement('canvas');
		canvas.width = size.width;
		canvas.height = size.height;
		const context = canvas.getContext('2d');
		if (!context) throw new Error('no canvas');
		context.drawImage(img, 0, 0, size.width, size.height);
		const drawn = canvas;
		snapshot = () => { try { return drawn.toDataURL('image/jpeg', 0.82); } catch { return null; } };
		map = measure(canvas);
	} catch {
		canvas = null;
		snapshot = null;
	}
	const frost = frostOf(canvas || img);
	if (!canvas || !isOversized(img.naturalWidth, img.naturalHeight)) return Promise.resolve({url, snapshot, map, frost});
	const shrunk = canvas;
	return new Promise<LoadedPicture>((resolve) => {
		shrunk.toBlob((blob) => {
			if (!blob) { resolve({url, snapshot, map, frost}); return; }
			releasePicture(url);
			resolve({url: URL.createObjectURL(blob), snapshot, map, frost});
		}, 'image/jpeg', 0.9);
	});
}

/** The picture's brightness as a grid: drawn at 64 x 36 and averaged in blocks (milliseconds, once per picture). */
function measure (source: HTMLCanvasElement | HTMLImageElement): number[] | null {
	try {
		const small = document.createElement('canvas');
		small.width = 64;
		small.height = 36;
		const context = small.getContext('2d');
		if (!context) return null;
		context.drawImage(source, 0, 0, 64, 36);
		return mapFromPixels(context.getImageData(0, 0, 64, 36).data);
	} catch { return null; }
}

/** The frosted glass: the picture cropped to 16:9 the way the wallpaper layer shows it (cover, centred), drawn at 480 x 270, blurred (and nothing else: the colours are the picture's own),
 *  and the shade of .wallpaper__scrim laid over it, so the glass matches what is behind the plate.
 *  Edges are padded first so the blur does not fade to nothing at the border of the screen. A 480 x 270 JPEG is a few KB, and the
 *  plates stretch it over the whole screen (blurred, so nobody sees the pixels). */
export function frostOf (source: HTMLCanvasElement | HTMLImageElement): string | null {
	try {
		const sw = source instanceof HTMLImageElement ? source.naturalWidth : source.width;
		const sh = source instanceof HTMLImageElement ? source.naturalHeight : source.height;
		if (!sw || !sh) return null;
		const cropW = Math.min(sw, sh * 16 / 9), cropH = cropW * 9 / 16;
		const sx = (sw - cropW) / 2, sy = (sh - cropH) / 2;
		const W = 480, H = 270, pad = 40;
		const padded = document.createElement('canvas');
		padded.width = W + 2 * pad;
		padded.height = H + 2 * pad;
		const pc = padded.getContext('2d');
		const out = document.createElement('canvas');
		out.width = W;
		out.height = H;
		const oc = out.getContext('2d');
		if (!pc || !oc) return null;
		pc.imageSmoothingQuality = 'high';
		pc.drawImage(source, sx, sy, cropW, cropH, 0, 0, W + 2 * pad, H + 2 * pad);     // rough filling for the margins
		pc.drawImage(source, sx, sy, cropW, cropH, pad, pad, W, H);                      // the picture itself
		oc.imageSmoothingQuality = 'high';
		if (typeof oc.filter === 'string') {
			oc.filter = 'blur(7px)';
			oc.drawImage(padded, -pad, -pad);
			oc.filter = 'none';
		} else {                                                                           // no canvas filters: shrink and enlarge, which blurs as well
			const tiny = document.createElement('canvas');
			tiny.width = 60;
			tiny.height = 34;
			const tc = tiny.getContext('2d');
			if (!tc) return null;
			tc.drawImage(padded, 0, 0, 60, 34);
			oc.drawImage(tiny, 0, 0, W, H);
		}
		const shade = oc.createLinearGradient(0, 0, 0, H);
		shade.addColorStop(0, 'rgba(0, 0, 0, 0.22)');
		shade.addColorStop(0.35, 'rgba(0, 0, 0, 0.08)');
		shade.addColorStop(0.55, 'rgba(0, 0, 0, 0)');
		shade.addColorStop(1, 'rgba(0, 0, 0, 0.5)');
		oc.fillStyle = shade;
		oc.fillRect(0, 0, W, H);
		return out.toDataURL('image/jpeg', 0.8);
	} catch { return null; }
}

/** The brightness grid and the frosted glass of a saved copy of a picture (a data URL), for when the launcher starts from the cache. */
export function measureDataUrl (url: string): Promise<{map: number[] | null; frost: string | null}> {
	return new Promise((resolve) => {
		const img = new Image();
		img.onload = () => resolve({map: measure(img), frost: frostOf(img)});
		img.onerror = () => resolve({map: null, frost: null});
		img.src = url;
	});
}

function decode (url: string): Promise<HTMLImageElement> {
	return new Promise<HTMLImageElement>((resolve, reject) => {
		const img = new Image();
		const timer = window.setTimeout(() => done(() => reject(pictureError('not-a-picture', tr('The picture took too long to open', 'Картинка слишком долго открывалась')))), WALLPAPER.probeTimeoutMs);
		const done = (fn: () => void) => {
			window.clearTimeout(timer);
			img.onload = null;
			img.onerror = null;
			fn();
		};
		img.onload = () => done(() => {
			const megapixels = Math.round(img.naturalWidth * img.naturalHeight / 1000000);
			if (img.naturalWidth * img.naturalHeight > WALLPAPER.maxPixels) {
				reject(pictureError('too-many-pixels', fmt(tr('The picture has {0} MP; the limit is {1} MP', 'В картинке {0} Мп, предел — {1} Мп'), megapixels, WALLPAPER.maxPixels / 1000000)));
			} else resolve(img);
		});
		img.onerror = () => done(() => reject(pictureError('not-a-picture', tr('The TV could not open the picture. Use JPG or PNG', 'ТВ не смог открыть картинку. Используйте JPG или PNG'))));
		img.src = url;
	});
}

// ---------- the startup cache: the picture last on screen, at screen size ----------

const CACHE_ID_KEY = 'customlauncher-wallpaper-id-v1';
const CACHE_DATA_KEY = 'customlauncher-wallpaper-data-v1';

export function wallpaperCacheId (mode: string, path: string): string {
	return `${mode}|${path}`;
}

/** The cached picture for this wallpaper choice (a data URL), or null. */
export function readWallpaperCache (id: string): string | null {
	try {
		if (localStorage.getItem(CACHE_ID_KEY) !== id) return null;
		const data = localStorage.getItem(CACHE_DATA_KEY);
		return data && data.indexOf('data:image/') === 0 ? data : null;
	} catch { return null; }
}

export function writeWallpaperCache (id: string, data: string): boolean {
	try {
		localStorage.setItem(CACHE_DATA_KEY, data);
		localStorage.setItem(CACHE_ID_KEY, id);
		return true;
	} catch { return false; }      // quota exceeded: the next start just loads the picture normally
}

export function clearWallpaperCache (): void {
	try { localStorage.removeItem(CACHE_ID_KEY); localStorage.removeItem(CACHE_DATA_KEY); } catch { /* ignore */ }
}

// ---------- sample data, so the layout can be developed in a desktop browser ----------

function mockListing (path: string): DirEntry[] {
	if (path === '/tmp/usb/sda/sda1') return [
		{name: 'Wallpapers', isDir: true}, {name: 'Holiday 2025', isDir: true}, {name: 'cover.jpg', isDir: false}
	];
	return [
		{name: 'mountains.jpg', isDir: false}, {name: 'night-sky.png', isDir: false}, {name: 'sea.jpg', isDir: false}
	];
}
