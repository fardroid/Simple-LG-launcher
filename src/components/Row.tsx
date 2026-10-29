import {useEffect, useRef} from 'preact/hooks';
import {DOCK, LAYOUT} from '../theme/tokens';
import {dockScale, indexAmong} from '../lib/dock';
import {clamp} from '../hooks/useKeys';
import {Tile, type TileModel} from './Tile';
import {tr} from '../lib/i18n';

interface Props {
	label?: string;
	items: TileModel[];
	focused: boolean;
	selected: number;
	emptyText?: string;
	onSelect: (index: number) => void;
	onActivate: (index: number) => void;
	/** tile size and gap in px, and the width the row has to fit in; the CSS must use the same sizes */
	tileSize?: number;
	tileGap?: number;
	viewportWidth?: number;
	/** the selected tile and its neighbours grow, like a dock (the home row) */
	magnify?: boolean;
	/** the look of the dock row (no outlines on the tiles, soft shadows) even when nothing is magnified at the moment, e.g. while the remote is on a card; default: as `magnify` */
	dock?: boolean;
	/** the white ring on the selected tile (default: when the row has the focus) */
	ring?: boolean;
	/** the tile the carousel centres on (default: the selected one). With a pointer it stays where the keys left it, so
	 *  the row does not slide away under the pointer. */
	scrollIndex?: number;
	/** where the pointer is, in tiles from the first one's centre (fractional); the magnification follows it. null: no pointer */
	pointerPos?: number | null;
	/** the pointer moved over the row (or left it: null) */
	onPointer?: (position: number | null) => void;
	/** the wheel turned over the row: -1 or 1 */
	onWheelStep?: (direction: number) => void;
	/** the pointer rests near an end of the row and there is more beyond it: centre the carousel on this tile */
	onEdgeScroll?: (index: number) => void;
}

/** how far into the row, from either end of what is shown, the pointer makes it scroll (in tile steps, past the viewport's padding) */
const EDGE_STEPS = 0.75;
/** the scrolling starts after a moment (a pointer only passing by does not move the row) */
const EDGE_DELAY_MS = 200;
/** its speed, layout px a second: slow where the edge zone begins, fast right at the end */
const EDGE_SPEED_MIN = 450;
const EDGE_SPEED_MAX = 1500;

/** Horizontal strip of tiles. Centred when it fits; otherwise a carousel that keeps
 *  the selected tile in the middle (clamped at the ends). With `magnify` the tiles near the selected one (or near
 *  the pointer, continuously) grow, and the tiles move apart to make room (only transforms change, so it stays smooth). */
