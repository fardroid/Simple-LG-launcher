// A small anti-aliased painter into memory, for drawings made once and then shown as a texture (the weather's curve):
// thick lines and curves with round ends, filled shapes, circles, dashed lines.
#pragma once

#include "gfx/image.h"

#include <vector>

struct Pt { float x, y; };
struct Paint { float r, g, b, a; };

class Canvas {
public:
	Canvas (int w, int h);
	/** a polyline `width` wide with round joins and ends */
	void stroke (const std::vector<Pt> &pts, float width, Paint p);
	/** a closed polygon, filled with a vertical gradient from `top` (at y0) to `bottom` (at y1) */
	void fill (const std::vector<Pt> &poly, Paint top, Paint bottom, float y0, float y1);
	void circle (Pt c, float r, Paint fill, Paint stroke, float stroke_w);
	/** a dashed vertical line */
	void dashed_v (float x, float y0, float y1, float width, float dash, float gap, Paint p);
	/** the points of cubic Bézier segments (SVG's C), `n` per segment */
	static void cubic (std::vector<Pt> &out, Pt a, Pt c1, Pt c2, Pt b, int n);
	Pixels &pixels () { return px_; }

private:
	void blend (int x, int y, Paint p, float cover);
	Pixels px_;
};
