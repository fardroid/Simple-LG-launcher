#include "ui/app_menu.h"

#include "gfx/text.h"
#include "i18n/i18n.h"
#include "ui/anim.h"
#include "ui/theme.h"
#include "ui/tokens.h"

#include <SDL.h>

#include <algorithm>
#include <cmath>

namespace {

const Color PILL = Color::hex("#F2F2F7"), ON_PILL = Color::hex("#111216"), ON_PILL_DETAIL = Color::hex("#3C3F47");

void put (const std::string &s, const TextStyle &st, float x, float top, float line, Color c, float opacity = 1) {
	const TextLine &l = text::line(s, st);
	text::draw(s, st, std::round(x), std::round(top + (line - l.h) / 2), c, opacity);
}

Color mix (Color a, Color b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t}; }

}  // namespace

void AppMenu::open (const std::string &title, Moves moves, const std::string &hide_detail, bool app, const std::string &blocked, double now) {
	open_ = true;
	title_ = title;
	moves_ = moves;
	hide_detail_ = hide_detail;
	app_ = app;
	blocked_ = blocked;
	step_ = Step::menu;
	confirm_only_ = false;
	q_fn_ = nullptr;
	w_text_.clear();
	ok_armed_ = false;
	index_ = first_enabled();
	focus_at_ = now;
}

void AppMenu::open_confirm (const std::string &title, double now) {
	open(title, Moves{}, "", true, "", now);
	step_ = Step::confirm;
	index_ = 0;          // on Cancel
	confirm_only_ = true;
}

void AppMenu::open_question (const std::string &title, const std::string &text, const std::string &yes, std::function<void()> fn, double now) {
	open_confirm(title, now);
	q_title_ = title;
	q_text_ = text;
	q_yes_ = yes;
	q_fn_ = fn;
}

void AppMenu::open_working (const std::string &title, const std::string &text, double now) {
	open(title, Moves{}, "", false, "", now);
	step_ = Step::working;
	w_text_ = text;
}

std::vector<AppMenu::Row> AppMenu::rows () const {
	if (step_ == Step::working) return {};
	if (step_ == Step::confirm && q_fn_) return {{"cancel", tr("Cancel", "Отмена"), ""}, {"yes", q_yes_, ""}};
	if (step_ == Step::confirm)
		return {{"cancel", tr("Cancel", "Отмена"), ""}, {"yes", fmt(tr("Uninstall {0}", "Удалить {0}"), title_), tr("This cannot be undone", "Это нельзя отменить")}};
	std::string first = tr("Already first", "Уже первое"), last = tr("Already last", "Уже последнее");
	std::vector<Row> out = {
		{"start", tr("Move to start", "В начало"), moves_.start ? "" : first, !moves_.start},
		{"left", tr("Move left", "Сдвинуть влево"), moves_.left ? "" : first, !moves_.left},
		{"right", tr("Move right", "Сдвинуть вправо"), moves_.right ? "" : last, !moves_.right},
		{"end", tr("Move to end", "В конец"), moves_.end ? "" : last, !moves_.end},
		{"hide", tr("Hide from the home screen", "Скрыть с главного экрана"), hide_detail_, false}
	};
	if (app_) out.push_back({"uninstall", tr("Uninstall", "Удалить"), blocked_.empty() ? tr("Removes the app from the TV", "Удаляет приложение с ТВ") : blocked_, !blocked_.empty()});
	out.push_back({"cancel", tr("Cancel", "Отмена"), ""});
	return out;
}

int AppMenu::first_enabled () const {
	auto r = rows();
	for (size_t i = 0; i < r.size(); i++) if (!r[i].disabled) return (int)i;
	return 0;
}

void AppMenu::run (const Row &row) {
	if (row.disabled) return;
	if (row.key == "start" && on_move) on_move(MoveAction::start);
	else if (row.key == "left" && on_move) on_move(MoveAction::left);
	else if (row.key == "right" && on_move) on_move(MoveAction::right);
	else if (row.key == "end" && on_move) on_move(MoveAction::end);
	else if (row.key == "hide" && on_hide) on_hide();
	else if (row.key == "uninstall") { step_ = Step::confirm; index_ = 0; }
	else if (row.key == "yes" && q_fn_) { auto fn = q_fn_; open_ = false; fn(); }
	else if (row.key == "yes") { step_ = Step::working; if (on_uninstall) on_uninstall(); }
	else if (row.key == "cancel" && on_close) on_close();
}

bool AppMenu::key_down (int sc, bool repeat, double now) {
	if (step_ == Step::working) return true;
	int n = (int)rows().size(), before = index_;
	if (sc == SDL_SCANCODE_UP) index_ = std::max(0, index_ - 1);
	else if (sc == SDL_SCANCODE_DOWN) index_ = std::min(n - 1, index_ + 1);
	else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) { if (!repeat) ok_armed_ = true; }
	else if (sc == SDL_SCANCODE_WEBOS_BACK || sc == SDL_SCANCODE_ESCAPE) {
		if (step_ == Step::confirm && !confirm_only_) { step_ = Step::menu; index_ = first_enabled(); }
		else if (on_close) on_close();
	}
	if (index_ != before) focus_at_ = now;
	return true;
}

