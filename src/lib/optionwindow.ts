// Which rows of a settings list are on screen. Rows are not all the same height (a description can
// take one, two or three lines), so the window is worked out from heights, not from a row count.

/** The room the list has on screen, in px at 1080p (below the title and tabs). */
export const OPTION_AREA = 800;

/** A row's height in px: the label, plus a line per ~85 characters of description, plus the gap below. */
export function optionHeight (description: string): number {
	const lines = Math.max(1, Math.ceil(description.length / 85));
	return 72 + 28 * lines + 11;
}

/** The first row to show. It stays where it was until the highlighted row would fall off the
 *  top or the bottom, so the list scrolls one row at a time, up as well as down. */
export function windowStart (heights: number[], selected: number, previousStart: number, area: number = OPTION_AREA): number {
	let start = Math.max(0, Math.min(previousStart, selected));
	const used = (from: number): number => {
		let sum = 0;
		for (let i = from; i <= selected && i < heights.length; i++) sum += heights[i];
		return sum;
	};
	while (start < selected && used(start) > area) start++;
	return start;
}

/** One past the last row that fits when the list starts at `start` (always at least one row). */
export function windowEnd (heights: number[], start: number, area: number = OPTION_AREA): number {
	let sum = 0;
	let end = start;
	while (end < heights.length && sum + heights[end] <= area) { sum += heights[end]; end++; }
	return Math.max(end, Math.min(start + 1, heights.length));
}
