#include "ui/quick_panel.h"

#include "data/apps.h"
#include "data/quick.h"
#include "gfx/text.h"
#include "i18n/i18n.h"
#include "sys/fs.h"
#include "ui/icons.h"
#include "ui/theme.h"
#include "ui/tokens.h"

#include <SDL.h>

#include <algorithm>
#include <cmath>

namespace {

const float TILE_W = 236, TILE_H = 150, GAP = 22, BOTTOM = 84;
const Color PILL = Color::hex("#F2F2F7"), ON_PILL = Color::hex("#111216"), ON_PILL_VALUE = Color::hex("#3C3F47");

void put (const std::string &s, const TextStyle &st, float x, float top, float line, Color c, float opacity = 1) {
	const TextLine &l = text::line(s, st);
	text::draw(s, st, std::round(x), std::round(top + (line - l.h) / 2), c, opacity);
}

Color mix (Color a, Color b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t}; }

std::string glyph_of (const std::string &id) { return id == "tvsettings" ? "sys-gear" : "q-" + id; }

}  // namespace

void QuickPanel::open (double now) {
	open_ = true;
	ok_armed_ = false;
	index_ = 0;
	focus_from_ = -1;
	focus_.jump(1);
	shown_.set(1, now, 240, Ease::ease_out);
	list_open_ = false;
	list_shown_.jump(0);
}

void QuickPanel::close (double now) {
	open_ = false;
	list_open_ = false;
	shown_.set(0, now, 200, Ease::ease_out);
	list_shown_.set(0, now, 160, Ease::ease_out);
}

std::vector<std::string> QuickPanel::options_of (const std::string &id) const {
	// the TV's own list once it has come; until then the one the launcher knows
	auto it = options_.find(id);
	if (it != options_.end() && !it->second.empty()) return it->second;
	const QuickSetting *s = find_quick(id);
	return s ? s->values : std::vector<std::string>();
}

void QuickPanel::open_list (double now) {
	const std::string &id = quick_settings()[index_].id;
	auto list = options_of(id);
	if (list.empty()) return;
	auto cur = values_.find(id);
	auto at = cur == values_.end() ? list.end() : std::find(list.begin(), list.end(), cur->second);
	list_open_ = true;
	list_tile_ = index_;
	list_index_ = at == list.end() ? 0 : (int)(at - list.begin());
	list_from_ = -1;
	list_focus_.jump(1);
	list_shown_.jump(0);
	list_shown_.set(1, now, 200, Ease::ease_out);
}

void QuickPanel::close_list (double now) {
	list_open_ = false;
	list_shown_.set(0, now, 160, Ease::ease_out);
}

void QuickPanel::move_tile (int to, double now) {
	int n = (int)quick_settings().size(), before = index_;
	index_ = std::max(0, std::min(n - 1, to));
	if (index_ != before) { focus_from_ = before; focus_.jump(0); focus_.set(1, now, motion::FOCUS_MS, Ease::ease_out); }
}

bool QuickPanel::key_down (int sc, bool repeat, double now) {
	bool ok = sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER;
	if (ok) { if (!repeat) ok_armed_ = true; return true; }
	if (list_open_) {
		int n = (int)options_of(quick_settings()[list_tile_].id).size(), before = list_index_;
		if (sc == SDL_SCANCODE_UP) list_index_ = std::max(0, list_index_ - 1);
		else if (sc == SDL_SCANCODE_DOWN) list_index_ = std::min(n - 1, list_index_ + 1);
		else if (sc == SDL_SCANCODE_WEBOS_BACK || sc == SDL_SCANCODE_ESCAPE) close_list(now);
		else if (sc == SDL_SCANCODE_LEFT || sc == SDL_SCANCODE_RIGHT) { close_list(now); move_tile(index_ + (sc == SDL_SCANCODE_LEFT ? -1 : 1), now); }
		if (list_index_ != before) { list_from_ = before; list_focus_.jump(0); list_focus_.set(1, now, motion::FOCUS_MS, Ease::ease_out); }
		return true;
	}
	if (sc == SDL_SCANCODE_LEFT) move_tile(index_ - 1, now);
	else if (sc == SDL_SCANCODE_RIGHT) move_tile(index_ + 1, now);
	else if (sc == SDL_SCANCODE_UP || sc == SDL_SCANCODE_WEBOS_BACK || sc == SDL_SCANCODE_ESCAPE) { if (on_close) on_close(); }
	return true;
}

bool QuickPanel::key_up (int sc, double now) {
	if (sc != SDL_SCANCODE_RETURN && sc != SDL_SCANCODE_KP_ENTER) return false;
	if (!ok_armed_) return false;          // the release of an OK pressed before the panel opened
	ok_armed_ = false;
	if (list_open_) {
		const std::string &id = quick_settings()[list_tile_].id;
		auto list = options_of(id);
		close_list(now);
		if (list_index_ < (int)list.size()) {
			std::string v = list[list_index_];
			if (values_[id] != v) { values_[id] = v; if (on_choose) on_choose(id, v); }
		}
		return true;
	}
	const QuickSetting &s = quick_settings()[index_];
	if (!s.category.empty()) open_list(now);
	else if (on_activate) on_activate(s.id);
	return true;
}

bool QuickPanel::animating (double now) const {
	return shown_.moving(now) || focus_.moving(now) || list_shown_.moving(now) || list_focus_.moving(now);
}