bool AppMenu::key_up (int sc, double now) {
	(void)now;
	if (sc != SDL_SCANCODE_RETURN && sc != SDL_SCANCODE_KP_ENTER) return false;
	if (!ok_armed_ || step_ == Step::working) return false;
	ok_armed_ = false;
	auto r = rows();
	if (index_ < (int)r.size()) run(r[index_]);
	return true;
}

int AppMenu::row_at (float x, float y) const {
	if (x < rows_x_ || x > rows_x_ + rows_w_) return -1;
	for (size_t i = 0; i < row_spans_.size(); i++) if (y >= row_spans_[i].first && y < row_spans_[i].second) return (int)i;
	return -1;
}

void AppMenu::pointer (float x, float y, double now) {
	if (step_ == Step::working) return;
	int i = row_at(x, y);
	if (i < 0 || i == index_) return;
	index_ = i;
	focus_at_ = now;
}

void AppMenu::click (float x, float y, double now) {
	if (step_ == Step::working) return;
	int i = row_at(x, y);
	if (i < 0) return;
	pointer(x, y, now);
	auto r = rows();
	if (i < (int)r.size()) run(r[i]);
}

bool AppMenu::animating (double now) const { return now - focus_at_ < motion::FOCUS_MS; }

void AppMenu::draw (const std::string &theme, double now) {
	if (!open_) return;
	const Theme &t = theme_named(theme);
	gfx::rect({0, 0, 1920, 1080}, 0, {0, 0, 0, 0.72f});
	// global.css .dialog: 900 px wide, in the middle
	auto r = rows();
	bool own = step_ == Step::working && !w_text_.empty();
	std::string title = own ? title_ : q_fn_ ? q_title_ : step_ == Step::confirm ? fmt(tr("Uninstall {0}?", "Удалить {0}?"), title_) : step_ == Step::working ? fmt(tr("Uninstalling {0}…", "Удаляется {0}…"), title_) : title_;
	std::string text = own ? w_text_ : q_fn_ ? q_text_ : step_ == Step::confirm ? tr("The app will be removed from the TV.", "Приложение будет удалено с ТВ.")
		: step_ == Step::working ? tr("Please wait, this can take a few seconds.", "Подождите, это может занять несколько секунд.") : "";
	TextStyle tst{Face::ui, 40, 600, 0}, xst{Face::ui, 20, 400, 0}, lst{Face::ui, 26, 400, 0}, dst{Face::ui, 20, 400, 0};
	float w = 900, inner = w - 88;
	auto text_lines = text.empty() ? std::vector<std::string>() : text::wrap(text, xst, (int)inner, 3);
	float h = 40 + 48 + 8 + (text_lines.empty() ? 0 : text_lines.size() * 28.f + 22) + r.size() * (84.f + 8) + 32;
	Rect box{(1920 - w) / 2, std::round((1080 - h) / 2), w, std::round(h)};
	gfx::shadow(box, 36, 30, 80, {0, 0, 0, 0.6f});
	gfx::rect(box, 36, Color::hex("#1C1C20"));
	float x = box.x + 44, y = box.y + 40;
	put(text::ellipsize(title, tst, (int)inner), tst, x, y, 48, t.textPrimary);
	y += 48 + 8;
	for (auto &l : text_lines) { put(l, xst, x, y, 28, t.textSecondary); y += 28; }
	if (!text_lines.empty()) y += 22;
	float f = (float)ease(Ease::ease_out, std::min(1.0, (now - focus_at_) / motion::FOCUS_MS));
	row_spans_.clear();
	rows_x_ = x;
	rows_w_ = inner;
	for (size_t i = 0; i < r.size(); i++) {
		y += 8;
		Rect row{x, y, inner, 84};
		row_spans_.push_back({y, y + 84});
		bool focused = (int)i == index_;
		float pill = focused ? f : 0;
		float opacity = r[i].disabled ? 0.5f : 1;
		gfx::set_alpha(opacity);
		gfx::rect(row, 22, mix(Color{1, 1, 1, 0.075f}, PILL, pill));
		float th = 34 + (r[i].detail.empty() ? 0 : 4 + 26), ty = y + (84 - th) / 2;
		put(text::ellipsize(r[i].label, lst, (int)inner - 56), lst, x + 28, ty, 34, mix(t.textPrimary, ON_PILL, pill));
		if (!r[i].detail.empty()) put(text::ellipsize(r[i].detail, dst, (int)inner - 56), dst, x + 28, ty + 38, 26, mix(t.textSecondary, ON_PILL_DETAIL, pill));
		gfx::set_alpha(1);
		y += 84;
	}
}
