// Where the cards stand (lib/boardlayout.ts): a block 686 px tall, moved so that the space above it and the space below it (to
// the top of the selected app icon) are the same; inside it, columns 40 px apart.
#pragma once

#include <optional>
#include <string>
#include <vector>

constexpr int BOARD_HEIGHT = 686;
constexpr int BOARD_GAP = 40;

struct BoardInput { int iconScale; int bottomMargin; bool showLabels; int rows; };
int board_top (const BoardInput &a, int item_count);

enum class CardId { clock, weather, calendar, news, quote };

struct ClockShape { int size; bool showDate; int dateSize; bool period; };
struct Cards { bool weather = false, calendar = false, news = false, quote = false; ClockShape clock{160, true, 100, false}; };

struct BoardCard { CardId id; int x, y, w, h; };
struct BoardColumn { int x, w; std::vector<BoardCard> cards; };
struct BoardLayout {
	bool narrow = false;
	std::vector<BoardColumn> columns;
	std::optional<BoardCard> card (CardId id) const;
};

int narrow_clock_size (const ClockShape &c);
int narrow_date_line (const ClockShape &c);
int clock_height (const ClockShape &c, bool narrow);
BoardLayout board_layout (const Cards &on, int top);

int column_at (const BoardLayout &l, int x);
std::optional<CardId> entry_card (const BoardLayout &l, int index);
std::optional<CardId> beside_card (const BoardLayout &l, CardId id, int step);
std::optional<CardId> next_in_column (const BoardLayout &l, CardId id, int step);
