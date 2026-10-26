import {Component, h, type ComponentType} from 'preact';
import {lang} from './i18n';

/** Two prop values count as the same when they are equal, or are dates of the same moment, or are objects (and arrays) made of the same
 *  things. Functions always count as the same: the handlers handed to the cards only call state setters and refs, so an older copy of one
 *  does exactly what a new one would. */
export function sameProps (a: unknown, b: unknown): boolean {
	if (a === b) return true;
	if (typeof a === 'function' && typeof b === 'function') return true;
	if (a instanceof Date && b instanceof Date) return a.getTime() === b.getTime();
	if (a && b && typeof a === 'object' && typeof b === 'object') {
		if (Array.isArray(a) !== Array.isArray(b)) return false;
		const ka = Object.keys(a), kb = Object.keys(b);
		if (ka.length !== kb.length) return false;
		const x = a as Record<string, unknown>, y = b as Record<string, unknown>;
		for (let i = 0; i < ka.length; i++) if (!sameProps(x[ka[i]], y[ka[i]])) return false;
		return true;
	}
	return false;
}

/** The component, drawn again only when its props really changed. Without this every key press and every move of the pointer
 *  rebuilt the clock, the weather and the calendar along with the row of apps, which the TV's processor felt as lag. A change of
 *  language draws it again too: its words are not in its props. */
export function memoProps<P extends object> (Inner: ComponentType<P>): ComponentType<P> {
	class Memo extends Component<P> {
		drawnIn = lang();
		shouldComponentUpdate (next: P) { return this.drawnIn !== lang() || !sameProps(this.props, next); }
		render () { this.drawnIn = lang(); return h(Inner as ComponentType<object>, this.props as object); }
	}
	return Memo as unknown as ComponentType<P>;
}
