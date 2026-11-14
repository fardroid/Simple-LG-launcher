#include "ui/home_method.h"

#include "data/apps.h"
#include "gfx/text.h"
#include "i18n/i18n.h"
#include "sys/fs.h"
#include "ui/anim.h"
#include "ui/icons.h"
#include "ui/theme.h"
#include "ui/tokens.h"

#include <SDL.h>

#include <algorithm>
#include <cmath>

namespace {

// global.css .screen--picker .homemethod: 1600 px in the middle; the list 760 px with rows 96 px, the panel beside it
const float X = (1920 - 1600) / 2.f, W = 1600, TOP = 84, LIST_W = 760, ROW_H = 96, PAD = 6;
const Color PILL = Color::hex("#F2F2F7"), ON_PILL = Color::hex("#111216"), ON_PILL_DETAIL = Color::hex("#3C3F47");
const Color GREEN = Color::hex("#30D158"), GREEN_ON_PILL = Color::hex("#1F8A3B"), ORANGE = Color::hex("#FF9F0A");

void put (const std::string &s, const TextStyle &st, float x, float top, float line, Color c, float opacity = 1) {
	const TextLine &l = text::line(s, st);
	text::draw(s, st, std::round(x), std::round(top + (line - l.h) / 2), c, opacity);
}

Color mix (Color a, Color b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t}; }

int index_of (const std::string &mode) {
	for (size_t i = 0; i < HOME_MODES.size(); i++) if (HOME_MODES[i] == mode) return (int)i;
	return 0;
}

}  // namespace

void HomeMethod::open (double now) {
	info_.reset();
	reading_ = true;
	busy_.clear();
	armed_.clear();
	index_ = 0;
	focus_at_ = now;
}

void HomeMethod::set_info (const std::optional<HomeModeInfo> &info, bool reading) {
	bool changed = (info ? info->mode : "") != (info_ ? info_->mode : "");
	info_ = info;
	reading_ = reading;
	if (info && changed) { index_ = index_of(info->mode); armed_.clear(); }
}

void HomeMethod::activate (int i) {
	const std::string &mode = HOME_MODES[i];
	if (!busy_.empty() || !info_ || mode == info_->mode || mode_note(mode, info_).blocked) return;
	if (armed_ != mode) { armed_ = mode; return; }
	armed_.clear();
	if (on_pick) on_pick(mode);
}

bool HomeMethod::key_down (int sc, double now) {
	int before = index_;
	if (sc == SDL_SCANCODE_UP) index_ = std::max(0, index_ - 1);
	else if (sc == SDL_SCANCODE_DOWN) index_ = std::min((int)HOME_MODES.size() - 1, index_ + 1);
	else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) activate(index_);
	else if (sc == SDL_SCANCODE_WEBOS_BACK || sc == SDL_SCANCODE_ESCAPE) { if (busy_.empty() && on_close) on_close(); }
	if (index_ != before) { focus_at_ = now; armed_.clear(); }
	return true;
}

// the rows as draw lays them out
int HomeMethod::row_at (float x, float y) const {
	float y0 = TOP + 66 + 28 + 40 + PAD;
	if (x < X + PAD || x >= X + LIST_W - PAD || y < y0) return -1;
	int i = (int)((y - y0) / ROW_H);
	return i < (int)HOME_MODES.size() ? i : -1;
}

void HomeMethod::pointer (float x, float y, double now) {
	int i = row_at(x, y);
	if (i < 0 || i == index_) return;
	index_ = i;
	focus_at_ = now;
	armed_.clear();
}

void HomeMethod::click (float x, float y, double now) {
	int i = row_at(x, y);
	if (i < 0) return;
	pointer(x, y, now);
	activate(i);
}

bool HomeMethod::animating (double now) const { return now - focus_at_ < motion::FOCUS_MS; }

