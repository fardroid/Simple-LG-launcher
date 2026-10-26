import {useEffect, useRef} from 'preact/hooks';
import {WALLPAPER} from '../config/constants';
import type {WallpaperConfig} from '../lib/storage';
import {
	clearWallpaperCache, listImages, loadPicture, measureDataUrl, readWallpaperCache, releasePicture, createShuffleOrder,
	wallpaperCacheId, writeWallpaperCache
} from '../lib/wallpaper';
import {tr} from '../lib/i18n';

interface Props {
	config: WallpaperConfig;
	/** called (at most once per wallpaper choice) when pictures can't be shown; says why */
	onProblem: (text: string) => void;
	/** the brightness grid of the picture now on screen, and its blurred copy for the glass (null: no picture, the theme's own background shows) */
	onTone?: (map: number[] | null, frost: string | null) => void;
}

/** Full-screen background: the default (black, or the calm gradient) with a picture or
 *  slideshow on top. Two layers cross-fade; the layer that faded out is emptied and its
 *  picture released, so only the picture on screen stays in memory.
 *
 *  Fast start: the picture last on screen is kept as a screen-sized copy in localStorage and
 *  shown at once when the launcher opens. A single picture then needs no further work; a
 *  slideshow lists its folder only when it is time for the next picture. The copy is brought
 *  up to date when the launcher goes to the background or this component is removed.
 *
 *  The slideshow pauses while the launcher is in the background. The picture from the copy appears at once
 *  (no fade); only a change from one slideshow picture to the next cross-fades. */
