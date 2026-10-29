// Reading a web address as bytes, for the news feeds and the quote of the day. The web view asks first. When that fails (most
// news sites send no CORS headers, and some older TVs cannot verify modern certificates) and the Homebrew Channel is available,
// the TV's own curl or wget saves the reply to a file, which is read back in pieces like a calendar file. Bytes, not text, so a
// feed in another encoding (windows-1251, say) can be decoded by what it says it is.

import {exec, shellQuote} from './apps';
import {readFileBytes} from './calendarload';

export interface Fetched {
	bytes: Uint8Array;
	/** the Content-Type header ('' when the shell fetched it) */
	type: string;
}

export interface FetchOptions {
	timeoutMs?: number;
	maxBytes?: number;
	/** extra request headers (the web view only) */
	headers?: Record<string, string>;
}

const DEFAULT_MAX = 2 * 1048576;
let fileSeq = 0;

/** An address that may go to the shell (quoted there): http or https, no spaces or control characters. */
export function fetchableUrl (url: string): boolean {
	return /^https?:\/\/[^\s\u0000-\u001f'"`\\<>]+$/i.test(url) && url.length <= 1000;
}

function viaWebView (url: string, options: FetchOptions): Promise<Fetched> {
	return new Promise<Fetched>((resolve, reject) => {
		const request = new XMLHttpRequest();
		request.open('GET', url, true);
		request.responseType = 'arraybuffer';
		request.timeout = options.timeoutMs || 15000;
		const headers = options.headers || {};
		Object.keys(headers).forEach((name) => request.setRequestHeader(name, headers[name]));
		request.onload = () => {
			if (request.status < 200 || request.status >= 300) { reject(new Error('HTTP ' + request.status)); return; }
			const bytes = new Uint8Array(request.response as ArrayBuffer);
			if (bytes.length > (options.maxBytes || DEFAULT_MAX)) { reject(new Error('too large')); return; }
			resolve({bytes, type: request.getResponseHeader('Content-Type') || ''});
		};
		request.onerror = () => reject(new Error('network error'));
		request.ontimeout = () => reject(new Error('timed out'));
		request.send();
	});
}

/** The shell command that saves `url` to `file`: curl, then curl without the certificate check, then wget the same two ways. */
export function shellSaveCommand (url: string, file: string, maxBytes: number, timeoutSec: number): string {
	const u = shellQuote(url), f = shellQuote(file);
	return `rm -f ${f}; curl -sfL -m ${timeoutSec} --max-filesize ${maxBytes} -A 'Mozilla/5.0 SimpleLGLauncher' -o ${f} ${u} || curl -sfkL -m ${timeoutSec} --max-filesize ${maxBytes} -A 'Mozilla/5.0 SimpleLGLauncher' -o ${f} ${u} || wget -q -T ${timeoutSec} -O ${f} ${u} || wget -q -T ${timeoutSec} --no-check-certificate -O ${f} ${u}; echo done`;
}

function viaShell (url: string, options: FetchOptions): Promise<Fetched> {
	if (!fetchableUrl(url)) return Promise.reject(new Error('unsafe address'));
	const file = `/tmp/simplelauncher-fetch-${Date.now() % 100000}-${fileSeq++}.bin`;
	const max = options.maxBytes || DEFAULT_MAX;
	const timeoutSec = Math.ceil((options.timeoutMs || 15000) / 1000);
	const cleanup = () => exec(`rm -f ${shellQuote(file)}`).catch(() => '');
	return exec(shellSaveCommand(url, file, max, timeoutSec), (timeoutSec * 4 + 10) * 1000)
		.then(() => readFileBytes(file, max))
		.then((bytes) => { cleanup(); return {bytes, type: ''}; }, (error) => { cleanup(); throw error; });
}

/** The reply at `url`: asked directly first, then (if allowed) through the TV's shell. */
export function fetchBytes (url: string, allowShell: boolean, options: FetchOptions = {}): Promise<Fetched> {
	return viaWebView(url, options).catch((error: Error) => (allowShell ? viaShell(url, options) : Promise.reject(error)));
}

/** The encoding of a reply: from its Content-Type, else from an XML declaration at its start, else UTF-8. */
export function charsetOf (bytes: Uint8Array, type: string): string {
	const fromType = /charset=["']?([A-Za-z0-9._-]+)/i.exec(type || '');
	if (fromType) return fromType[1].toLowerCase();
	let head = '';
	for (let i = 0; i < Math.min(bytes.length, 200); i++) head += String.fromCharCode(bytes[i]);
	const fromXml = /^\s*<\?xml[^>]*encoding=["']([A-Za-z0-9._-]+)["']/i.exec(head.replace(/^﻿|^ï»¿/, ''));
	return fromXml ? fromXml[1].toLowerCase() : 'utf-8';
}

/** The text of a reply, decoded by its own encoding (UTF-8 when the TV does not know that one). */
export function decodeText (fetched: Fetched): string {
	const charset = charsetOf(fetched.bytes, fetched.type);
	try { return new TextDecoder(charset).decode(fetched.bytes); }
	catch { return new TextDecoder('utf-8').decode(fetched.bytes); }
}

/** The text at `url`. */
export function fetchTextOf (url: string, allowShell: boolean, options: FetchOptions = {}): Promise<string> {
	return fetchBytes(url, allowShell, options).then(decodeText);
}
