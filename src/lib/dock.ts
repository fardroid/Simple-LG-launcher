// The magnified row of apps (a dock): how big each tile is, and which tile the pointer is on. Pure functions.

import {DOCK} from '../theme/tokens';

const clamp = (value: number, min: number, max: number): number => Math.min(max, Math.max(min, value));

/** The size of a tile `distance` tiles from the magnified spot: DOCK[0] right under it, falling smoothly to 1. */
export function dockScale (distance: number): number {
	if (distance >= DOCK.length) return 1;
	const a = Math.floor(distance), f = distance - a;
	const from = DOCK[a], to = a + 1 < DOCK.length ? DOCK[a + 1] : 1;
	return from + (to - from) * f;
}

/** Which tile (fractional) the pointer is on, for a pointer `px` from the strip's left edge. The magnified tiles are pushed
 *  apart (a tile far along the row sits up to a whole tile to the right of its place in the untouched layout), so the
 *  index is worked out where the tiles really are: start from the untouched layout and correct it a few times. */
export function pointerIndex (px: number, count: number, tileSize: number, step: number): number {
	let pos = clamp((px - tileSize / 2) / step, -0.4, count - 0.6);
	for (let round = 0; round < 4; round++) {
		const centres: number[] = [];
		let running = 0;
		for (let i = 0; i < count; i++) {
			const e = tileSize * (dockScale(Math.abs(i - pos)) - 1);
			centres.push(i * step + tileSize / 2 + running + e / 2);
			running += e;
		}
		let i = 0;
		while (i < count - 2 && px >= centres[i + 1]) i++;
		const span = (centres[i + 1] === undefined ? step : centres[i + 1] - centres[i]) || step;
		pos = clamp(i + (px - centres[i]) / span, -0.4, count - 0.6);
	}
	return pos;
}

/** Which tile (fractional, 0 = the first one's centre) a pointer at `x` is on, from where the tiles' centres really are on the
 *  screen (measured from the page, so it is right whatever the tiles are doing). */
export function indexAmong (centres: number[], x: number): number {
	const n = centres.length;
	if (n === 0) return 0;
	if (n === 1) return 0;
	let i = 0;
	while (i < n - 2 && x >= centres[i + 1]) i++;
	const span = centres[i + 1] - centres[i] || 1;
	return clamp(i + (x - centres[i]) / span, -0.4, n - 0.6);
}
