import {useEffect, useRef} from 'preact/hooks';
import type {PlateLook} from '../lib/plates';

/** The bottom of a plate: the frosted glass (the blurred wallpaper, see frostOf in lib/wallpaper.ts), the plate's colour and its rim.
 *  The picture changes with a cross-fade of two layers, with the same length and curve as the wallpaper's own fade (.wallpaper__layer),
 *  so the glass and the picture behind it change together. The layers are only moved by their classes, as in Wallpaper. */
export function Frost ({plate}: {plate?: PlateLook}) {
	const url = (plate && plate.frost) || '';
	const layers = [useRef<HTMLDivElement>(null), useRef<HTMLDivElement>(null)];
	const state = useRef({front: -1, url: ''});

	useEffect(() => {
		const s = state.current;
		if (url === s.url) return;
		const els = [layers[0].current as HTMLDivElement, layers[1].current as HTMLDivElement];
		const back = s.front === 0 ? 1 : 0;
		if (url) {
			els[back].style.backgroundImage = `url("${url}")`;
			window.requestAnimationFrame(() => els[back].classList.add('frost__layer--on'));
		}
		if (s.front >= 0) els[s.front].classList.remove('frost__layer--on');
		s.front = url ? back : -1;
		s.url = url;
	}, [url]);

	return (
		<div class="frost" aria-hidden="true">
			<div class="frost__layer" ref={layers[0]} />
			<div class="frost__layer" ref={layers[1]} />
		</div>
	);
}
