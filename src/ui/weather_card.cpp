#include "ui/weather_card.h"

#include "data/apps.h"
#include "gfx/canvas.h"
#include "gfx/text.h"
#include "sys/fs.h"
#include "ui/icons.h"
#include "ui/theme.h"

#include <algorithm>
#include <cmath>

namespace {

// the curve's own box (WeatherCard.tsx): points between y 46 (warmest) and 108 (coldest), 50 px in from the sides
const float TOP = 46, BOTTOM = 108, EDGE = 50, CURVE_H = 150;

Color icon_tint (const std::string &icon) {
	if (icon == "sun" || icon == "partly") return Color::hex("#ffc857");
	if (icon == "moon" || icon == "cmoon") return Color::hex("#dfe8ff");
	if (icon == "rain" || icon == "storm") return Color::hex("#8cc4ff");
	return Color::hex("#F2F5F8");
}

void draw_icon (const std::string &icon, Rect r) {
	TexturePtr t = icons::get(join_path(package_dir(), "glyphs/" + icon + ".png"), (int)r.w);
	if (t) gfx::image(*t, r, 1, 0, icon_tint(icon));
}

// A line of text whose CSS line box (`line` px tall) starts at `top`.
void line_at (const std::string &s, const TextStyle &st, float x, float top, float line, Color c, float opacity = 1) {
	const TextLine &l = text::line(s, st);
	text::draw(s, st, std::round(x), std::round(top + (line - l.h) / 2), c, opacity);
}

// A line of text on a baseline (SVG <text>), centred on x.
void centred_on_baseline (const std::string &s, const TextStyle &st, float x, float baseline, Color c) {
	const TextLine &l = text::line(s, st);
	text::draw(s, st, std::round(x - l.w / 2.f), std::round(baseline - l.ascent), c);
}

struct Curve { std::string key; TexturePtr tex; };
Curve cached;

// The curve's picture: the soft fill under it, the line, the points. Made again when the hours or the width change.
const Texture *curve_texture (const WeatherView &v, int W) {
	const Color accent = current_theme().accent, dot = current_theme().plate;     // the theme's
	const Paint AMBER{accent.r, accent.g, accent.b, 1};
	std::string key = std::to_string(W) + "|" + std::to_string(AMBER.r) + std::to_string(AMBER.g) + std::to_string(AMBER.b);
	for (auto &h : v.hours) key += "|" + std::to_string(h.tempC);
	if (cached.key == key && cached.tex) return cached.tex.get();
	auto &hours = v.hours;
	double lo = 1e9, hi = -1e9;
	for (auto &h : hours) { lo = std::min(lo, h.tempC); hi = std::max(hi, h.tempC); }
	double span = std::max(hi - lo, 3.0);
	std::vector<Pt> pts;
	for (size_t i = 0; i < hours.size(); i++)
		pts.push_back({EDGE + (W - 2 * EDGE) * (float)i / (hours.size() - 1), (float)(BOTTOM - (hours[i].tempC - lo) / span * (BOTTOM - TOP))});
	std::vector<Pt> path{pts[0]};
	for (size_t i = 1; i < pts.size(); i++) {
		float cx = (pts[i - 1].x + pts[i].x) / 2;
		Canvas::cubic(path, pts[i - 1], {cx, pts[i - 1].y}, {cx, pts[i].y}, pts[i], 16);
	}
	Canvas c(W, (int)CURVE_H);
	// the fill: down to y 118, the gradient over its own height (#ffc36b, 0.34 to 0), the whole at 0.5
	std::vector<Pt> area = path;
	area.push_back({pts.back().x, 118});
	area.push_back({pts.front().x, 118});
	float top = 118;
	for (auto &p : path) top = std::min(top, p.y);
	c.fill(area, {AMBER.r, AMBER.g, AMBER.b, 0.34f * 0.5f}, {AMBER.r, AMBER.g, AMBER.b, 0}, top, 118);
	c.stroke(path, 3.5f, AMBER);
	for (auto &p : pts) c.circle(p, 6, {dot.r, dot.g, dot.b, 1}, AMBER, 3.5f);
	cached.key = key;
	cached.tex = make_texture(c.pixels());
	return cached.tex.get();
}

}  // namespace

