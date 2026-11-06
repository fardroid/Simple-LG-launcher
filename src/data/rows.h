// The home screen's apps in one row or two, and moving the highlight around them (lib/rows.ts).
#pragma once

#include <vector>

enum class Dir { left, right, up, down };

struct RowsPosition {
	int row = 0;
	std::vector<int> cols{0, 0};
};

/** How many tiles go in each row: one row takes everything; two rows split them in half (the top row gets the extra one). */
std::vector<int> split_rows (int count, int rows);
/** Index of each row's first tile in the full list. */
std::vector<int> row_offsets (const std::vector<int> &sizes);
/** The position made valid for rows of these sizes. */
RowsPosition clamp_position (const std::vector<int> &sizes, const RowsPosition &p);
/** Left and right move along the row and stop at its ends; up and down change row, keeping the column where there is one. */
RowsPosition move_position (const std::vector<int> &sizes, const RowsPosition &p, Dir d);
