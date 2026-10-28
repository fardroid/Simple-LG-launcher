import {useLayoutEffect, useRef} from 'preact/hooks';
import {WALLPAPER} from '../config/constants';
import type {PlateLook} from '../lib/plates';

const DEFAULT_BG = 'rgba(255, 255, 255, 0.11)';

/** The bottom of a plate: the frosted glass (the blurred wallpaper, see frostOf in lib/wallpaper.ts) under the plate's colour, then its rim.
 *  The glass and the colour change together, as one picture, with a cross-fade of two layers that has the same length and curve as the
 *  wallpaper's own fade (.wallpaper__layer): a light plate turns dark as the picture behind it does, not at once. Only opacity is
 *  animated (the TV animates nothing else smoothly). The new layer fades in on top of the old one, which stays until it is covered,
 *  so the sharp wallpaper never shows through half-way; without glass the colour alone is cross-fading, and both layers fade.
 *  The layer is set up while the plate renders (a layout effect, not one after the next paint), so its fade is asked for in the same
 *  frame as the picture's. A plate that appears with its look already known shows it at once, as the picture from the cache is. */
export function Frost ({plate}: {plate?: PlateLook}) {
	const url = (plate && plate.frost) || '';
	const bg = (plate && plate.bg) || DEFAULT_BG;
	const layers = [useRef<HTMLDivElement>(null), useRef<HTMLDivElement>(null)];
	const state = useRef({front: -1, key: '', mounted: false, timer: 0});

	useLayoutEffect(() => {
		const s = state.current;
		const first = !s.mounted;
		s.mounted = true;
		const key = `${bg}|${url}`;
		if (key === s.key) return;
		const els = [layers[0].current as HTMLDivElement, layers[1].current as HTMLDivElement];
		const back = s.front === 0 ? 1 : 0;
		const old = s.front;
		const layer = els[back];
		window.clearTimeout(s.timer);
		layer.style.backgroundImage = `linear-gradient(${bg}, ${bg})${url ? `, url("${url}")` : ''}`;
		layer.style.zIndex = '1';
		if (old >= 0) els[old].style.zIndex = '0';
		if (first) {
			layer.style.transition = 'none';
			layer.classList.add('frost__layer--on');
			void layer.offsetWidth;                // apply it now, before the transition is switched back on
			layer.style.transition = '';
		} else window.requestAnimationFrame(() => layer.classList.add('frost__layer--on'));
		if (old >= 0) {
			// with glass the old layer is opaque: it waits under the new one until that has covered it
			if (url) s.timer = window.setTimeout(() => els[old].classList.remove('frost__layer--on'), WALLPAPER.fadeMs + 50);
			else els[old].classList.remove('frost__layer--on');
		}
		s.front = back;
		s.key = key;
	}, [url, bg]);

	useLayoutEffect(() => () => window.clearTimeout(state.current.timer), []);

	return (
		<div class="frost" aria-hidden="true">
			<div class="frost__layer" ref={layers[0]} />
			<div class="frost__layer" ref={layers[1]} />
		</div>
	);
}
