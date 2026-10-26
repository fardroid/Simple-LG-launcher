// The home screen's apps in one row or two, and moving the highlight around them.
// Pure functions (no UI), so they can be tested on their own.

export type Direction = 'left' | 'right' | 'up' | 'down';

/** How many tiles go in each row: one row takes everything; two rows split the tiles in half (the top row gets the extra one). */
export function splitRows (count: number, rows: number): number[] {
	if (rows < 2 || count < 2) return [count];
	const first = Math.ceil(count / 2);
	return [first, count - first];
}

/** Index of each row's first tile in the full list. */
export function rowOffsets (sizes: number[]): number[] {
	let sum = 0;
	return sizes.map((size) => { const start = sum; sum += size; return start; });
}

/** Which row has the highlight, and the column last highlighted in each row. */
export interface RowsPosition {
	row: number;
	cols: number[];
}

export function initialPosition (): RowsPosition {
	return {row: 0, cols: [0, 0]};
}

/** The position made valid for rows of these sizes (after the number of tiles or rows changed). */
export function clampPosition (sizes: number[], position: RowsPosition): RowsPosition {
	const row = Math.min(Math.max(0, position.row), Math.max(0, sizes.length - 1));
	const cols = sizes.map((size, r) => Math.min(Math.max(0, position.cols[r] || 0), Math.max(0, size - 1)));
	return {row, cols: cols.length ? cols : [0]};
}

/** Left and right move along the row and stop at its ends; up and down change row, keeping the same column where there is one. */
export function movePosition (sizes: number[], position: RowsPosition, direction: Direction): RowsPosition {
	const at = clampPosition(sizes, position);
	const cols = at.cols.slice();
	let row = at.row;
	if (direction === 'left') {
		cols[row] = Math.max(0, cols[row] - 1);
	} else if (direction === 'right') {
		cols[row] = Math.max(0, Math.min(sizes[row] - 1, cols[row] + 1));
	} else {
		const target = direction === 'up' ? row - 1 : row + 1;
		if (target >= 0 && target < sizes.length) {
			cols[target] = Math.max(0, Math.min(cols[row], sizes[target] - 1));
			row = target;
		}
	}
	return {row, cols};
}