void draw_weather (const BoardCard &card, const WeatherView &v, int card_size, Face face, const std::tm &now, float focus) {
	const Theme &t = current_theme();
	float k = card_size / 100.f;
	int t_hero = (int)std::lround(64 * k), t1 = (int)std::lround(22 * k), t2 = (int)std::lround(17 * k), t3 = (int)std::lround(14 * k);
	float x0 = card.x + 32.f, w_in = card.w - 64.f, y0 = card.y + 26.f;

	// now: the picture, the temperature, the words
	float now_h = 68 * k, cy = y0 + now_h / 2;
	float icon = 60 * k;
	draw_icon(v.icon, {x0, cy - icon / 2, icon, icon});
	TextStyle hero{face, t_hero, 300, -2};
	float x = x0 + icon + 20;
	line_at(v.temp, hero, x, cy - t_hero / 2.f, (float)t_hero, t.textPrimary);
	x += text::width(v.temp, hero) + 24;
	TextStyle main{face, t1, 500, 0}, detail{face, t2, 400, 0};
	float main_lh = t1 * 1.25f, detail_lh = t2 * 1.3f;
	float block = main_lh + (v.detail.empty() ? 0 : detail_lh);
	line_at(v.text, main, x, cy - block / 2, main_lh, t.textPrimary);
	if (!v.detail.empty()) line_at(v.detail, detail, x, cy - block / 2 + main_lh, detail_lh, t.textSecondary);

	// the hours
	float hours_y = y0 + now_h + 6;
	int W = (int)w_in;
	if (v.hours.size() >= 2) {
		const Texture *curve = curve_texture(v, W);
		gfx::image(*curve, {x0, hours_y, (float)W, CURVE_H});
		// the present moment (today only): a dashed line
		if (v.day == 0) {
			int first = v.hours.front().hour, last = v.hours.back().hour;
			double hour_now = now.tm_hour + now.tm_min / 60.0;
			float nx = x0 + EDGE + (W - 2 * EDGE) * (float)std::min(1.0, std::max(0.0, (hour_now - first) / std::max(1, last - first)));
			for (float y = 22; y < 120; y += 9) gfx::rect({nx - 1, hours_y + y, 2, std::min(4.f, 120 - y)}, 0, {1, 1, 1, 0.55f});
		}
		double lo = 1e9, hi = -1e9;
		for (auto &h : v.hours) { lo = std::min(lo, h.tempC); hi = std::max(hi, h.tempC); }
		double span = std::max(hi - lo, 3.0);
		TextStyle temp{face, t2, 600, 0}, hour{face, t3, 400, 0};
		for (size_t i = 0; i < v.hours.size(); i++) {
			float px = x0 + EDGE + (W - 2 * EDGE) * (float)i / (v.hours.size() - 1);
			float py = hours_y + (float)(BOTTOM - (v.hours[i].tempC - lo) / span * (BOTTOM - TOP));
			centred_on_baseline(v.hours[i].temp, temp, px, py - 14, {1, 1, 1, 1});
			centred_on_baseline(v.hours[i].label, hour, px, hours_y + 144, {1, 1, 1, 0.62f});
		}
	}

	// the days
	float days_y = hours_y + CURVE_H + 6, days_h = card.y + card.h - 26 - days_y;
	size_t n = v.days.size();
	if (!n || days_h <= 0) return;
	float dw = (w_in - 8 * (n - 1)) / n;
	TextStyle name{face, t2, 400, 0}, high{face, t1, 600, 0}, low{face, t2, 400, 0};
	float day_icon = 36 * k;
	float column = t2 + 4 + day_icon + 4 + t1 + 5 + t2;
	for (size_t i = 0; i < n; i++) {
		const DayView &d = v.days[i];
		float dx = x0 + i * (dw + 8), mid = dx + dw / 2;
		if (d.selected) gfx::rect({dx, days_y, dw, days_h}, 24, {1, 1, 1, 0.14f + 0.07f * focus});
		float y = days_y + (days_h - column) / 2;
		const TextLine &nl = text::line(d.label, name);
		line_at(d.label, name, mid - nl.w / 2.f, y, (float)t2, d.today ? t.accent : t.textSecondary);
		y += t2 + 4;
		draw_icon(d.icon, {mid - day_icon / 2, y, day_icon, day_icon});
		y += day_icon + 4;
		const TextLine &hl = text::line(d.high, high);
		line_at(d.high, high, mid - hl.w / 2.f, y, (float)t1, t.textPrimary);
		y += t1 + 5;
		const TextLine &ll = text::line(d.low, low);
		line_at(d.low, low, mid - ll.w / 2.f, y, (float)t2, t.textSecondary);
	}
}

int weather_day_at (const BoardCard &card, int days, int card_size, float x, float y) {
	// the same places as draw_weather's
	float k = card_size / 100.f;
	float x0 = card.x + 32.f, w_in = card.w - 64.f, y0 = card.y + 26.f;
	float days_y = y0 + 68 * k + 6 + CURVE_H + 6, days_h = card.y + card.h - 26 - days_y;
	if (days <= 0 || days_h <= 0 || y < days_y || y > days_y + days_h || x < x0 || x > x0 + w_in) return -1;
	float dw = (w_in - 8 * (days - 1)) / days;
	int i = (int)std::floor((x - x0) / (dw + 8));
	return i >= 0 && i < days && x - x0 - i * (dw + 8) <= dw ? i : -1;
}
