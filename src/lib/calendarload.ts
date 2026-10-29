// Reading the calendars: web addresses (through fetchText), files on the TV and every .ics file in a folder
// (through the Homebrew Channel's shell, in pieces like the wallpaper pictures). Several calendars are shown together.

import {exec, shellQuote} from './apps';
import {WALLPAPER} from '../config/constants';
import {joinCalendars, parseCalendar, normalizeUrl, validPath, type CalendarData, type CalendarSource} from './calendar';
import type {Occurrence} from './ics';
import {fetchText} from './weathernet';

type Part = {source: CalendarSource; events: Occurrence[]};

const ICS_NAME = /^[A-Za-z0-9._@-]+\.ics$/i;

/** A web address, a file, or a folder: the files in a folder become calendars of their own. */
export function expandSources (sources: string[]): Promise<string[]> {
	const lists = sources.map((s) => {
		if (s.charAt(s.length - 1) !== '/') return Promise.resolve([s]);
		if (!validPath(s)) return Promise.resolve([] as string[]);
		return exec(`ls -1 ${shellQuote(s)}`).then(
			(out) => out.split('\n').map((l) => l.trim()).filter((l) => ICS_NAME.test(l)).sort().map((l) => s + l),
			() => [] as string[]
		);
	});
	return Promise.all(lists).then((all) => all.reduce((a, b) => a.concat(b), [] as string[]));
}

function decode (b64: string): Uint8Array {
	const binary = atob(b64);
	const bytes = new Uint8Array(binary.length);
	for (let i = 0; i < binary.length; i++) bytes[i] = binary.charCodeAt(i);
	return bytes;
}

/** A file from the TV as bytes, read in pieces (a calendar can be a few megabytes). */
export function readFileBytes (path: string, maxBytes: number = 16 * 1048576): Promise<Uint8Array> {
	if (!validPath(path)) return Promise.reject(new Error('bad path'));
	const quoted = shellQuote(path);
	const chunk = WALLPAPER.chunkBytes;
	return exec(`wc -c < ${quoted}`).then((out) => {
		const size = parseInt(out.trim(), 10);
		if (!(size > 0)) throw new Error('empty file');
		if (size > maxBytes) throw new Error('file too large');
		const count = Math.ceil(size / chunk);
		const parts: Uint8Array[] = new Array(count);
		const batch = (first: number): Promise<void> => {
			if (first >= count) return Promise.resolve();
			const indexes: number[] = [];
			for (let i = first; i < Math.min(count, first + WALLPAPER.parallelChunks); i++) indexes.push(i);
			return Promise.all(indexes.map((i) => exec(`dd if=${quoted} bs=${chunk} skip=${i} count=1 2>/dev/null | base64 | tr -d '\\n'`, 20000).then((text) => {
				const b64 = text.trim();
				if (!b64 || !/^[A-Za-z0-9+\/=]+$/.test(b64)) throw new Error('unreadable');
				parts[i] = decode(b64);
			}))).then(() => batch(first + WALLPAPER.parallelChunks));
		};
		return batch(0).then(() => {
			let total = 0;
			parts.forEach((p) => { total += p.length; });
			const all = new Uint8Array(total);
			let at = 0;
			parts.forEach((p) => { all.set(p, at); at += p.length; });
			return all;
		});
	});
}

/** A text file from the TV (UTF-8), read in pieces. */
export function readTextFile (path: string): Promise<string> {
	return readFileBytes(path).then((bytes) => new TextDecoder('utf-8').decode(bytes));
}

function isFile (src: string): boolean {
	return src.charAt(0) === '/';
}

/** Size and change time: a file that has not changed is not read again. */
function signature (path: string): Promise<string> {
	return exec(`stat -c '%s-%Y' ${shellQuote(path)}`, 5000).then((out) => out.trim(), () => '');
}

/** Reads every calendar in `sources` (folders expanded) into one set of events. `known` is what was read before: files
 *  that have not changed since are taken from it. A calendar that cannot be read is left out; it is an error only when none can. */
export function fetchCalendars (sources: string[], key: string, allowShell: boolean, known: CalendarData | null): Promise<CalendarData> {
	const now = Date.now();
	return expandSources(sources.map((s) => (isFile(s) ? s : normalizeUrl(s)))).then((list) => {
		const parts: Part[] = new Array(list.length);
		let failed = 0;
		const one = (src: string, index: number): Promise<void> => {
			const done = (p: Part | null) => { if (p) parts[index] = p; else failed++; };
			if (!isFile(src)) return fetchText(src, allowShell).then((text) => done(parseCalendar(text, src, '', index, now)), () => done(null));
			return signature(src).then((sig) => {
				const old = known && sig ? known.sources.findIndex((s) => s.src === src && s.sig === sig) : -1;
				if (known && old !== -1) {
					const events = known.events.filter((e) => e.src === old).map((e) => ({...e, src: index}));
					parts[index] = {source: known.sources[old], events};
					return undefined;
				}
				return readTextFile(src).then((text) => done(parseCalendar(text, src, sig, index, now)), () => done(null));
			});
		};
		// one at a time: reading a big calendar already uses the shell's whole capacity
		return list.reduce((chain, src, i) => chain.then(() => one(src, i)), Promise.resolve()).then(() => {
			const ok = parts.filter((p) => !!p);
			if (!ok.length) throw new Error(failed ? 'nothing could be read' : 'no calendars');
			ok.forEach((p, i) => { p.events.forEach((e) => { e.src = i; }); });
			return joinCalendars(key, ok, now);
		});
	});
}