void QuickPanel::draw_list (const Theme &t, double now) {
	float p = (float)list_shown_.at(now);
	if (p <= 0) return;
	const std::string &id = quick_settings()[list_tile_].id;
	auto list = options_of(id);
	if (list.empty()) return;
	const auto &all = quick_settings();
	int n = (int)all.size();
	float total = n * TILE_W + (n - 1) * GAP;
	float x0 = std::round((1920 - total) / 2), tile_top = 1080 - BOTTOM - TILE_H;
	TextStyle st{Face::ui, 25, 400, 0};
	const float ROW = 60, PAD = 10, CHECK = 26;
	float w = TILE_W + 60;
	for (auto &v : list) w = std::max(w, (float)text::width(quick_value_name(id, v), st) + 2 * 24 + CHECK + 16 + 2 * PAD);
	float h = list.size() * ROW + 2 * PAD;
	float cx = x0 + list_tile_ * (TILE_W + GAP) + TILE_W / 2;
	Rect box{std::round(std::max(40.f, std::min(1920 - 40 - w, cx - w / 2))), std::round(tile_top - 20 - h + (1 - p) * 24), std::round(w), h};
	gfx::set_alpha(p * (float)shown_.at(now));
	gfx::shadow(box, 28, 16, 50, {0, 0, 0, 0.55f});
	gfx::rect(box, 28, Color::hex("#1C1C20"));
	float f = (float)list_focus_.at(now);
	const std::string &cur = values_.count(id) ? values_.at(id) : std::string();
	for (size_t i = 0; i < list.size(); i++) {
		Rect row{box.x + PAD, box.y + PAD + i * ROW, box.w - 2 * PAD, ROW};
		float on = (int)i == list_index_ ? f : (int)i == list_from_ ? 1 - f : 0;
		if (on > 0) gfx::rect(row, 20, PILL.with_alpha(on));
		Color ink = mix(t.textPrimary, ON_PILL, on);
		if (list[i] == cur)
			if (TexturePtr g = icons::get(join_path(package_dir(), "glyphs/check-bold.png"), 32))
				gfx::image(*g, {row.x + 20, row.y + (ROW - CHECK) / 2, CHECK, CHECK}, 1, 0, mix(t.accent, ON_PILL, on));
		put(quick_value_name(id, list[i]), st, row.x + 20 + CHECK + 16, row.y + (ROW - 32) / 2, 32, ink);
	}
	gfx::set_alpha(1);
}

void QuickPanel::draw (const std::string &theme, double now) {
	float p = (float)shown_.at(now);
	if (p <= 0) return;
	const Theme &t = theme_named(theme);
	const auto &list = quick_settings();
	int n = (int)list.size();
	float total = n * TILE_W + (n - 1) * GAP;
	float x0 = std::round((1920 - total) / 2), y0 = 1080 - BOTTOM - TILE_H + (1 - p) * 60;
	// the bottom of the screen darkens under the strip (higher while a list is open over it)
	float lp = (float)list_shown_.at(now);
	GradientStop stops[] = {{{0, 0, 0, 0}, 0}, {{0, 0, 0, 0.7f}, 0.45f}, {{0, 0, 0, 0.88f}, 1}};
	float gh = 520 + 380 * lp;
	gfx::gradient({0, 1080 - gh, 1920, gh}, 180, stops, 3, p);
	gfx::set_alpha(p);
	TextStyle head{Face::ui, 19, 600, 19 * 0.08f};
	put(text::upper(tr("Quick settings", "Быстрые настройки")), head, x0 + 8, y0 - 46, 24, {1, 1, 1, 0.7f * (1 - lp)});
	float f = (float)focus_.at(now);
	TextStyle lst{Face::ui, 24, 500, 0}, vst{Face::ui, 21, 400, 0};
	for (int i = 0; i < n; i++) {
		const std::string &id = list[i].id;
		float on = i == index_ ? f : i == focus_from_ ? 1 - f : 0;
		float s = 1 + 0.05f * on;
		Rect box{x0 + i * (TILE_W + GAP) + TILE_W * (1 - s) / 2, y0 + TILE_H * (1 - s) / 2, TILE_W * s, TILE_H * s};
		gfx::shadow(box, 26, 10, 30, {0, 0, 0, 0.4f});
		gfx::rect(box, 26, mix(t.tileBgFocus, PILL, on));
		Color ink = mix(t.textPrimary, ON_PILL, on), sub = mix(t.textSecondary, ON_PILL_VALUE, on);
		if (TexturePtr g = icons::get(join_path(package_dir(), "glyphs/" + glyph_of(id) + ".png"), 48))
			gfx::image(*g, {box.x + 24, box.y + 22, 44, 44}, 1, 0, ink);
		put(text::ellipsize(quick_label(id), lst, (int)(box.w - 48)), lst, box.x + 24, box.y + box.h - 72, 30, ink);
		std::string value = list[i].category.empty() ? (id == "screenoff" ? tr("Sound stays on", "Звук останется") : tr("Opens the TV's menu", "Откроет меню ТВ"))
			: values_.count(id) && !values_[id].empty() ? quick_value_name(id, values_[id]) : "—";
		put(text::ellipsize(value, vst, (int)(box.w - 48)), vst, box.x + 24, box.y + box.h - 42, 26, sub);
	}
	gfx::set_alpha(1);
	draw_list(t, now);
	gfx::set_alpha(1);
}
