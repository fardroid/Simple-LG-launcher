// Where the clock / weather / calendar block stands. It keeps its size (686 px tall); it is moved so that the space above it (to the top
// of the screen) and the space below it (to the top of the selected app icon, which grows a little) are the same.
// That depends on the icon size, the bottom margin, the names under the icons and the rows.

import {FONT, LAYOUT, MOTION} from '../theme/tokens';
import {splitRows} from './rows';

export const BOARD_HEIGHT = 686;
const MIN_TOP = 16;
const ROW_GAP = 14;           // between two rows of apps (see .rows--multi in global.css)

export interface BoardInput {
	iconScale: number;
	bottomMargin: number;
	showLabels: boolean;
	rows: number;
}

/** The distance from the top of the 1080p screen to the block, in px. `itemCount` is how many tiles are on the home row. */
export function boardTop (a: BoardInput, itemCount: number): number {
	const scale = a.iconScale / 100;
	const tile = Math.round(LAYOUT.tileSize * scale);
	const font = Math.min(30, Math.max(16, Math.round(FONT.tileLabel * scale)));
	const label = a.showLabels ? font * 2.4 + 14 : 0;
	const rows = splitRows(itemCount, a.rows).length;
	const rowsHeight = rows * (tile + label) + (rows - 1) * ROW_GAP;
	const rowsTop = LAYOUT.screenHeight - a.bottomMargin - rowsHeight;
	const biggestTop = rowsTop - tile * (MOTION.focusScale - 1);       // the selected tile grows upwards from its bottom edge
	return Math.max(MIN_TOP, Math.round((biggestTop - BOARD_HEIGHT) / 2));
}
