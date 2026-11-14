#include "ui/plates.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {

const float SW = 1920, SH = 1080;

float hex_brightness (const char *hex) {
	unsigned n = (unsigned)strtoul(hex + 1, nullptr, 16);
	return (0.2126f * ((n >> 16) & 255) + 0.7152f * ((n >> 8) & 255) + 0.0722f * (n & 255)) / 255;
}

#include "ui/themes.inc"

// the default background's three colours per theme
struct Stops { const char *start, *mid, *end; };
Stops stops_of (const std::string &theme) {
	const ThemeColors &t = theme_colors(theme);
	return {t.start, t.mid, t.end};
}

Rgba tint_of (const std::string &theme, float a) {
	unsigned n = (unsigned)strtoul(theme_colors(theme).plate + 1, nullptr, 16);
	return {((n >> 16) & 255) / 255.f, ((n >> 8) & 255) / 255.f, (n & 255) / 255.f, a};
}

const PlateLook LIGHT{{1, 1, 1, 0.11f}, {1, 1, 1, 0.06f}};

// The dark plate: the theme's colour (dusk keeps the web version's own). The card in focus lights up, as a light plate does: a
// darker tint over it made the chosen card the darkest of all.
PlateLook dark_look (const std::string &theme) {
	const Rgba focus{1, 1, 1, 0.1f};
	if (theme == "dusk") return {{12 / 255.f, 18 / 255.f, 46 / 255.f, 0.55f}, focus};
	return {tint_of(theme, 0.55f), focus};
}

}  // namespace

BrightnessMap map_from_pixels (const unsigned char *data) {
	BrightnessMap out;
	for (int my = 0; my < MAP_H; my++)
		for (int mx = 0; mx < MAP_W; mx++) {
			float sum = 0;
			for (int y = 0; y < 4; y++)
				for (int x = 0; x < 4; x++) {
					int i = ((my * 4 + y) * MAP_W * 4 + mx * 4 + x) * 4;
					sum += (0.2126f * data[i] + 0.7152f * data[i + 1] + 0.0722f * data[i + 2]) / 255;
				}
			out.push_back(std::round(sum / 16 * 1000) / 1000);
		}
	return out;
}

BrightnessMap theme_map (const std::string &theme) {
	Stops c = stops_of(theme);
	float at[3] = {0, 0.55f, 1}, v[3] = {hex_brightness(c.start), hex_brightness(c.mid), hex_brightness(c.end)};
	float angle = 155 * (float)M_PI / 180;
	float dx = std::sin(angle), dy = -std::cos(angle);
	float length = std::fabs(SW * dx) + std::fabs(SH * dy);
	BrightnessMap out;
	for (int my = 0; my < MAP_H; my++)
		for (int mx = 0; mx < MAP_W; mx++) {
			float x = (mx + 0.5f) / MAP_W * SW - SW / 2, y = (my + 0.5f) / MAP_H * SH - SH / 2;
			float t = std::min(1.f, std::max(0.f, (x * dx + y * dy) / length + 0.5f));
			int i = t < at[1] ? 0 : 1;
			float k = (t - at[i]) / (at[i + 1] - at[i]);
			out.push_back(v[i] + (v[i + 1] - v[i]) * k);
		}
	return out;
}

float region_brightness (const BrightnessMap &map, const std::array<int, 4> &r) {
	int x0 = std::max(0, (int)std::floor(r[0] / SW * MAP_W));
	int x1 = std::min(MAP_W, std::max(x0 + 1, (int)std::ceil((r[0] + r[2]) / SW * MAP_W)));
	int y0 = std::max(0, (int)std::floor(r[1] / SH * MAP_H));
	int y1 = std::min(MAP_H, std::max(y0 + 1, (int)std::ceil((r[1] + r[3]) / SH * MAP_H)));
	float sum = 0;
	int n = 0;
	for (int y = y0; y < y1; y++)
		for (int x = x0; x < x1; x++) { sum += (size_t)(y * MAP_W + x) < map.size() ? map[y * MAP_W + x] : 0; n++; }
	return n ? sum / n : 0;
}

PlateLook look_for (float b, const std::string &theme) {
	// one of the two looks, the same as Light and Dark: in between ones read as a third colour
	return b <= 0.34f ? LIGHT : dark_look(theme);
}

PlateLook plate_look (const std::string &mode, const std::string &theme, const BrightnessMap &map, const std::vector<std::array<int, 4>> &rects) {
	if (mode == "light") return LIGHT;
	if (mode == "dark") return dark_look(theme);
	float brightest = brightest_of(map.empty() ? theme_map(theme) : map, rects);
	return brightest < 0 ? LIGHT : look_for(brightest, theme);
}

float brightest_of (const BrightnessMap &map, const std::vector<std::array<int, 4>> &rects) {
	float brightest = -1;
	for (auto &r : rects) if (r[3] > 0) brightest = std::max(brightest, region_brightness(map, r));
	return brightest;
}
