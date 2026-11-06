#include "ui/clock.h"

#include "gfx/text.h"
#include "i18n/dates.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

struct FontInfo { const char *id; Face face; double capTop; };
// lib/clockfont.ts: where the tops of the digits are below the top of a line of the font (the date lines up with it)
const FontInfo FONTS[] = {
	{"inter", Face::inter, 0.137}, {"tv", Face::tv, 0.14}, {"poppins", Face::poppins, 0.15},
	{"oswald", Face::oswald, 0.14}, {"mono", Face::mono, 0.178}, {"serif", Face::serif, 0.19}
};

struct ColorInfo { const char *id; const char *color; const char *to; const char *accent; bool dark; };
// lib/clockcolor.ts: a plain colour, or a gradient from `color` (left) to `to` (right) filling each line
const ColorInfo COLORS[] = {
	{"white", "#F2F2F2", nullptr, "#F2F2F2", false}, {"warm", "#FFE9C7", nullptr, "#FFE9C7", false}, {"blue", "#C4E2FF", nullptr, "#C4E2FF", false},
	{"amber", "#FFB74D", nullptr, "#FFB74D", false}, {"grey", "#9AA0A6", nullptr, "#9AA0A6", false}, {"charcoal", "#3C4043", nullptr, "#3C4043", true},
	{"black", "#0A0A0A", nullptr, "#0A0A0A", true}, {"ember", "#D62839", "#F08C1A", "#F0803F", false}, {"ocean", "#1E4FD8", "#19C6D8", "#1F8BDA", true},
	{"dusk", "#6A2FC9", "#F2634F", "#A04A9A", true}
};

const FontInfo &font_info (const std::string &id) {
	for (const auto &f : FONTS) if (id == f.id) return f;
	return FONTS[0];
}

const ColorInfo &color_info (const std::string &id) {
	for (const auto &c : COLORS) if (id == c.id) return c;
	return COLORS[0];
}

// The y of a line's texture inside a CSS line box `line` px tall (the font's own content area centred in it).
float in_line (float top, float line, const TextLine &l) { return top + (line - l.h) / 2; }

struct Shadow { float dy, blur; Color c; };

// `to`: a gradient fill; its soft shadow is then the one drop shadow of .clock--gradient
void with_shadows (const std::string &s, const TextStyle &st, float x, float y, const Shadow *sh, int n, Color c, float opacity, const Color *to) {
	if (to) text::draw_shadow(s, st, x, y, 2, 6, {0, 0, 0, 0.3f * opacity});
	else for (int i = 0; i < n; i++) text::draw_shadow(s, st, x, y, sh[i].dy, sh[i].blur, sh[i].c.with_alpha(sh[i].c.a * opacity));
	text::draw(s, st, x, y, c, opacity, to);
}

}  // namespace

Face clock_face (const std::string &font) { return font_info(font).face; }

void draw_clock (const BoardCard &card, const std::tm &now, const ClockConfig &cfg, const std::string &lang, bool h24, bool narrow) {
	const FontInfo &fi = font_info(cfg.font);
	const ColorInfo &ci = color_info(cfg.color);
	Color color = Color::hex(ci.color);
	Color to_color = ci.to ? Color::hex(ci.to) : Color();
	const Color *to = ci.to ? &to_color : nullptr;
	float opacity = cfg.opacity / 100.f;
	TimeParts tp = time_parts(now, h24);
	bool period = !h24 && cfg.showPeriod;
	ClockShape shape{cfg.size, cfg.showDate, cfg.dateSize, period};
	int size = narrow ? narrow_clock_size(shape) : cfg.size;
	int date_size = (int)std::lround(32.0 * cfg.dateSize / 100);
	int date_line = (int)std::lround(date_size * 1.2);

	// .clock__time and the others' soft shadows (light ones behind a dark colour)
	Shadow time_sh[2] = {{2, 6, {0, 0, 0, 0.28f}}, {4, 44, {0, 0, 0, 0.5f}}};
	Shadow date_sh[2] = {{2, 6, {0, 0, 0, 0.35f}}, {2, 24, {0, 0, 0, 0.5f}}};
	if (ci.dark) {
		time_sh[0] = date_sh[0] = {1, 3, {1, 1, 1, 0.35f}};
		time_sh[1] = date_sh[1] = {2, 20, {1, 1, 1, 0.25f}};
	}

	TextStyle ts{fi.face, size, cfg.weight, (float)std::lround(size * 0.03)};
	TextStyle ps{fi.face, (int)std::lround(size * 0.27), 300, 0};
	ps.spacing = (float)std::lround(ps.size * 0.1);
	const TextLine &tl = text::line(tp.time, ts);
	int period_w = period ? (int)std::lround(size * 0.12) + text::width(tp.period, ps) : 0;
	// CSS letter-spacing comes after every character, the last one too
	int time_w = tl.w + (int)ts.spacing + period_w;

	std::string day = day_name(lang, now), date = date_year(lang, now);
	TextStyle day_st{fi.face, date_size, 500, 0};      // (the 0.02em letter-spacing is under a pixel: left out)
	TextStyle date_st{fi.face, date_size, 400, 0};

	float top = card.y + 12;
	float time_x, date_x = 0, date_y = 0;
	if (!narrow) {
		time_x = card.x + 40;
		date_x = time_x + (float)std::ceil(time_w) + (float)std::lround(size * 0.2);
		int digits_top = (int)std::lround(size * fi.capTop);
		date_y = top + std::max(0, digits_top - (int)std::lround(date_size * 0.24));
	} else {
		time_x = card.x + (card.w - time_w) / 2.f;
		date_y = top + size + 8;
	}
	float time_y = in_line(top, (float)size, tl);
	// the digits centred in the card by what the font really draws, time and date moved together by whole pixels
	bool date_taller = !narrow && cfg.showDate && (date_y - top) + 2 * date_line > size;
	if (!narrow && !date_taller && tl.ink_bottom > tl.ink_top) {
		float above = time_y + tl.ink_top - card.y, below = card.y + card.h - (time_y + tl.ink_bottom);
		float shift = std::round((below - above) / 2);
		time_y += shift;
		date_y += shift;
	}
	time_y = std::round(time_y);
	with_shadows(tp.time, ts, time_x, time_y, time_sh, 2, color, opacity, to);
	if (period) {
		const TextLine &pl = text::line(tp.period, ps);
		float px = time_x + tl.w + (float)std::lround(size * 0.12);
		float py = std::round(time_y + tl.ascent - pl.ascent);
		with_shadows(tp.period, ps, px, py, date_sh, 2, color, opacity * 0.75f, to);
	}
	if (!cfg.showDate) return;
	if (!narrow) {
		const TextLine &dl = text::line(day, day_st), &yl = text::line(date, date_st);
		with_shadows(day, day_st, date_x, std::round(in_line(date_y, (float)date_line, dl)), date_sh, 2, color, opacity, to);
		with_shadows(date, date_st, date_x, std::round(in_line(date_y + date_line, (float)date_line, yl)), date_sh, 2, color, opacity * 0.75f, to);
	} else {
		std::string rest = ", " + date;
		int dw = text::width(day, day_st), rw = text::width(rest, date_st);
		float x = card.x + (card.w - dw - rw) / 2.f;
		const TextLine &dl = text::line(day, day_st);
		float y = std::round(in_line(date_y, (float)date_line, dl));
		with_shadows(day, day_st, x, y, date_sh, 2, color, opacity, to);
		with_shadows(rest, date_st, x + dw, y, date_sh, 2, color, opacity * 0.75f, to);
	}
}
