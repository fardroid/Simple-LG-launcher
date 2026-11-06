#include "data/rows.h"

#include <algorithm>

std::vector<int> split_rows (int count, int rows) {
	if (rows < 2 || count < 2) return {count};
	int first = (count + 1) / 2;
	return {first, count - first};
}

std::vector<int> row_offsets (const std::vector<int> &sizes) {
	std::vector<int> out;
	int sum = 0;
	for (int s : sizes) { out.push_back(sum); sum += s; }
	return out;
}

RowsPosition clamp_position (const std::vector<int> &sizes, const RowsPosition &p) {
	RowsPosition out;
	out.row = std::min(std::max(0, p.row), std::max(0, (int)sizes.size() - 1));
	out.cols.clear();
	for (size_t r = 0; r < sizes.size(); r++) {
		int c = r < p.cols.size() ? p.cols[r] : 0;
		out.cols.push_back(std::min(std::max(0, c), std::max(0, sizes[r] - 1)));
	}
	if (out.cols.empty()) out.cols.push_back(0);
	return out;
}

RowsPosition move_position (const std::vector<int> &sizes, const RowsPosition &p, Dir d) {
	RowsPosition at = clamp_position(sizes, p);
	int row = at.row;
	if (d == Dir::left) at.cols[row] = std::max(0, at.cols[row] - 1);
	else if (d == Dir::right) at.cols[row] = std::max(0, std::min(sizes[row] - 1, at.cols[row] + 1));
	else {
		int target = d == Dir::up ? row - 1 : row + 1;
		if (target >= 0 && target < (int)sizes.size()) {
			at.cols[target] = std::max(0, std::min(at.cols[row], sizes[target] - 1));
			at.row = target;
		}
	}
	return at;
}
