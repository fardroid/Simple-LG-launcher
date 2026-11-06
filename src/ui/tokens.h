// Sizes and motion (theme/tokens.ts LAYOUT, FONT, MOTION): no drawing in here, so pure code and its tests can use it.
#pragma once

namespace layout {
constexpr int SCREEN_W = 1920, SCREEN_H = 1080;
constexpr int EDGE_PADDING = 140, HEADER_TOP = 84;
constexpr int TILE_SIZE = 150, TILE_GAP = 16, TILE_RADIUS = 24;
constexpr int ROW_LABEL_GAP = 14, ROW_GAP = 28, GRID_COLUMNS = 6;
constexpr int TILE_LABEL = 25;          // FONT.tileLabel
constexpr int ROW_LABEL = 20;
}  // namespace layout

namespace motion {
constexpr int FOCUS_MS = 160, SCROLL_MS = 260;
constexpr double FOCUS_SCALE = 1.12, LIFT_SCALE = 1.2, UNFOCUSED_ROW_OPACITY = 0.55;
}  // namespace motion
