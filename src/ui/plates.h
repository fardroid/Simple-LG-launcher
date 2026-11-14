// The plates behind the clock and the cards (lib/plates.ts). In "auto" their colour follows the brightness of the wallpaper
// behind them; all get the same colour, chosen for the brightest place any of them covers. The wallpaper is measured once,
// when it appears, as a coarse grid of brightness values (16 x 9).
#pragma once

#include <array>
#include <string>
#include <vector>

constexpr int MAP_W = 16, MAP_H = 9;
using BrightnessMap = std::vector<float>;          // MAP_W * MAP_H, 0-1

struct Rgba { float r, g, b, a; };
struct PlateLook { Rgba bg; Rgba focus; };

/** Brightness grid of an image drawn at 64 x 36 (RGBA bytes): the mean of 4 x 4 blocks. */
BrightnessMap map_from_pixels (const unsigned char *rgba64x36);
/** The brightness grid of a theme's own background (its 155 degree gradient). */
BrightnessMap theme_map (const std::string &theme);
/** Average brightness under a rectangle of the 1920 x 1080 screen. */
float region_brightness (const BrightnessMap &map, const std::array<int, 4> &rect);
/** The brightest place under any of `rects` (-1: none). */
float brightest_of (const BrightnessMap &map, const std::vector<std::array<int, 4>> &rects);
/** The plate's colour for a background of this brightness. */
PlateLook look_for (float brightness, const std::string &theme = "dusk");
/** The one look of all the plates: `rects` the cards on the screen, `map` the wallpaper's grid (empty: the theme's). */
PlateLook plate_look (const std::string &mode, const std::string &theme, const BrightnessMap &map, const std::vector<std::array<int, 4>> &rects);