export function Wallpaper ({config, onProblem, onTone}: Props) {
	const layerA = useRef<HTMLDivElement>(null);
	const layerB = useRef<HTMLDivElement>(null);
	const report = useRef(onProblem);
	report.current = onProblem;
	const tone = useRef(onTone);
	tone.current = onTone;

	useEffect(() => {
		const layers = [layerA.current as HTMLDivElement, layerB.current as HTMLDivElement];
		const held: (string | null)[] = [null, null];     // the picture URL each layer holds
		const cacheId = wallpaperCacheId(config.mode, config.path);
		let front = -1;              // layer currently showing, -1 when none
		let disposed = false;
		let failed = false;
		let told = false;
		let busy = false;
		let timer = 0;
		let files: string[] = [];
		let position = -1;
		let order: ReturnType<typeof createShuffleOrder> | null = null;
		let lastError = '';
		let snapshot: (() => string | null) | null = null;    // makes the screen-sized copy of the picture on screen
		let unsaved = false;                   // the copy isn't in the cache yet
		let cacheKnown = false;                // the cache already holds a picture for this choice
		let shownAt = Date.now();              // when the picture on screen appeared

		const stopTimer = () => { window.clearTimeout(timer); timer = 0; };

		const saveCache = () => {
			if (!unsaved || !snapshot) return;
			const data = snapshot();
			if (data && writeWallpaperCache(cacheId, data)) { unsaved = false; snapshot = null; }     // saved: the picture's canvas can go
		};

		const release = (i: number) => {
			const url = held[i];
			held[i] = null;
			layers[i].style.backgroundImage = '';
			if (url) releasePicture(url);
		};

		const hide = (i: number) => {
			layers[i].classList.remove('wallpaper__layer--on');
			window.setTimeout(() => {
				if (!layers[i].classList.contains('wallpaper__layer--on')) release(i);
			}, WALLPAPER.fadeMs + 250);
		};

		const hideAll = () => {
			hide(0);
			hide(1);
			front = -1;
		};

		/** `instant`: no fade (the picture is already known, e.g. when the launcher comes back to the front) */
		const show = (url: string, instant?: boolean) => {
			const back = front === 0 ? 1 : 0;
			if (held[back]) release(back);
			held[back] = url;
			const layer = layers[back];
			layer.style.backgroundImage = `url("${url}")`;
			if (instant) {
				layer.style.transition = 'none';
				layer.classList.add('wallpaper__layer--on');
				void layer.offsetWidth;                // apply it now, before the transition is switched back on
				layer.style.transition = '';
			} else {
				window.requestAnimationFrame(() => layer.classList.add('wallpaper__layer--on'));
			}
			if (front >= 0) hide(front);
			front = back;
			shownAt = Date.now();
		};

		// A failure keeps whatever picture is already showing (say, the USB drive was unplugged).
		const fail = (text: string) => {
			failed = true;
			busy = false;
			if (!told) { told = true; report.current(text); }
		};

		const advance = () => {
			if (disposed) return;
			if (files.length) { next(0); return; }
			listImages(config.path).then(
				(list) => {
					if (disposed) return;
					if (!list.length) { fail(tr('There are no pictures in the folder', 'В папке нет картинок')); return; }
					files = list;
					order = config.shuffle ? createShuffleOrder(files, cacheId) : null;
					if (files.length === 1) return;          // a one-picture folder: nothing to rotate
					next(0);
				},
				() => { if (!disposed) fail(tr('Could not read the wallpaper folder. Is the USB drive still plugged in?', 'Не удалось прочитать папку с обоями. USB-накопитель всё ещё подключён?')); }
			);
		};

		const schedule = () => {
			stopTimer();
			const rotates = config.mode === 'folder' && (files.length === 0 || files.length > 1);
			if (rotates && !document.hidden) timer = window.setTimeout(advance, config.intervalSec * 1000);
		};

		const next = (attempt: number) => {
			if (disposed || busy) return;
			if (attempt >= files.length) { fail(lastError || tr('None of the pictures here could be shown', 'Не удалось показать ни одной картинки из этого места')); return; }
			busy = true;
			position = (position + 1) % files.length;
			const path = order ? order.take() : files[position];
			if (!path) { fail(tr('There are no pictures in the folder', 'В папке нет картинок')); return; }
			loadPicture(path).then(
				(loaded) => {
					busy = false;
					if (disposed) { releasePicture(loaded.url); return; }
					failed = false;
					if (order) order.shown(path);
					show(loaded.url);
					if (tone.current) tone.current(loaded.map, loaded.frost);
					snapshot = loaded.snapshot;
					unsaved = !!loaded.snapshot;
					if (!cacheKnown) { saveCache(); cacheKnown = !unsaved; }    // the first picture is cached at once
					schedule();
				},
				(error: Error) => {
					busy = false;
					if (disposed) return;
					lastError = error && error.message ? error.message : tr('Could not show the picture', 'Не удалось показать картинку');
					if (files.length === 1) fail(lastError);
					else next(attempt + 1);
				}
			);
		};

		const start = () => {
			stopTimer();
			failed = false;
			position = -1;
			lastError = '';
			files = [];
			order = null;
			if (config.mode === 'gradient' || !config.path) { clearWallpaperCache(); hideAll(); if (tone.current) tone.current(null, null); return; }

			const cached = readWallpaperCache(cacheId);
			if (cached) {                                          // instant: no shell work at all
				show(cached, true);                                // already known: no fade-in
				measureDataUrl(cached).then((look) => { if (!disposed && tone.current) tone.current(look.map, look.frost); });
				snapshot = () => cached;
				unsaved = false;
				cacheKnown = true;
				if (config.mode === 'folder') {
					if (config.shuffle) advance();
					else schedule();
				}
				return;
			}
			if (config.mode === 'image') { files = [config.path]; next(0); return; }
			listImages(config.path).then(
				(list) => {
					if (disposed) return;
					if (!list.length) { fail(tr('There are no pictures in the folder', 'В папке нет картинок')); return; }
					files = list;
					order = config.shuffle ? createShuffleOrder(files, cacheId) : null;
					next(0);
				},
				() => { if (!disposed) fail(tr('Could not read the wallpaper folder. Is the USB drive still plugged in?', 'Не удалось прочитать папку с обоями. USB-накопитель всё ещё подключён?')); }
			);
		};

		const onVisibility = () => {
			if (config.mode === 'gradient') return;
			if (document.hidden) {
				stopTimer();
				// encoding the copy takes a good half second: not while the TV is busy starting the other app
				window.setTimeout(() => { if (document.hidden) saveCache(); }, 2500);
				return;
			}
			if (failed) start();                                   // the USB drive may be back
			else if (config.mode === 'folder') {
				// carry on where the interval left off: coming back to the front must not start a fade of its own (and the
				// work of loading the next picture would make the remote react late, so it waits a while)
				stopTimer();
				timer = window.setTimeout(advance, Math.max(WALLPAPER.returnGraceMs, config.intervalSec * 1000 - (Date.now() - shownAt)));
			}
		};

		start();
		document.addEventListener('visibilitychange', onVisibility);
		return () => {
			disposed = true;
			stopTimer();
			saveCache();
			document.removeEventListener('visibilitychange', onVisibility);
			layers.forEach((layer) => layer.classList.remove('wallpaper__layer--on'));
			release(0);
			release(1);
		};
	}, [config.mode, config.path, config.intervalSec, config.shuffle]);

	return (
		<div class="wallpaper" aria-hidden="true">
			<div class="wallpaper__layer" ref={layerA} />
			<div class="wallpaper__layer" ref={layerB} />
			<div class="wallpaper__scrim" />
		</div>
	);
}
