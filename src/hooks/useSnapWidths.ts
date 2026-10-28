import {useLayoutEffect} from 'preact/hooks';
import type {RefObject} from 'preact';

/** Rounds the width of each element matching `selector` under `root` up to a whole pixel (as a min-width), after every render.
 *  Text is rarely a whole number of pixels wide, so whatever is placed after it (the next button) or against the right edge (a
 *  row's value) would land on a fraction of a pixel, and its text would be soft through the TV's 2x scale-up of its 1080p picture.
 *  An element is measured again only when its text changes. `even`: to an even number, for something centred (half of the space
 *  left beside it is then whole too). `root` itself counts when it matches. */
export function useSnapWidths (root: RefObject<HTMLElement>, selector: string, even?: boolean): void {
	useLayoutEffect(() => {
		const el = root.current;
		if (!el) return;
		const items = el.matches(selector) ? [el] : el.querySelectorAll(selector);
		for (let i = 0; i < items.length; i++) {
			const item = items[i] as HTMLElement & {snapText?: string};
			const text = item.textContent || '';
			if (item.snapText === text) continue;
			item.snapText = text;
			item.style.minWidth = '';
			const width = Math.ceil(item.getBoundingClientRect().width);
			item.style.minWidth = `${even && width % 2 ? width + 1 : width}px`;
		}
	});
}