void HomeMethod::draw (double now) {
	const Theme &t = theme_named(theme_);
	gfx::rect({0, 0, 1920, 1080}, 0, t.screen.with_alpha(t.screen.a * SCREEN_OPACITY));      // the wallpaper a little through it
	put(tr("How the launcher opens", "Как открывается лаунчер"), TextStyle{Face::ui, 46, 600, -0.46f}, X, TOP, 56, t.textPrimary);
	put(tr("What the Home button and Back out of the last app open. Switching the TV on shows what was on before. One way at a time.", "Что открывают кнопка Home и выход из последнего приложения. При включении ТВ показывается то, что было до выключения. Работает один способ за раз."),
		TextStyle{Face::ui, 21, 400, 0}, X, TOP + 66, 28, t.textMuted);
	float y0 = TOP + 66 + 28 + 40;
	std::string current = info_ ? info_->mode : "";
	// the list of ways
	gfx::rect({X, y0, LIST_W, HOME_MODES.size() * ROW_H + 2 * PAD}, 28, {1, 1, 1, 0.075f});
	float f = (float)ease(Ease::ease_out, std::min(1.0, (now - focus_at_) / motion::FOCUS_MS));
	TextStyle lst{Face::ui, 26, 400, 0}, dst{Face::ui, 20, 400, 0};
	for (size_t i = 0; i < HOME_MODES.size(); i++) {
		const std::string &mode = HOME_MODES[i];
		ModeNote note = mode_note(mode, info_);
		Rect row{X + PAD, y0 + PAD + i * ROW_H, LIST_W - 2 * PAD, ROW_H};
		bool focused = (int)i == index_;
		float pill = focused ? f : 0;
		if (i > 0 && !focused && (int)i - 1 != index_) gfx::rect({row.x + 92, row.y, row.w - 92 - 24, 1}, 0, {1, 1, 1, 0.09f});
		gfx::set_alpha(note.blocked ? 0.5f : 1);
		if (pill > 0) gfx::rect(row, 22, PILL.with_alpha(pill));
		if (mode == current)
			if (TexturePtr g = icons::get(join_path(package_dir(), "glyphs/check.png"), 40)) gfx::image(*g, {row.x + 28, row.y + (ROW_H - 40) / 2, 40, 40}, 1, 0, mix(GREEN, GREEN_ON_PILL, pill));
		float tx = row.x + 28 + 40 + 24, tw = row.x + row.w - 28 - tx;
		std::string detail = mode == current ? tr("On now", "Включено сейчас") : !note.text.empty() ? note.text : busy_ == mode ? tr("Switching…", "Переключаю…") : "";
		float h = 34 + (detail.empty() ? 0 : 4 + 26), ty = row.y + (ROW_H - h) / 2;
		put(text::ellipsize(mode_text(mode).name, lst, (int)tw), lst, tx, ty, 34, mix(t.textPrimary, ON_PILL, pill));
		if (!detail.empty()) put(text::ellipsize(detail, dst, (int)tw), dst, tx, ty + 38, 26, mix(t.textSecondary, ON_PILL_DETAIL, pill));
		gfx::set_alpha(1);
	}
	// the panel: how the focused way works, its pros and cons, and what OK does now
	const std::string &focused_mode = HOME_MODES[index_];
	ModeText text = mode_text(focused_mode);
	ModeNote note = mode_note(focused_mode, info_);
	float px = X + LIST_W + 40, pw = W - LIST_W - 40, inner = pw - 80;
	TextStyle how{Face::ui, 25, 400, 0}, li{Face::ui, 22, 400, 0}, mark{Face::ui, 26, 700, 0}, act{Face::ui, 22, 600, 0};
	auto how_lines = text::wrap(text.how, how, (int)inner, 6);
	float h = 34 + how_lines.size() * 36.f + 24;
	std::vector<std::pair<std::vector<std::string>, bool>> items;
	for (auto &p : text.pros) items.push_back({text::wrap(p, li, (int)inner - 40, 4), true});
	for (auto &c : text.cons) items.push_back({text::wrap(c, li, (int)inner - 40, 4), false});
	for (auto &it : items) h += it.first.size() * 31.f + 12;
	h += 18 * 2;
	std::string action = tr("OK switches to this way", "OK — включить этот способ");
	if (!info_) action = reading_ ? tr("Reading the TV's settings…", "Читаю настройки ТВ…") : tr("Could not read the TV's settings", "Не удалось прочитать настройки ТВ");
	else if (!busy_.empty()) action = busy_ == focused_mode ? tr("Switching… The screen may blink.", "Переключаю… Экран может мигнуть.") : tr("Switching…", "Переключаю…");
	else if (focused_mode == current) action = tr("This way is on now", "Этот способ включён сейчас");
	else if (note.blocked) action = note.text;
	else if (armed_ == focused_mode) action = tr("Press OK again to switch", "Нажмите OK ещё раз, чтобы переключить");
	auto action_lines = text::wrap(action, act, (int)inner, 2);
	h += 26 + 22 + action_lines.size() * 28.f + 30;
	gfx::rect({px, y0, pw, h}, 28, {1, 1, 1, 0.075f});
	float x = px + 40, y = y0 + 34;
	for (auto &l : how_lines) { put(l, how, x, y, 36, t.textPrimary); y += 36; }
	y += 24;
	for (size_t k = 0; k < items.size(); k++) {
		if (k == text.pros.size()) y += 6;
		put(items[k].second ? "+" : "−", mark, x + 4, y, 31, items[k].second ? GREEN : ORANGE);
		for (auto &l : items[k].first) { put(l, li, x + 40, y, 31, t.textSecondary); y += 31; }
		y += 12;
	}
	y += 18;
	gfx::rect({x, y + 26, inner, 1}, 0, {1, 1, 1, 0.09f});
	y += 26 + 22;
	for (auto &l : action_lines) { put(l, act, x, y, 28, armed_ == focused_mode ? t.textPrimary : t.textMuted); y += 28; }
}
