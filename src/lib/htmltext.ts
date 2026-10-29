// Text out of a page's HTML, without a DOM (so it is the same in the tests): the page as lines of text, the items of its
// top-level lists, or the text of the elements with a given class. Built for Wikiquote's pages (see quote.ts); good enough for
// the bits of HTML in news feeds too.

/** Elements whose content is never text to read. */
const DROP = ['style', 'script', 'sup', 'table', 'figure', 'noscript', 'math', 'svg'];
/** Classes whose element is dropped with all it holds (edit links, navigation boxes, notes; in Wikiquote's quote boxes the
 *  original-language text and the editors' remarks). */
const DROP_CLASS = /\b(mw-editsection|navbox|metadata|noprint|reference|mw-empty-elt|toc|thumb|mw-references-wrap|hatnote|ambox|q-original|q-comment)\b/;
const BLOCK = ['p', 'div', 'td', 'th', 'tr', 'table', 'h1', 'h2', 'h3', 'h4', 'h5', 'h6', 'br', 'li', 'ul', 'ol', 'dl', 'dd', 'dt', 'center', 'blockquote', 'section', 'tbody'];
const VOID = ['br', 'img', 'hr', 'input', 'meta', 'link', 'wbr', 'source', 'area', 'col', 'embed'];

const ENTITIES: Record<string, string> = {amp: '&', lt: '<', gt: '>', quot: '"', apos: '\'', nbsp: ' ', ndash: '–', mdash: '—', laquo: '«', raquo: '»', hellip: '…', rsquo: '’', lsquo: '‘', rdquo: '”', ldquo: '“', bdquo: '„', middot: '·', shy: ''};

/** "&amp;" → "&", "&#8212;" → "—" and so on. */
export function decodeEntities (text: string): string {
	return text.replace(/&(#x[0-9a-f]+|#\d+|[a-z]+);/gi, (whole, name: string) => {
		if (name.charAt(0) === '#') {
			const code = name.charAt(1) === 'x' || name.charAt(1) === 'X' ? parseInt(name.slice(2), 16) : parseInt(name.slice(1), 10);
			return code > 0 && code < 0x110000 ? String.fromCodePoint(code) : whole;
		}
		const found = ENTITIES[name.toLowerCase()];
		return found === undefined ? whole : found;
	});
}

interface Tag {
	name: string;
	close: boolean;
	attrs: string;
	start: number;
	end: number;
}

function tags (html: string): Tag[] {
	const out: Tag[] = [];
	const re = /<(\/?)([a-zA-Z][a-zA-Z0-9]*)([^>]*)>|<!--[\s\S]*?-->/g;
	let m: RegExpExecArray | null;
	while ((m = re.exec(html))) {
		if (!m[2]) continue;
		out.push({name: m[2].toLowerCase(), close: m[1] === '/', attrs: m[3] || '', start: m.index, end: m.index + m[0].length});
	}
	return out;
}

function classOf (attrs: string): string {
	const m = /\bclass\s*=\s*("([^"]*)"|'([^']*)')/i.exec(attrs);
	return m ? (m[2] !== undefined ? m[2] : m[3]) : '';
}

/** One pass over the HTML: `onText` gets every piece of text outside dropped elements, `onTag` every tag (with the depth of
 *  open elements that are not dropped). Returns nothing; the callers keep their own state. */
function walk (html: string, onTag: (tag: Tag, cls: string) => void, onText: (text: string) => void, drop: readonly string[] = DROP): void {
	const list = tags(html);
	let at = 0;
	let dropping = 0;
	const dropStack: string[] = [];
	for (const tag of list) {
		if (!dropping && tag.start > at) onText(html.slice(at, tag.start));
		at = tag.end;
		const selfClosing = VOID.indexOf(tag.name) !== -1 || /\/\s*$/.test(tag.attrs);
		if (dropping) {
			if (selfClosing) continue;
			if (tag.close && tag.name === dropStack[dropStack.length - 1]) { dropStack.pop(); dropping--; }
			else if (!tag.close && tag.name === dropStack[dropStack.length - 1]) { dropStack.push(tag.name); dropping++; }
			continue;
		}
		const cls = tag.close ? '' : classOf(tag.attrs);
		if (!tag.close && !selfClosing && (drop.indexOf(tag.name) !== -1 || DROP_CLASS.test(cls))) {
			dropping = 1;
			dropStack.length = 0;
			dropStack.push(tag.name);
			continue;
		}
		onTag(tag, cls);
	}
	if (!dropping && at < html.length) onText(html.slice(at));
}

