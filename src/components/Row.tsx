import {useRef} from 'preact/hooks';
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
}

/** Horizontal strip of tiles. Centred when it fits; otherwise a carousel that keeps
 *  the selected tile in the middle (clamped at the ends). With `magnify` the tiles near the selected one (or near
 *  the pointer, continuously) grow, and the tiles move apart to make room (only transforms change, so it stays smooth). */
export function Row ({label, items, focused, selected, emptyText, onSelect, onActivate, tileSize = LAYOUT.tileSize, tileGap = LAYOUT.tileGap, viewportWidth = LAYOUT.screenWidth - 2 * LAYOUT.edgePadding, magnify, dock, ring, scrollIndex, pointerPos, onPointer, onWheelStep}: Props) {
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
	const moved = (event: MouseEvent) => {
		if (!onPointer || !track.current) return;
		// where the tiles' centres really are on the screen, so the tile under the pointer is the one that grows
		const centres: number[] = [];
		for (let i = 0; i < track.current.children.length; i++) {
			const r = track.current.children[i].getBoundingClientRect();
			centres.push((r.left + r.right) / 2);
		}
		frame.current.pos = indexAmong(centres, event.clientX);
		if (frame.current.queued) return;
		frame.current.queued = true;
		window.requestAnimationFrame(() => { frame.current.queued = false; onPointer(frame.current.pos); });
	};

	return (
		<section class={`row${focused ? ' row--focused' : ''}${(dock === undefined ? magnify : dock) ? ' row--dock' : ''}${pointerPos !== null && pointerPos !== undefined ? ' row--pointer' : ''}`}>
			{label && <h2 class="row__label">{label}</h2>}
			<div class="row__viewport">
				{items.length === 0
					? <div class="row__empty">{emptyText || tr('Nothing here yet', 'Здесь пока ничего нет')}</div>
					: <div
						ref={track}
						class={`row__track${fits ? ' row__track--centered' : ''}${last.current.fast ? ' row__track--fast' : ''}`}
						style={{transform: `translate3d(${-offset}px, 0, 0)`}}
						onMouseMove={moved}
						onMouseLeave={() => { if (onPointer) onPointer(null); }}
						onWheel={(event) => { if (onWheelStep && event.deltaY !== 0) onWheelStep(event.deltaY > 0 ? 1 : -1); }}
					>
						{items.map((item, i) => (
							<Tile
								key={item.key}
								model={item}
								focused={(ring === undefined ? focused : ring) && i === selected}
								dockNear={magnify ? true : Math.abs(i - selected) <= DOCK.length + 1}
								dockTransform={magnify ? `translate3d(${shift[i].toFixed(1)}px, 0, 0) scale(${(1 + extra[i] / tileSize).toFixed(3)})` : undefined}
								onHover={onPointer ? undefined : () => onSelect(i)}
								onActivate={() => onActivate(i)}
							/>
						))}
					</div>}
			</div>
		</section>
	);
}
