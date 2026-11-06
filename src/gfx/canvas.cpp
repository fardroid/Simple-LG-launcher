#include "gfx/canvas.h"

#include <algorithm>
#include <cmath>

Canvas::Canvas (int w, int h) {
	px_.w = w;
	px_.h = h;
	px_.rgba.assign((size_t)w * h * 4, 0);
}

// "source over", in straight alpha
void Canvas::blend (int x, int y, Paint p, float cover) {
	if (x < 0 || y < 0 || x >= px_.w || y >= px_.h || cover <= 0) return;
	unsigned char *o = &px_.rgba[((size_t)y * px_.w + x) * 4];
	float sa = p.a * std::min(1.f, cover), da = o[3] / 255.f;
	float oa = sa + da * (1 - sa);
	if (oa <= 0) return;
	float src[3] = {p.r, p.g, p.b};
	for (int k = 0; k < 3; k++) o[k] = (unsigned char)std::lround((src[k] * 255 * sa + o[k] * da * (1 - sa)) / oa);
	o[3] = (unsigned char)std::lround(oa * 255);
}

static float seg_dist (Pt p, Pt a, Pt b) {
	float dx = b.x - a.x, dy = b.y - a.y, l = dx * dx + dy * dy;
	float t = l > 0 ? std::max(0.f, std::min(1.f, ((p.x - a.x) * dx + (p.y - a.y) * dy) / l)) : 0;
	float ex = a.x + t * dx - p.x, ey = a.y + t * dy - p.y;
	return std::sqrt(ex * ex + ey * ey);
}

void Canvas::stroke (const std::vector<Pt> &pts, float width, Paint p) {
	if (pts.size() < 2) return;
	float half = width / 2;
	float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
	for (auto &q : pts) { x0 = std::min(x0, q.x); y0 = std::min(y0, q.y); x1 = std::max(x1, q.x); y1 = std::max(y1, q.y); }
	for (int y = std::max(0, (int)(y0 - half - 2)); y <= std::min(px_.h - 1, (int)(y1 + half + 2)); y++)
		for (int x = std::max(0, (int)(x0 - half - 2)); x <= std::min(px_.w - 1, (int)(x1 + half + 2)); x++) {
			Pt c{x + 0.5f, y + 0.5f};
			float d = 1e9f;
			for (size_t i = 1; i < pts.size(); i++) d = std::min(d, seg_dist(c, pts[i - 1], pts[i]));
			blend(x, y, p, std::max(0.f, std::min(1.f, half + 0.5f - d)));
		}
}

void Canvas::fill (const std::vector<Pt> &poly, Paint top, Paint bottom, float g0, float g1) {
	if (poly.size() < 3) return;
	float y0 = 1e9f, y1 = -1e9f;
	for (auto &q : poly) { y0 = std::min(y0, q.y); y1 = std::max(y1, q.y); }
	// four samples per pixel row, the crossings of the polygon's edges give the covered spans
	const int SUB = 4;
	std::vector<float> cover(px_.w);
	for (int y = std::max(0, (int)y0); y <= std::min(px_.h - 1, (int)y1); y++) {
		std::fill(cover.begin(), cover.end(), 0.f);
		for (int s = 0; s < SUB; s++) {
			float sy = y + (s + 0.5f) / SUB;
			std::vector<float> xs;
			for (size_t i = 0; i < poly.size(); i++) {
				Pt a = poly[i], b = poly[(i + 1) % poly.size()];
				if ((a.y <= sy && b.y > sy) || (b.y <= sy && a.y > sy)) xs.push_back(a.x + (sy - a.y) / (b.y - a.y) * (b.x - a.x));
			}
			std::sort(xs.begin(), xs.end());
			for (size_t i = 0; i + 1 < xs.size(); i += 2)
				for (int x = std::max(0, (int)xs[i]); x <= std::min(px_.w - 1, (int)xs[i + 1]); x++) {
					float l = std::max((float)x, xs[i]), r = std::min((float)x + 1, xs[i + 1]);
					if (r > l) cover[x] += (r - l) / SUB;
				}
		}
		float t = g1 > g0 ? std::max(0.f, std::min(1.f, (y + 0.5f - g0) / (g1 - g0))) : 0;
		Paint p{top.r + (bottom.r - top.r) * t, top.g + (bottom.g - top.g) * t, top.b + (bottom.b - top.b) * t, top.a + (bottom.a - top.a) * t};
		for (int x = 0; x < px_.w; x++) if (cover[x] > 0) blend(x, y, p, cover[x]);
	}
}

void Canvas::circle (Pt c, float r, Paint fillp, Paint strokep, float sw) {
	float outer = r + sw / 2;
	for (int y = std::max(0, (int)(c.y - outer - 2)); y <= std::min(px_.h - 1, (int)(c.y + outer + 2)); y++)
		for (int x = std::max(0, (int)(c.x - outer - 2)); x <= std::min(px_.w - 1, (int)(c.x + outer + 2)); x++) {
			float d = std::hypot(x + 0.5f - c.x, y + 0.5f - c.y);
			blend(x, y, fillp, std::max(0.f, std::min(1.f, r + 0.5f - d)));
			blend(x, y, strokep, std::max(0.f, std::min(1.f, sw / 2 + 0.5f - std::fabs(d - r))));
		}
}

void Canvas::dashed_v (float x, float y0, float y1, float width, float dash, float gap, Paint p) {
	for (float y = y0; y < y1; y += dash + gap) {
		float end = std::min(y1, y + dash);
		for (int yy = (int)y; yy < (int)std::ceil(end); yy++) {
			float vc = std::min(end, yy + 1.f) - std::max(y, (float)yy);
			for (int xx = (int)(x - width / 2 - 1); xx <= (int)(x + width / 2 + 1); xx++) {
				float hc = std::min(x + width / 2, xx + 1.f) - std::max(x - width / 2, (float)xx);
				if (hc > 0 && vc > 0) blend(xx, yy, p, hc * vc);
			}
		}
	}
}

void Canvas::cubic (std::vector<Pt> &out, Pt a, Pt c1, Pt c2, Pt b, int n) {
	for (int i = 1; i <= n; i++) {
		float t = (float)i / n, u = 1 - t;
		out.push_back({u * u * u * a.x + 3 * u * u * t * c1.x + 3 * u * t * t * c2.x + t * t * t * b.x,
			u * u * u * a.y + 3 * u * u * t * c1.y + 3 * u * t * t * c2.y + t * t * t * b.y});
	}
}