function tidy (text: string): string {
	return decodeEntities(text).replace(/[ \t\r\f\v ]+/g, ' ').replace(/ *\n */g, '\n').trim();
}

/** Elements dropped when tables are kept (a main page lays its boxes out in tables). */
const DROP_KEEP_TABLES = DROP.filter((n) => n !== 'table');

/** The page as lines of text (one per block element), empty lines left out. `tables`: keep what is in tables. */
export function htmlLines (html: string, tables: boolean = false): string[] {
	let out = '';
	walk(html, (tag) => { if (BLOCK.indexOf(tag.name) !== -1) out += '\n'; }, (text) => { out += text.replace(/\s+/g, ' '); }, tables ? DROP_KEEP_TABLES : DROP);
	return decodeEntities(out).split('\n').map((l) => l.replace(/[  ]+/g, ' ').trim()).filter((l) => l.length > 0);
}

/** Plain text of a bit of HTML (a feed's description, say): tags gone, entities decoded, blocks on lines of their own. */
export function htmlToText (html: string): string {
	return htmlLines(html).join('\n');
}

/** The headings an item is under, outermost first ("Quotes about him / Letters"): a heading replaces those at its level and below. */
function sectionTracker () {
	const levels: string[] = [];
	return {
		set (tag: string, text: string): string {
			const depth = Math.max(0, parseInt(tag.slice(1), 10) - 2);
			levels.length = depth;
			levels[depth] = text;
			return levels.filter((x) => !!x).join(' / ');
		}
	};
}

export interface ListItem {
	text: string;
	/** the headings of the section the item is in, outermost first, joined with " / " ('' before the first heading) */
	section: string;
}

/** The items of the page's top-level lists (lists nested in an item, which hold the sources or a translation, left out),
 *  each with the heading of its section. */
export function topLevelItems (html: string): ListItem[] {
	const items: ListItem[] = [];
	let depth = 0;           // open ul / ol / dl
	let current: string | null = null;
	let heading: string | null = null;
	let level = 'h2';
	const sections = sectionTracker();
	let section = '';
	walk(html, (tag) => {
		const n = tag.name;
		if (/^h[1-6]$/.test(n)) {
			if (!tag.close) { heading = ''; level = n; }
			else if (heading !== null) { section = sections.set(level, tidy(heading)); heading = null; }
			return;
		}
		if (n === 'ul' || n === 'ol' || n === 'dl') {
			depth += tag.close ? -1 : 1;
			if (depth < 0) depth = 0;
			return;
		}
		if (n === 'li' && depth === 1) {
			if (!tag.close) current = '';
			else if (current !== null) { items.push({text: tidy(current), section}); current = null; }
			return;
		}
		if (n === 'br' && current !== null && depth === 1) current += '\n';
	}, (text) => {
		if (heading !== null) heading += text;
		else if (current !== null && depth === 1) current += text.replace(/\s+/g, ' ');
	});
	return items.filter((i) => i.text.length > 0);
}

/** The text of every element whose class list has `name` (nested ones are part of their parent's text). `tables`: look inside
 *  tables too (the quote boxes of Russian and Ukrainian Wikiquote are table rows). */
export function textOfClass (html: string, name: string, tables: boolean = false): ListItem[] {
	const out: ListItem[] = [];
	const re = new RegExp('(^|\\s)' + name + '(\\s|$)');
	const stack: string[] = [];
	let current: string | null = null;
	let heading: string | null = null;
	let level = 'h2';
	const sections = sectionTracker();
	let section = '';
	walk(html, (tag, cls) => {
		if (/^h[1-6]$/.test(tag.name) && current === null) {
			if (!tag.close) { heading = ''; level = tag.name; }
			else if (heading !== null) { section = sections.set(level, tidy(heading)); heading = null; }
			return;
		}
		const selfClosing = VOID.indexOf(tag.name) !== -1;
		if (selfClosing) { if (tag.name === 'br' && current !== null) current += '\n'; return; }
		if (current === null) {
			if (!tag.close && re.test(cls)) { current = ''; stack.length = 0; stack.push(tag.name); }
			return;
		}
		if (!tag.close) { stack.push(tag.name); if (BLOCK.indexOf(tag.name) !== -1) current += '\n'; return; }
		if (tag.name === stack[stack.length - 1]) stack.pop();
		if (!stack.length) { out.push({text: tidy(current), section}); current = null; }
	}, (text) => {
		if (heading !== null) heading += text;
		else if (current !== null) current += text.replace(/\s+/g, ' ');
	}, tables ? DROP_KEEP_TABLES : DROP);
	return out.filter((i) => i.text.length > 0);
}
