// Small previews of the wallpaper pictures, for the tiles of the wallpaper gallery.
//
// The TV has no tool that shrinks a JPEG, so the launcher makes each preview itself: it reads the picture through the shell (as the
// wallpaper does, lib/wallpaper.ts), draws it small on a canvas and keeps that small JPEG in memory for as long as the launcher is
// loaded, so the gallery opens at once the next time. Photos from a camera or phone carry a small preview of their own near the start
// of the file (in their EXIF block): when there is one, only the first chunk of the file is read.

import {WALLPAPER} from '../config/constants';
import {exec, shellQuote} from './apps';
import {isWebOS} from './luna';
import {decode, decodeBase64, fileUrl, isSafePath, mimeType, readFileChunks} from './wallpaper';

/** the preview's size: 16:9, a little over the size of a tile of the gallery */
export const THUMB = {width: 320, height: 180};

const made = new Map<string, string | null>();
const making = new Map<string, Promise<string | null>>();

/** The preview already made for this picture: a data URL, null when it could not be made, undefined when it was not tried yet. */
export function cachedThumb (path: string): string | null | undefined {
	return made.get(path);
}

/** The JPEG preview inside a JPEG's EXIF block (APP1), when the first bytes of the file hold one. Pure. */
export function exifThumbnail (bytes: Uint8Array): Uint8Array | null {
	if (bytes.length < 4 || bytes[0] !== 0xFF || bytes[1] !== 0xD8) return null;
	let i = 2;
	while (i + 4 <= bytes.length && bytes[i] === 0xFF) {
		const marker = bytes[i + 1];
		const length = (bytes[i + 2] << 8) | bytes[i + 3];
		if (marker === 0xDA || length < 2) return null;          // the picture itself starts: no EXIF block before it
		const end = Math.min(bytes.length, i + 2 + length);
		const exif = marker === 0xE1 && bytes[i + 4] === 0x45 && bytes[i + 5] === 0x78 && bytes[i + 6] === 0x69 && bytes[i + 7] === 0x66;
		if (exif) {
			let start = -1;
			for (let k = i + 10; k + 2 < end; k++) if (bytes[k] === 0xFF && bytes[k + 1] === 0xD8 && bytes[k + 2] === 0xFF) { start = k; break; }
			if (start === -1) return null;
			for (let k = end - 2; k > start; k--) if (bytes[k] === 0xFF && bytes[k + 1] === 0xD9) return bytes.subarray(start, k + 2);
			return null;
		}
		i = end;
	}
	return null;
}

/** The picture drawn small (cropped to 16:9, like the wallpaper covering the screen), as a JPEG data URL. */
function shrink (img: HTMLImageElement): string {
	const canvas = document.createElement('canvas');
	canvas.width = THUMB.width;
	canvas.height = THUMB.height;
	const context = canvas.getContext('2d');
	if (!context) throw new Error('no canvas');
	const scale = Math.max(THUMB.width / img.naturalWidth, THUMB.height / img.naturalHeight);
	const w = img.naturalWidth * scale, h = img.naturalHeight * scale;
	context.drawImage(img, (THUMB.width - w) / 2, (THUMB.height - h) / 2, w, h);
	return canvas.toDataURL('image/jpeg', 0.8);
}

function fromBlob (parts: Uint8Array[], type: string): Promise<string> {
	const url = URL.createObjectURL(new Blob(parts as unknown as BlobPart[], {type}));
	return decode(url).then(
		(img) => { const out = shrink(img); URL.revokeObjectURL(url); return out; },
		(error: Error) => { URL.revokeObjectURL(url); throw error; }
	);
}

/** Makes the picture's preview (or gives the one made before). Resolves with null when it cannot be made. */
export function loadThumb (path: string): Promise<string | null> {
	const known = made.get(path);
	if (known !== undefined) return Promise.resolve(known);
	const running = making.get(path);
	if (running) return running;
	let work: Promise<string>;
	if (!isWebOS()) work = decode(fileUrl(path)).then(shrink);
	else if (!isSafePath(path)) work = Promise.reject(new Error('Unsafe path'));
	else {
		const full = () => readFileChunks(path).then((parts) => fromBlob(parts, mimeType(path)));
		work = mimeType(path) !== 'image/jpeg' ? full()
			: exec(`dd if=${shellQuote(path)} bs=${WALLPAPER.chunkBytes} count=1 2>/dev/null | base64 | tr -d '\\n'`).then((text) => {
				const head = decodeBase64(text.trim());
				if (head.length < WALLPAPER.chunkBytes) return fromBlob([head], 'image/jpeg');     // a small file: that was all of it
				const own = exifThumbnail(head);
				return own ? fromBlob([own], 'image/jpeg').catch(full) : full();
			});
	}
	const done = work.then(
		(url) => { made.set(path, url); making.delete(path); return url; },
		() => { made.set(path, null); making.delete(path); return null; }
	);
	making.set(path, done);
	return done;
}
