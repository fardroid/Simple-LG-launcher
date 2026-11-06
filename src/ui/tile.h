// A tile of an app or an input (components/Tile.tsx): its box with the artwork, an input's glyph, a built-in symbol or a
// monogram. The home row and the grids of Settings frame it each in their own way.
#pragma once

#include "gfx/gfx.h"
#include "ui/theme.h"

#include <string>

struct TileModel {
	std::string key, label;
	std::string imagePath;       // full-bleed artwork (app icons)
	std::string glyphPath;       // LG's grey line-art input icons, drawn smaller and lightened
	std::string glyph;           // a built-in symbol: "house", "tv", "gear"
	std::string color;           // the monogram's background when there is no artwork
};

/** The box at `frame` (radius `r`, grown by `s` from a tile of `ts` px) in colour `box` (a monogram's own colour wins), with what
 *  is in it. `icon_scale`: the icon size setting, in %. */
void draw_tile_art (const TileModel &m, Rect frame, float r, float s, int ts, int icon_scale, const Theme &t, Color box);
