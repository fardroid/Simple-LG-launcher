// Text, drawn once into a texture and kept (a line of text costs nothing more until it changes).
// Fonts are the TV's own (/usr/share/fonts) unless a style names a bundled one; characters a font lacks (Chinese, Japanese,
// Korean...) come from the TV's fallback fonts.
#pragma once

#include "gfx/gfx.h"

#include <string>
#include <vector>

enum class Face {
	ui,          // the screens' text: LG Smart UI (the web version: "Inter Variable", "LG Smart UI")
	tv,          // the clock's "TV font": LG Smart UI
	inter, poppins, oswald, mono, serif     // the clock's other fonts (bundled when present, else LG Smart UI)
};

struct TextStyle {
	Face face = Face::ui;
	int size = 24;            // px (CSS font-size)
	int weight = 400;         // 200-700
	float spacing = 0;        // extra px after each character (CSS letter-spacing)
	bool italic = false;      // the TV's font has its own; the bundled ones are slanted by FreeType
};

/** A line of text as a white texture with its alpha (tinted when drawn). */
struct TextLine {
	TexturePtr tex;
	int w = 0, h = 0;          // the texture's size: w is the advance width, h the font's line height
	int ascent = 0;            // from the top to the baseline
	int ink_top = 0, ink_bottom = 0;   // how far the drawn ink reaches, from the top (digits centring, as the web version's Clock)
	int pad = 0, scale = 1;    // a shadow: its margin round the line, and how many times smaller than drawn its texture is
};

namespace text {

void init (const std::string &bundled_fonts_dir);
/** How FreeType fits the letters to the pixels: "subpixel" (the default: light, the letters placed to a fraction of a pixel), "light", "normal", "mono" or "none" (for comparing on the TV). */
void set_hinting (const std::string &mode);
/** The line, made or from the cache (main thread). */
const TextLine &line (const std::string &s, const TextStyle &st);
/** The line's soft shadow: its alpha blurred by `blur` px (CSS text-shadow's blur), same placement plus a margin of its `pad`; a wide
 *  blur is made at a fraction of the size (`scale`) and drawn stretched. */
const TextLine &shadow (const std::string &s, const TextStyle &st, float blur);
int shadow_pad (float blur);
/** Width of a line without drawing it. */
int width (const std::string &s, const TextStyle &st);
/** Line height of the style (the font's own). */
int height (const TextStyle &st);
/** Words wrapped into at most `max_lines` lines of `max_w` px (the last one cut with "…" when there is more). A "\n" in the text
 *  starts a new line (CSS white-space: pre-line). */
std::vector<std::string> wrap (const std::string &s, const TextStyle &st, int max_w, int max_lines);
/** One line cut with "…" to fit `max_w` (CSS text-overflow: ellipsis). */
std::string ellipsize (const std::string &s, const TextStyle &st, int max_w);
/** Capitals (CSS text-transform: uppercase) for Latin, Cyrillic and Greek. */
std::string upper (const std::string &s);
/** Drops lines not used for a while (call once in a while). */
void trim ();

/** Draws a line with its top-left corner at x, y; with `to`, filled with a gradient from `c` (left) to `to` (right). */
void draw (const std::string &s, const TextStyle &st, float x, float y, Color c, float opacity = 1, const Color *to = nullptr);
/** A CSS text-shadow: `0 dy blur color` under the line at x, y. */
void draw_shadow (const std::string &s, const TextStyle &st, float x, float y, float dy, float blur, Color c);

}  // namespace text