export function Row ({label, items, focused, selected, emptyText, onSelect, onActivate, tileSize = LAYOUT.tileSize, tileGap = LAYOUT.tileGap, viewportWidth = LAYOUT.screenWidth - 2 * LAYOUT.edgePadding, magnify, dock, ring, scrollIndex, pointerPos, onPointer, onWheelStep, onEdgeScroll}: Props) {
	const step = tileSize + tileGap;
	const total = items.length * step - tileGap;
	const track = useRef<HTMLDivElement>(null);
	const frame = useRef({pos: 0, queued: false});
	const mid = pointerPos !== null && pointerPos !== undefined ? pointerPos : selected;
	const anchor = scrollIndex === undefined ? selected : scrollIndex;
	// how much wider each tile gets when magnified, and how far each one has to move to make room
	const extra = items.map((_item, i) => (magnify && focused ? tileSize * (dockScale(Math.abs(i - mid)) - 1) : 0));
	const shift: number[] = [];
	let running = 0;
	extra.forEach((e, i) => { shift[i] = running + e / 2; running += e; });
	const widened = total + running;
	const fits = widened <= viewportWidth;
	const offset = fits ? running / 2 : clamp(anchor * step + tileSize / 2 + (shift[Math.round(anchor)] || 0) - viewportWidth / 2, 0, widened - viewportWidth);

	// Steps that follow each other quickly (a held arrow key) glide with a short linear move instead of
	// restarting the usual ease-out curve every time.
	const last = useRef({selected: anchor, at: 0, fast: false});
	if (last.current.selected !== anchor) {
		const now = Date.now();
		last.current = {selected: anchor, at: now, fast: now - last.current.at < 280};
	}

	// The pointer: its place among the tiles (measured on the untouched layout, so the magnification does not chase itself),
	// reported at most once per frame.
	const report = (x: number) => {
		if (!onPointer || !track.current) return;
		// where the tiles' centres really are on the screen, so the tile under the pointer is the one that grows
		const centres: number[] = [];
		for (let i = 0; i < track.current.children.length; i++) {
			const r = track.current.children[i].getBoundingClientRect();
			centres.push((r.left + r.right) / 2);
		}
		frame.current.pos = indexAmong(centres, x);
		if (frame.current.queued) return;
		frame.current.queued = true;
		window.requestAnimationFrame(() => { frame.current.queued = false; onPointer(frame.current.pos); });
	};

	// Near an end of what is shown, with more tiles beyond it, the row glides on by itself while the pointer stays there, faster the
	// closer the pointer is to the end. The glide is drawn frame by frame straight onto the track, with no CSS transition and no
	// render per frame (each step restarting an eased transition looked jerky); the tile under the resting pointer is worked out from
	// the distance glided. Where it stops, the row stays (`held`) until the keys or the wheel move it.
	const edge = useRef({x: 0, dir: 0, depth: 0, timer: 0, raf: 0, cur: 0, from: 0, last: 0, k: 1, centres: [] as number[]});
	const held = useRef<{anchor: number; offset: number} | null>(null);
	const max = Math.max(0, widened - viewportWidth);
	if (held.current && held.current.anchor !== anchor) held.current = null;
	const shown = fits ? offset : edge.current.raf ? edge.current.cur : held.current ? clamp(held.current.offset, 0, max) : offset;
	const view = useRef({shown, max, canLeft: false, canRight: false, onPointer, onEdgeScroll});
	view.current = {shown, max, canLeft: !fits && shown > 0.5, canRight: !fits && shown < max - 0.5, onPointer, onEdgeScroll};
	const stopEdge = () => {
		const e = edge.current, v = view.current;
		if (e.timer) window.clearTimeout(e.timer);
		e.timer = 0; e.dir = 0;
		if (!e.raf) return;
		window.cancelAnimationFrame(e.raf);
		e.raf = 0;
		if (track.current) track.current.classList.remove('row__track--free');
		const index = Math.round((e.cur + viewportWidth / 2 - tileSize / 2) / step);
		held.current = {anchor: index, offset: e.cur};
		if (v.onEdgeScroll) v.onEdgeScroll(index);
	};
	const glide = (now: number) => {
		const e = edge.current, v = view.current, t = track.current;
		if (!t || !e.dir) { stopEdge(); return; }
		const dt = Math.min(50, now - e.last);
		e.last = now;
		e.cur = clamp(e.cur + e.dir * (EDGE_SPEED_MIN + (EDGE_SPEED_MAX - EDGE_SPEED_MIN) * e.depth) * dt / 1000, 0, v.max);
		t.style.transform = `translate3d(${-Math.round(e.cur)}px, 0, 0)`;
		if (v.onPointer) v.onPointer(indexAmong(e.centres.map((c) => c - (e.cur - e.from) * e.k), e.x));
		if (e.cur <= 0 || e.cur >= v.max) { stopEdge(); return; }
		e.raf = window.requestAnimationFrame(glide);
	};
	const startEdge = () => {
		const e = edge.current, t = track.current;
		e.timer = 0;
		if (!t || !view.current.onEdgeScroll) return;
		e.cur = e.from = view.current.shown;
		t.classList.add('row__track--free');
		t.style.transform = `translate3d(${-Math.round(e.cur)}px, 0, 0)`;
		const port = t.parentElement as HTMLElement;
		e.k = port.offsetWidth ? port.getBoundingClientRect().width / port.offsetWidth : 1;
		e.centres = [];
		for (let i = 0; i < t.children.length; i++) {
			const r = t.children[i].getBoundingClientRect();
			e.centres.push((r.left + r.right) / 2);
		}
		e.last = performance.now();
		e.raf = window.requestAnimationFrame(glide);
	};
	const moved = (event: MouseEvent) => {
		const e = edge.current;
		e.x = event.clientX;
		if (!e.raf) report(event.clientX);     // while gliding, the glide reports the tile itself
		if (!onEdgeScroll || !track.current) return;
		const port = track.current.parentElement as HTMLElement;
		const box = port.getBoundingClientRect();
		// the viewport's padding (room for the glow) is clipped too; the sizes here are the layout's, the box the screen's
		const zone = ((port.offsetWidth - viewportWidth) / 2 + EDGE_STEPS * step) * (port.offsetWidth ? box.width / port.offsetWidth : 1);
		const v = view.current;
		const dir = event.clientX < box.left + zone && (v.canLeft || e.dir < 0) ? -1 : event.clientX > box.right - zone && (v.canRight || e.dir > 0) ? 1 : 0;
		e.depth = dir < 0 ? clamp((box.left + zone - event.clientX) / zone, 0, 1) : dir > 0 ? clamp((event.clientX - box.right + zone) / zone, 0, 1) : 0;
		if (dir === e.dir) return;
		stopEdge();
		e.dir = dir;
		if (dir) e.timer = window.setTimeout(startEdge, EDGE_DELAY_MS);
	};
	// the pointer was put away (a key pressed) or the row lost it: no more scrolling
	const pointing = pointerPos !== null && pointerPos !== undefined;
	useEffect(() => { if (!pointing) stopEdge(); }, [pointing]);
	useEffect(() => stopEdge, []);

	return (
		<section class={`row${focused ? ' row--focused' : ''}${(dock === undefined ? magnify : dock) ? ' row--dock' : ''}${pointerPos !== null && pointerPos !== undefined ? ' row--pointer' : ''}`}>
			{label && <h2 class="row__label">{label}</h2>}
			<div class="row__viewport">
				{items.length === 0
					? <div class="row__empty">{emptyText || tr('Nothing here yet', 'Здесь пока ничего нет')}</div>
					: <div
						ref={track}
						class={`row__track${fits ? ' row__track--centered' : ''}${last.current.fast ? ' row__track--fast' : ''}${edge.current.raf ? ' row__track--free' : ''}`}
						style={{transform: `translate3d(${-Math.round(shown)}px, 0, 0)`}}
						onMouseMove={moved}
						onMouseLeave={() => { stopEdge(); if (onPointer) onPointer(null); }}
						onWheel={(event) => { if (onWheelStep && event.deltaY !== 0) onWheelStep(event.deltaY > 0 ? 1 : -1); }}
					>
						{items.map((item, i) => (
							<Tile
								key={item.key}
								model={item}
								focused={(ring === undefined ? focused : ring) && i === selected}
								dockNear={magnify ? true : Math.abs(i - selected) <= DOCK.length + 1}
								dockTransform={magnify ? `translate3d(${Math.round(shift[i])}px, 0, 0) scale(${(1 + extra[i] / tileSize).toFixed(3)})` : undefined}
								onHover={onPointer ? undefined : () => onSelect(i)}
								onActivate={() => onActivate(i)}
							/>
						))}
					</div>}
			</div>
		</section>
	);
}
