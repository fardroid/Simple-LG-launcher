#include "ui/boardlayout.h"

#include "data/rows.h"
#include "ui/tokens.h"

#include <algorithm>
#include <climits>
#include <cmath>

namespace {
const int MIN_TOP = 16;
const int ROW_GAP = 14;
const int LEFT_WIDTH = 880, RIGHT_WIDTH = 600, THIRD_WIDTH = 560;
const int QUOTE_HEIGHT = 176;
const int CLOCK_PAD = 12, CLOCK_SIDE = 40;

bool focusable (CardId id) { return id == CardId::weather || id == CardId::calendar || id == CardId::news; }
}  // namespace

int board_top (const BoardInput &a, int item_count) {
	double scale = a.iconScale / 100.0;
	int tile = (int)std::lround(layout::TILE_SIZE * scale);
	int font = std::min(30, std::max(16, (int)std::lround(layout::TILE_LABEL * scale)));
	double label = a.showLabels ? font * 2.4 + 14 : 0;
	int rows = (int)split_rows(item_count, a.rows).size();
	double rows_height = rows * (tile + label) + (rows - 1) * ROW_GAP;
	double rows_top = layout::SCREEN_H - a.bottomMargin - rows_height;
	double biggest_top = rows_top - tile * (motion::FOCUS_SCALE - 1);
	return std::max(MIN_TOP, (int)std::lround((biggest_top - BOARD_HEIGHT) / 2));
}

std::optional<BoardCard> BoardLayout::card (CardId id) const {
	for (auto &c : columns) for (auto &card : c.cards) if (card.id == id) return card;
	return std::nullopt;
}

int narrow_clock_size (const ClockShape &c) {
	int room = THIRD_WIDTH - 2 * CLOCK_SIDE;
	return std::min(c.size, (int)std::floor(room / (c.period ? 3.05 : 2.65)));
}

int narrow_date_line (const ClockShape &c) { return (int)std::lround(32.0 * c.dateSize / 100 * 1.2); }

int clock_height (const ClockShape &c, bool narrow) {
	if (!narrow) return c.size + 2 * CLOCK_PAD;
	return narrow_clock_size(c) + 2 * CLOCK_PAD + (c.showDate ? narrow_date_line(c) + 8 : 0);
}

BoardLayout board_layout (const Cards &on, int top) {
	bool three = on.news && on.calendar;
	std::vector<std::vector<CardId>> lists;
	lists.push_back(on.weather ? std::vector<CardId>{CardId::clock, CardId::weather} : std::vector<CardId>{CardId::clock});
	if (three) {
		lists.push_back({CardId::calendar});
		lists.push_back(on.quote ? std::vector<CardId>{CardId::news, CardId::quote} : std::vector<CardId>{CardId::news});
	} else {
		std::vector<CardId> right;
		if (on.calendar) right.push_back(CardId::calendar);
		if (on.news) right.push_back(CardId::news);
		if (on.quote) right.push_back(CardId::quote);
		if (!right.empty()) lists.push_back(right);
	}
	std::vector<int> widths = three ? std::vector<int>{THIRD_WIDTH, THIRD_WIDTH, THIRD_WIDTH} : std::vector<int>{LEFT_WIDTH, RIGHT_WIDTH};
	int total = 0;
	for (size_t i = 0; i < lists.size(); i++) total += widths[i];
	total += BOARD_GAP * ((int)lists.size() - 1);
	int x = (int)std::lround((layout::SCREEN_W - total) / 2.0);
	auto fixed = [&] (CardId id, bool alone) -> int {
		if (id == CardId::clock) return clock_height(on.clock, three);
		if (id == CardId::quote) return alone ? BOARD_HEIGHT : QUOTE_HEIGHT;
		return -1;
	};
	BoardLayout out;
	out.narrow = three;
	for (size_t i = 0; i < lists.size(); i++) {
		const auto &ids = lists[i];
		bool alone = ids.size() == 1;
		std::vector<int> heights;
		int flexible = 0, used = BOARD_GAP * ((int)ids.size() - 1);
		for (auto id : ids) {
			int h = fixed(id, alone);
			heights.push_back(h);
			if (h < 0) flexible++; else used += h;
		}
		int share = flexible ? std::max(0, (int)std::floor((BOARD_HEIGHT - used) / (double)flexible)) : 0;
		BoardColumn col{x, widths[i], {}};
		int y = top;
		for (size_t k = 0; k < ids.size(); k++) {
			int h = heights[k] < 0 ? share : heights[k];
			col.cards.push_back({ids[k], x, y, widths[i], h});
			y += h + BOARD_GAP;
		}
		out.columns.push_back(col);
		x += widths[i] + BOARD_GAP;
	}
	return out;
}

int column_at (const BoardLayout &l, int x) {
	int best = -1;
	long distance = LONG_MAX;
	for (size_t i = 0; i < l.columns.size(); i++) {
		const auto &c = l.columns[i];
		if (std::none_of(c.cards.begin(), c.cards.end(), [] (const BoardCard &k) { return focusable(k.id); })) continue;
		long d = x < c.x ? c.x - x : x > c.x + c.w ? x - c.x - c.w : 0;
		if (d < distance) { distance = d; best = (int)i; }
	}
	return best;
}

std::optional<CardId> entry_card (const BoardLayout &l, int index) {
	if (index < 0 || index >= (int)l.columns.size()) return std::nullopt;
	std::optional<CardId> out;
	for (auto &c : l.columns[index].cards) if (focusable(c.id)) out = c.id;
	return out;
}

std::optional<CardId> beside_card (const BoardLayout &l, CardId id, int step) {
	int index = -1;
	for (size_t i = 0; i < l.columns.size(); i++)
		for (auto &c : l.columns[i].cards) if (c.id == id) index = (int)i;
	if (index == -1) return std::nullopt;
	for (int i = index + step; i >= 0 && i < (int)l.columns.size(); i += step) {
		auto card = entry_card(l, i);
		if (card) return card;
	}
	return std::nullopt;
}

std::optional<CardId> next_in_column (const BoardLayout &l, CardId id, int step) {
	for (auto &col : l.columns) {
		int i = -1;
		for (size_t k = 0; k < col.cards.size(); k++) if (col.cards[k].id == id) i = (int)k;
		if (i == -1) continue;
		for (int j = i + step; j >= 0 && j < (int)col.cards.size(); j += step)
			if (focusable(col.cards[j].id)) return col.cards[j].id;
		return std::nullopt;
	}
	return std::nullopt;
}
