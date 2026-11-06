#include "ui/settings.h"

#include "data/apps.h"
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

const double LONG_PRESS_MS = 600;     // TIMING.longPressMs
const int COLUMNS = layout::GRID_COLUMNS;

// Where things are, in px of the 1080p screen (global.css .settings, .set__nav, .set__main, .options, .grid__viewport)
const float NAV_X = 140, NAV_W = 600, TOP = 84, BOTTOM = 1020;
const float ITEM_X = NAV_X + 96, ITEM_W = NAV_W - 96 - 36, ITEM_H = 70, ITEM_STEP = 76;
const float MAIN_X = NAV_X + NAV_W + 64;
const float TITLE_Y = TOP + 84, CONTENT_Y = TITLE_Y + 56 + 30;      // no line of key hints under the title
const float LIST_X = MAIN_X - 8, LIST_W = 1100, LIST_H = BOTTOM - CONTENT_Y;     // OptionList: down to the foot (the description is on the left)
const float ROW_H = 76, BOX_PAD = 6, GROUP_TITLE = 24 + 12, GROUP_GAP = 30, TRACK_PAD = 8;
const float PILL_H = 54;
const float GRID_Y = CONTENT_Y + PILL_H + 8, GRID_PAD_X = 24, GRID_PAD_Y = 20;
const float GRID_VIEW = 688;          // Grid.tsx VIEWPORT_HEIGHT
const float TILE = layout::TILE_SIZE, TILE_STEP = layout::TILE_SIZE + layout::TILE_GAP;
const float GRID_LABEL = std::round(layout::TILE_LABEL * 2.4f) + 14, GRID_ROW_STEP = layout::TILE_SIZE + GRID_LABEL + 40;
const Color PILL = Color::hex("#F2F2F7"), ON_PILL = Color::hex("#111216"), ON_PILL_VALUE = Color::hex("#3C3F47");

struct Icon { const char *id, *color; };
const Icon ICONS[] = {{"apps", "#0A84FF"}, {"inputs", "#30B0C7"}, {"look", "#5E5CE6"}, {"widgets", "#FF9F0A"}, {"wallpaper", "#BF5AF2"}, {"home", "#30D158"}};

Color mix (Color a, Color b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t}; }

// A line of text, its line box `line` px tall starting at `top`
void put (const std::string &s, const TextStyle &st, float x, float top, float line, Color c, float opacity = 1) {
	const TextLine &l = text::line(s, st);
	text::draw(s, st, std::round(x), std::round(top + (line - l.h) / 2), c, opacity);
}

TexturePtr glyph (const std::string &name, int size) { return icons::get(join_path(package_dir(), "glyphs/" + name + ".png"), size); }

std::vector<std::string> keys_of (const std::vector<TileModel> &list) {
	std::vector<std::string> out;
	for (auto &t : list) out.push_back(t.key);
	return out;
}

std::vector<std::string> move_item (std::vector<std::string> list, int from, int to) {
	std::string item = list[from];
	list.erase(list.begin() + from);
	list.insert(list.begin() + to, item);
	return list;
}

int clampi (int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); }

/** Where each row of a list sits in the track (from its top, with the track's padding), and where its group starts. */
struct ListLayout { std::vector<float> row_top, group_top; std::vector<bool> first; float height = 0; };

ListLayout list_layout (const std::vector<OptionItem> &options) {
	ListLayout l;
	float y = TRACK_PAD, group_top = y;
	for (size_t i = 0; i < options.size(); i++) {
		bool first = i == 0 || options[i].group != options[i - 1].group;
		if (first) {
			if (i > 0) y += BOX_PAD + GROUP_GAP;
			group_top = y;
			if (!options[i].group.empty()) y += GROUP_TITLE;
			y += BOX_PAD;
		}
		l.row_top.push_back(y);
		l.group_top.push_back(group_top);
		l.first.push_back(first);
		y += ROW_H;
	}
	l.height = y + BOX_PAD + TRACK_PAD;
	return l;
}

}  // namespace

void Settings::draw_peek (double now) {
	// the setting's name and its value in a bar at the top, over the home screen
	float p = (float)peek_.at(now);
	const SettingsTab *c = current();
	if (p <= 0 || !c || !c->has_options) return;
	const OptionItem *o = nullptr;
	for (auto &item : c->options) if (item.id == peek_id_) o = &item;
	if (!o) return;
	TextStyle lst{Face::ui, 27, 500, 0}, vst{Face::ui, 27, 400, 0}, big{Face::ui, 34, 400, 0};
	std::string value = o->value >= 0 ? (o->value ? tr("On", "Вкл.") : tr("Off", "Выкл.")) : o->valueText;
	float lw = (float)text::width(o->label, lst), vw = (float)text::width(value, vst), aw = o->stepper ? 2 * 34.f : 0;
	float w = std::round(lw + 36 + vw + aw + 64), h = 76;
	Rect bar{std::round((1920 - w) / 2), 56 - (1 - p) * 20, w, h};
	gfx::set_alpha(p);
	gfx::shadow(bar, h / 2, 12, 36, {0, 0, 0, 0.5f});
	gfx::rect(bar, h / 2, PILL);
	float x = bar.x + 32;
	put(o->label, lst, x, bar.y + (h - 34) / 2, 34, ON_PILL);
	x += lw + 36;
	if (o->stepper) { put("‹", big, x, bar.y, h, ON_PILL_VALUE); x += 34; }
	put(value, vst, x, bar.y + (h - 34) / 2, 34, value == tr("On", "Вкл.") && o->value >= 0 ? ON_PILL : ON_PILL_VALUE);
	x += vw;
	if (o->stepper) put("›", big, x + 34 - text::width("›", big), bar.y, h, ON_PILL_VALUE);
	gfx::set_alpha(1);
}

void Settings::open (double now) {
	tab_ = button_ = grid_ = option_ = 0;
	zone_ = Zone::nav;
	reordering_ = false;
	held_.reset();
	ok_armed_ = ok_fired_ = false;
	focus_from_ = -1;
	focus_.jump(1);
	list_.jump(0);
	grid_scroll_.jump(0);
	shift_ = 0;
	knobs_.clear();
	tiles_.clear();
	peek_.jump(0);
	peek_until_ = -1;
	(void)now;
}

std::vector<std::string> Settings::buttons () const {
	auto t = grid_tab();
	if (!t) return {};
	if (reordering_) return {"arrange"};
	if (t->refresh && on_refresh) return {"refresh", "arrange"};
	return {"arrange"};
}

const std::vector<TileModel> &Settings::grid_items () const {
	static const std::vector<TileModel> none;
	auto t = grid_tab();
	return !t ? none : reordering_ ? t->shown : t->items;
}

int Settings::count () const {
	auto c = current();
	if (!c) return 0;
	return c->has_options ? (int)c->options.size() : (int)grid_items().size();
}

int Settings::option_at () const {
	auto c = current();
	int n = c && c->has_options ? (int)c->options.size() : 0;
	return clampi(option_, 0, std::max(0, n - 1));      // a row may have gone (rows show only when they apply)
}

void Settings::focus_option (const std::string &tab, const std::string &id, double now) {
	for (size_t t = 0; t < tabs_.size(); t++) {
		if (tabs_[t].id != tab) continue;
		select_tab((int)t, now);
		for (size_t i = 0; i < tabs_[t].options.size(); i++) if (tabs_[t].options[i].id == id) { set_zone(Zone::content, now); set_option((int)i, now); }
		return;
	}
}

void Settings::set_zone (Zone z, double now) {
	if (z == zone_) return;
	zone_ = z;
	focus_from_ = -1;           // a pill in another zone: it fades in where it lands
	focus_.jump(0);
	focus_.set(1, now, motion::FOCUS_MS, Ease::ease_out);
}

void Settings::set_option (int i, double now) {
	if (i == option_at()) return;
	focus_from_ = option_at();
	focus_zone_ = Zone::content;
	option_ = i;
	focus_.jump(0);
	focus_.set(1, now, motion::FOCUS_MS, Ease::ease_out);
}

void Settings::set_grid (int i, double now) {
	if (i == grid_) return;
	grid_ = i;
	(void)now;
}

void Settings::select_tab (int i, double now) {
	if (reordering_ || i == tab_) return;            // the section is locked while re-ordering
	focus_from_ = tab_;
	focus_zone_ = Zone::nav;
	tab_ = i;
	grid_ = option_ = button_ = 0;
	focus_.jump(0);
	focus_.set(1, now, motion::FOCUS_MS, Ease::ease_out);
	list_.jump(0);
	shift_ = 0;
	grid_scroll_.jump(0);
	knobs_.clear();
	tiles_.clear();
}

void Settings::set_mode (bool next, double now) {
	auto t = grid_tab();
	if (!t || next == reordering_) return;
	const auto &items = grid_items();
	std::string focused = grid_ < (int)items.size() ? items[grid_].key : "";
	auto target = keys_of(next ? t->shown : t->items);
	auto it = std::find(target.begin(), target.end(), focused);
	grid_ = it == target.end() ? 0 : (int)(it - target.begin());
	held_.reset();
	reordering_ = next;
	button_ = 0;
	tiles_.clear();
	if (next && target.empty()) set_zone(Zone::buttons, now);
}

void Settings::move_held (int to, double now) {
	auto t = grid_tab();
	if (!t || !held_) return;
	int dest = clampi(to, 0, count() - 1);
	if (dest == held_->index) return;
	if (on_reorder) on_reorder(t->id, move_item(keys_of(t->shown), held_->index, dest));
	held_->index = dest;
	set_grid(dest, now);
}

void Settings::activate_grid (int i, double now) {
	auto t = grid_tab();
	const auto &items = grid_items();
	if (!t || i >= (int)items.size()) return;
	if (!reordering_) { if (on_toggle) on_toggle(t->id, items[i].key); return; }
	if (held_) { held_.reset(); return; }                           // put down
	held_ = Held{i, keys_of(t->shown)};                             // pick up
	set_grid(i, now);
	lift_at_ = now;
}

void Settings::press_button (const std::string &b, double now) {
	if (b == "refresh") { if (on_refresh) on_refresh(); }
	else set_mode(!reordering_, now);
}

void Settings::enter (double now) {
	if (count() > 0) set_zone(Zone::content, now);
	else if (!buttons().empty()) set_zone(Zone::buttons, now);
}

void Settings::press (double now) {
	if (zone_ == Zone::nav) { enter(now); return; }
	if (zone_ == Zone::buttons) {
		auto b = buttons();
		if (button_ < (int)b.size()) press_button(b[button_], now);
		return;
	}
	auto c = current();
	if (c && c->has_options) {
		if (c->options.empty()) return;
		const OptionItem &o = c->options[option_at()];
		if (o.stepper && o.value < 0) return;      // a value of ‹ › changes with the arrows only; OK flips switches and opens rows
		std::string id = o.id;
		if (on_option) on_option(id, Press::ok);
		start_peek(id, now);
		return;
	}
	activate_grid(grid_, now);
}

namespace {
const double PEEK_MS = 2200;          // how long the home screen stays after the last change
// The settings that show on the home screen as soon as they change
bool shows_on_home (const std::string &id) {
	static const char *ids[] = {"iconScale", "showLabels", "sideMargin", "bottomMargin", "cardSize", "plates", "glass", "frost", "shadows",
		"clockFont", "clockFormat", "clockSize", "clockWeight", "clockColor", "showDate", "dateSize", "theme", "weatherUnit"};
	for (const char *i : ids) if (id == i) return true;
	return false;
}
}  // namespace

void Settings::start_peek (const std::string &id, double now) {
	if (!shows_on_home(id)) return;
	peek_id_ = id;
	peek_until_ = now + PEEK_MS;
	peek_.set(1, now, 260, Ease::ease_out);
}

void Settings::end_peek (double now) {
	peek_until_ = -1;
	peek_.set(0, now, 260, Ease::ease_out);
}

void Settings::long_press (double now) {
	if (grid_tab() && zone_ != Zone::nav) set_mode(!reordering_, now);
}

bool Settings::key_down (int sc, bool repeat, double now) {
	if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) {
		// handled on release (press) or after a hold (long press), as useOkPress
		if (repeat || ok_armed_) return true;
		ok_armed_ = true;
		ok_fired_ = false;
		ok_down_at_ = now;
		return true;
	}
	key(sc, now);
	return true;
}

bool Settings::key_up (int sc, double now) {
	if (sc != SDL_SCANCODE_RETURN && sc != SDL_SCANCODE_KP_ENTER) return false;
	if (!ok_armed_) return false;
	ok_armed_ = false;
	if (!ok_fired_) press(now);
	return true;
}

bool Settings::tick (double now) {
	if (peek_until_ >= 0 && now >= peek_until_) { end_peek(now); return true; }
	if (ok_armed_ && !ok_fired_ && now - ok_down_at_ >= LONG_PRESS_MS) {
		ok_fired_ = true;
		long_press(now);
		return true;
	}
	return false;
}

double Settings::next_event (double now) const {
	double t = ok_armed_ && !ok_fired_ ? std::max(0.0, ok_down_at_ + LONG_PRESS_MS - now) : -1;
	if (peek_until_ >= 0) t = t < 0 ? std::max(0.0, peek_until_ - now) : std::min(t, std::max(0.0, peek_until_ - now));
	return t;
}

void Settings::key (int sc, double now) {
	bool left = sc == SDL_SCANCODE_LEFT, right = sc == SDL_SCANCODE_RIGHT, up = sc == SDL_SCANCODE_UP, down = sc == SDL_SCANCODE_DOWN;
	bool back = sc == SDL_SCANCODE_WEBOS_BACK || sc == SDL_SCANCODE_ESCAPE;
	auto t = grid_tab();
	auto c = current();
	int n = count();
	// while the home screen shows a change: ← → go on changing it; ↑ ↓ and Back bring Settings back first
	if (peek_until_ >= 0 && (up || down || back)) { end_peek(now); return; }
	if (back) {
		if (held_ && t) {
			if (on_reorder) on_reorder(t->id, held_->original);
			set_grid(held_->index, now);
			held_.reset();
		} else if (reordering_) set_mode(false, now);
		else if (zone_ != Zone::nav) set_zone(Zone::nav, now);
		else if (on_close) on_close();
		return;
	}
	if (held_) {
		if (left) move_held(held_->index - 1, now);
		else if (right) move_held(held_->index + 1, now);
		else if (up) move_held(held_->index - COLUMNS, now);
		else if (down) move_held(held_->index + COLUMNS, now);
		return;
	}
	if (zone_ == Zone::nav) {
		if (up) select_tab(clampi(tab_ - 1, 0, (int)tabs_.size() - 1), now);
		else if (down) select_tab(clampi(tab_ + 1, 0, (int)tabs_.size() - 1), now);
		else if (right) enter(now);
		return;
	}
	if (zone_ == Zone::buttons) {
		int nb = (int)buttons().size();
		if (left) { if (button_ > 0) button_--; else if (!reordering_) set_zone(Zone::nav, now); }
		else if (right) button_ = clampi(button_ + 1, 0, nb - 1);
		else if (down && n > 0) set_zone(Zone::content, now);
		return;
	}
	if (c && c->has_options) {
		int at = option_at();
		const OptionItem *o = n ? &c->options[at] : nullptr;
		if (up) set_option(clampi(at - 1, 0, n - 1), now);
		else if (down) set_option(clampi(at + 1, 0, n - 1), now);
		else if ((left || right) && o && o->stepper && o->disabled.empty()) {
			std::string id = o->id;
			if (on_option) on_option(id, left ? Press::left : Press::right);
			start_peek(id, now);
		}
		else if (left) set_zone(Zone::nav, now);
		return;
	}
	if (left) {
		if (grid_ % COLUMNS == 0) { if (!reordering_) set_zone(Zone::nav, now); }
		else set_grid(grid_ - 1, now);
	} else if (right) set_grid(clampi(grid_ + 1, 0, n - 1), now);
	else if (up) {
		if (grid_ >= COLUMNS) set_grid(grid_ - COLUMNS, now);
		else if (!buttons().empty()) set_zone(Zone::buttons, now);
	} else if (down) { if (grid_ + COLUMNS < n) set_grid(grid_ + COLUMNS, now); }
}

bool Settings::animating (double now) const {
	if (focus_.moving(now) || list_.moving(now) || grid_scroll_.moving(now) || peek_.moving(now)) return true;
	for (auto &k : knobs_) if (k.second.moving(now)) return true;
	for (auto &k : tiles_) if (k.second.moving(now)) return true;
	return now - lift_at_ < motion::FOCUS_MS;
}

// ---------- the pointer ----------

namespace {
bool inside (const Rect &r, float x, float y) { return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h; }
}

bool Settings::hit (float x, float y, Zone &zone, int &index, float &mid) const {
	for (size_t i = 0; i < tabs_.size(); i++)
		if (inside({ITEM_X, TITLE_Y + 56 + 34 + i * ITEM_STEP, ITEM_W, ITEM_H}, x, y)) { zone = Zone::nav; index = (int)i; return true; }
	for (auto &s : button_spots_) if (inside(s.r, x, y)) { zone = Zone::buttons; index = s.index; return true; }
	for (auto &s : row_spots_) if (inside(s.r, x, y)) { zone = Zone::content; index = s.index; mid = s.mid; return true; }
	for (auto &s : tile_spots_) if (inside(s.r, x, y)) { zone = Zone::content; index = s.index; return true; }
	return false;
}

void Settings::pointer (float x, float y, double now) {
	if (peek_until_ >= 0) return;          // the home screen is showing a change: the pointer leaves it be
	Zone z;
	int i = -1;
	float mid = 0;
	if (!hit(x, y, z, i, mid)) return;
	if (z == Zone::nav) {
		if (reordering_) return;
		set_zone(Zone::nav, now);
		select_tab(i, now);
	} else if (z == Zone::buttons) {
		set_zone(Zone::buttons, now);
		button_ = i;
	} else if (held_) move_held(i, now);       // a tile picked up follows the pointer
	else {
		set_zone(Zone::content, now);
		if (current() && current()->has_options) set_option(i, now);
		else set_grid(i, now);
	}
}

void Settings::click (float x, float y, double now) {
	if (peek_until_ >= 0) { end_peek(now); return; }
	Zone z;
	int i = -1;
	float mid = 0;
	if (!hit(x, y, z, i, mid)) return;
	pointer(x, y, now);
	if (z == Zone::nav) { if (!reordering_) enter(now); return; }
	if (z == Zone::buttons) { auto b = buttons(); if (i < (int)b.size()) press_button(b[i], now); return; }
	auto c = current();
	if (c && c->has_options && i < (int)c->options.size()) {
		const OptionItem &o = c->options[i];
		if (o.stepper && o.value < 0) {
			if (!o.disabled.empty()) return;
			if (on_option) on_option(o.id, x < mid ? Press::left : Press::right);
			start_peek(o.id, now);
			return;
		}
	}
	press(now);
}

void Settings::wheel (int dy, double now) {
	if (peek_until_ >= 0) return;
	if (zone_ == Zone::nav) select_tab(clampi(tab_ + (dy > 0 ? -1 : 1), 0, (int)tabs_.size() - 1), now);
	else key(dy > 0 ? SDL_SCANCODE_UP : SDL_SCANCODE_DOWN, now);
}

// ---------- drawing ----------

void Settings::draw_nav (double now) {
	const Theme &t = theme_named(theme_);
	gfx::rect({NAV_X, TOP, NAV_W, BOTTOM - TOP}, 0, {1, 1, 1, 0.035f});
	put(tr("Settings", "Настройки"), TextStyle{Face::ui, 46, 600, -0.46f}, ITEM_X + 20, TITLE_Y, 56, t.textPrimary);
	float f = (float)focus_.at(now);
	for (size_t i = 0; i < tabs_.size(); i++) {
		const SettingsTab &tab = tabs_[i];
		float y = TITLE_Y + 56 + 34 + i * ITEM_STEP;
		bool cur = (int)i == tab_;
		// the focused pill fades in on the new section and out on the one it left
		float pill = zone_ == Zone::nav && cur ? f : zone_ == Zone::nav && focus_zone_ == Zone::nav && (int)i == focus_from_ ? 1 - f : 0;
		if (zone_ != Zone::nav && cur) pill = 0;
		float opacity = reordering_ && !cur ? 0.35f : 1;
		gfx::set_alpha(opacity);
		if (cur) gfx::rect({ITEM_X, y, ITEM_W, ITEM_H}, 20, {1, 1, 1, 0.09f * (1 - pill)});
		if (pill > 0) gfx::rect({ITEM_X, y, ITEM_W, ITEM_H}, 20, PILL.with_alpha(pill));
		Color icon_color = Color::hex("#8E8E93");
		for (auto &ic : ICONS) if (tab.icon == ic.id) icon_color = Color::hex(ic.color);
		Rect icon{ITEM_X + 20, y + (ITEM_H - 44) / 2, 44, 44};
		gfx::rect(icon, 12, icon_color);
		if (TexturePtr g = glyph("section-" + tab.icon, 28)) gfx::image(*g, {icon.x + 8, icon.y + 8, 28, 28});
		TextStyle st{Face::ui, 27, 400, 0};
		Color c = mix(cur ? t.textPrimary : t.textSecondary, ON_PILL, pill);
		float lx = icon.x + 44 + 20;
		put(text::ellipsize(tab.label, st, (int)(ITEM_X + ITEM_W - 20 - lx)), st, lx, y + (ITEM_H - 34) / 2, 34, c);
		gfx::set_alpha(1);
	}
}

float Settings::list_shift (const std::vector<OptionItem> &options, int selected, float height) {
	// slide just far enough to keep the focused row (and, for the first row of a group, its heading) in view
	if (options.empty()) return 0;
	ListLayout l = list_layout(options);
	int i = clampi(selected, 0, (int)options.size() - 1);
	float top = (l.first[i] ? l.group_top[i] : l.row_top[i]) - 8;
	float bottom = l.row_top[i] + ROW_H + 16;
	double next = shift_;
	if (top < next) next = std::max(0.f, top);
	else if (bottom > next + height) next = bottom - height;
	return (float)next;
}

void Settings::draw_options (double now, float top) {
	const Theme &t = theme_named(theme_);
	const SettingsTab *c = current();
	const auto &options = c->options;
	int sel = option_at();
	double next = list_shift(options, sel, LIST_H);
	if (next != shift_) { shift_ = next; list_.set(-next, now, motion::SCROLL_MS, Ease::scroll); }
	float dy = (float)list_.at(now);
	ListLayout l = list_layout(options);
	float x0 = LIST_X + TRACK_PAD, w = LIST_W - 2 * TRACK_PAD;
	float f = (float)focus_.at(now);
	gfx::push_clip({LIST_X, top, LIST_W, LIST_H});
	row_spots_.clear();
	for (size_t i = 0; i < options.size(); i++) {
		const OptionItem &o = options[i];
		float ry = top + dy + l.row_top[i];
		bool first = l.first[i];
		size_t last = i;
		while (last + 1 < options.size() && !l.first[last + 1]) last++;
		if (first) {
			float gy = top + dy + l.group_top[i];
			float box_top = gy + (o.group.empty() ? 0 : GROUP_TITLE);
			float box_h = l.row_top[last] + ROW_H + BOX_PAD - (l.group_top[i] + (o.group.empty() ? 0 : GROUP_TITLE));
			if (!o.group.empty()) {
				TextStyle gst{Face::ui, 19, 600, 19 * 0.08f};
				put(text::upper(o.group), gst, x0 + 26, gy, 24, t.textMuted);
			}
			gfx::rect({x0, box_top, w, box_h}, 28, {1, 1, 1, 0.075f});
		}
		if (ry > top + LIST_H || ry + ROW_H < top) continue;
		bool focused = zone_ == Zone::content && (int)i == sel;
		float pill = focused ? f : zone_ == Zone::content && focus_zone_ == Zone::content && (int)i == focus_from_ ? 1 - f : 0;
		Rect row{x0 + BOX_PAD, ry, w - 2 * BOX_PAD, ROW_H};
		// hairlines between the rows of a group, hidden around the focused one
		if (!first) {
			float prev_pill = zone_ == Zone::content && ((int)i - 1 == sel || (focus_zone_ == Zone::content && (int)i - 1 == focus_from_)) ? 1 : 0;
			float line = 1 - std::max(pill > 0 ? 1.f : 0.f, prev_pill);
			if (line > 0) gfx::rect({row.x + 24, ry, row.w - 48, 1}, 0, {1, 1, 1, 0.09f * line});
		}
		if (pill > 0) gfx::rect(row, 22, PILL.with_alpha(pill));
		float dim = o.disabled.empty() ? 1 : 0.4f;
		TextStyle lst{Face::ui, 27, 400, 0}, vst{Face::ui, 25, 400, 0};
		Color label_c = mix(t.textPrimary, ON_PILL, pill), value_c = mix(t.textSecondary, ON_PILL_VALUE, pill);
		float right = row.x + row.w - 24, value_left = right;
		if (o.value >= 0) {
			// an iOS-style switch, its knob sliding over
			if (!knobs_.count(o.id)) knobs_[o.id].jump(o.value);
			Tween &k = knobs_[o.id];
			k.set(o.value, now, motion::FOCUS_MS, Ease::ease_out);
			float on = (float)k.at(now);
			Rect sw{right - 74, ry + (ROW_H - 42) / 2, 74, 42};
			Color track = mix(Color{1, 1, 1, 0.22f}, Color{17 / 255.f, 18 / 255.f, 22 / 255.f, 0.18f}, pill);
			gfx::set_alpha(dim);
			gfx::rect(sw, 21, mix(track, t.accent, on));
			Rect knob{sw.x + 4 + 32 * on, sw.y + 4, 34, 34};
			gfx::shadow(knob, 17, 2, 6, {0, 0, 0, 0.3f});
			gfx::rect(knob, 17, {1, 1, 1, 1});
			gfx::set_alpha(1);
			value_left = sw.x;
		} else {
			// the value: ‹ text › (the arrows show on the focused row), and a chevron for a row that opens another screen
			float x = right;
			TextStyle big{Face::ui, 34, 400, 0};
			if (!o.icon.empty())
				if (TexturePtr g = glyph(o.icon, 32)) { x -= 30; gfx::image(*g, {x, ry + (ROW_H - 30) / 2, 30, 30}, dim, 0, label_c); x -= 14; }
			if (o.opens) {
				const TextLine &g = text::line("›", big);
				x -= g.w;
				put("›", big, x, ry, ROW_H, value_c, 0.5f * dim);
				x -= 14;
			}
			if (o.stepper) {
				x -= 30;
				const TextLine &g = text::line("›", big);
				put("›", big, x + (30 - g.w) / 2, ry, ROW_H, value_c, 0.85f * pill * dim);
			}
			int max_text = 560 - (o.stepper ? 60 : 0) - (o.opens ? 40 : 0);
			std::string v = text::ellipsize(o.valueText, vst, max_text);
			const TextLine &vl = text::line(v, vst);
			x -= vl.w;
			put(v, vst, x, ry + (ROW_H - 32) / 2, 32, value_c, dim);
			if (o.stepper) {
				x -= 30;
				const TextLine &g = text::line("‹", big);
				put("‹", big, x + (30 - g.w) / 2, ry, ROW_H, value_c, 0.85f * pill * dim);
			}
			value_left = x;
		}
		float lx = row.x + 24;
		float label_top = ry + (ROW_H - 34) / 2;
		if (o.progress >= 0) {
			// a download: the label goes up a little for the bar under it
			label_top -= 8;
			Rect bar{lx, ry + ROW_H - 22, value_left - 30 - lx, 6};
			gfx::rect(bar, 3, mix(Color{1, 1, 1, 0.16f}, Color{17 / 255.f, 18 / 255.f, 22 / 255.f, 0.16f}, pill));
			if (o.progress > 0) gfx::rect({bar.x, bar.y, std::max(6.f, bar.w * std::min(1.f, o.progress)), bar.h}, 3, t.accent);
		}
		put(text::ellipsize(o.label, lst, (int)(value_left - 30 - lx)), lst, lx, label_top, 34, label_c, dim);
		// the part of the row in view, for the pointer
		float vt = std::max(ry, top), vb = std::min(ry + ROW_H, top + LIST_H);
		if (vb > vt) row_spots_.push_back({{row.x, vt, row.w, vb - vt}, (int)i, o.stepper ? (value_left + right) / 2 : 0});
	}
	gfx::pop_clip();
}

void Settings::draw_grid (double now, float top) {
	const Theme &t = theme_named(theme_);
	const SettingsTab *tab = grid_tab();
	const auto &items = grid_items();
	auto checked = keys_of(tab->shown);
	int n = (int)items.size();
	int rows = (n + COLUMNS - 1) / COLUMNS;
	float total = rows * GRID_ROW_STEP - 40;
	int row = grid_ / COLUMNS;
	float offset = total <= GRID_VIEW ? 0 : std::max(0.f, std::min(row * GRID_ROW_STEP - (GRID_VIEW - GRID_ROW_STEP) / 2, total - GRID_VIEW));
	grid_scroll_.set(-offset, now, motion::SCROLL_MS, Ease::scroll);
	float dy = (float)grid_scroll_.at(now);
	float x0 = MAIN_X + GRID_PAD_X, y0 = top + GRID_PAD_Y + dy;
	gfx::push_clip({MAIN_X - 40, top, 1920 - MAIN_X + 40, 1080 - top});
	int focused_i = -1;
	tile_spots_.clear();
	auto draw_one = [&] (int i) {
		const TileModel &m = items[i];
		float x = x0 + (i % COLUMNS) * TILE_STEP, y = y0 + (i / COLUMNS) * GRID_ROW_STEP;
		if (y > 1080 || y + GRID_ROW_STEP < top) return;
		if (y + TILE > top) tile_spots_.push_back({{x, std::max(y, top), TILE, y + TILE - std::max(y, top)}, i});
		bool focused = zone_ == Zone::content && i == grid_;
		bool lifted = held_ && held_->index == i;
		if (!tiles_.count(m.key)) tiles_[m.key].jump(focused ? 1 : 0);
		Tween &tw = tiles_[m.key];
		tw.set(focused ? 1 : 0, now, motion::FOCUS_MS, Ease::ease_out);
		float f = (float)tw.at(now);
		bool is_checked = !reordering_ && std::find(checked.begin(), checked.end(), m.key) != checked.end();
		bool dimmed = !reordering_ && !is_checked;
		// the frame grows from its middle (.tile--focused .tile__frame), more and a little up when picked up
		float s = lifted ? (float)motion::LIFT_SCALE : 1 + (float)(motion::FOCUS_SCALE - 1) * f;
		float w = TILE * s;
		Rect frame{x + (TILE - w) / 2, y + (TILE - w) / 2 - (lifted ? 6 : 0), w, w};
		float r = layout::TILE_RADIUS * s;
		if (lifted) gfx::shadow(frame, r, 18, 40, t.liftShadow);
		gfx::set_alpha(dimmed ? 0.45f : 1);
		gfx::ring(frame, r, 2 * s, t.tileBorder);
		draw_tile_art(m, frame, r, s, (int)TILE, 100, t, f > 0.5f || lifted ? t.tileBgFocus : t.tileBg);
		if (is_checked) {
			// the tick badge in the top right corner
			Rect badge{frame.x + frame.w - (12 + 34) * s, frame.y + 12 * s, 34 * s, 34 * s};
			gfx::rect(badge, badge.w / 2, t.badge);
			if (TexturePtr g = glyph("check-bold", 24)) gfx::image(*g, {badge.x + 7 * s, badge.y + 7 * s, 20 * s, 20 * s}, 1, 0, t.badgeText);
		}
		gfx::set_alpha(1);
		float ring = lifted ? 1 : f;
		if (ring > 0) gfx::ring(frame, r, 5 * s, t.tileBorderFocus.with_alpha(ring));
		// the name, up to two lines (not scaled)
		TextStyle st{Face::ui, layout::TILE_LABEL, 400, 0};
		float lw = TILE + layout::TILE_GAP - 8, line_h = std::round(layout::TILE_LABEL * 1.2f);
		Color c = dimmed ? t.textMuted : mix(t.textSecondary, t.textPrimary, lifted ? 1 : f);
		auto lines = text::wrap(m.label, st, (int)lw, 2);
		for (size_t l = 0; l < lines.size(); l++) {
			const TextLine &tl = text::line(lines[l], st);
			put(lines[l], st, x + TILE / 2 - tl.w / 2.f, y + TILE + 14 + l * line_h, line_h, c);
		}
	};
	for (int i = 0; i < n; i++) {
		if (i == grid_ && zone_ == Zone::content) { focused_i = i; continue; }
		draw_one(i);
	}
	if (focused_i >= 0) draw_one(focused_i);     // over its neighbours, as it grows
	gfx::pop_clip();
}

void Settings::draw_footer () {
	// the focused row's description on the left, under the sections
	const Theme &t = theme_named(theme_);
	const SettingsTab *c = current();
	if (!c || !c->has_options || zone_ != Zone::content || c->options.empty()) return;
	const OptionItem &o = c->options[option_at()];
	std::string text = o.disabled.empty() ? o.description : o.disabled;
	if (text.empty()) return;
	TextStyle st{Face::ui, 23, 400, 0};
	float x = ITEM_X + 20, w = ITEM_W - 40;
	float y = TITLE_Y + 56 + 34 + tabs_.size() * ITEM_STEP + 48;
	int max_lines = std::max(1, (int)((BOTTOM - 40 - y) / 33));
	gfx::rect({x, y - 24, w, 1}, 0, {1, 1, 1, 0.09f});
	for (auto &l : text::wrap(text, st, (int)w, max_lines)) { put(l, st, x, y, 33, o.disabled.empty() ? t.textSecondary : t.danger); y += 33; }
}

void Settings::draw (double now) {
	const Theme &t = theme_named(theme_);
	gfx::rect({0, 0, 1920, 1080}, 0, t.screen.with_alpha(t.screen.a * SCREEN_OPACITY));      // the wallpaper a little through it
	draw_nav(now);
	const SettingsTab *c = current();
	if (!c) return;
	put(c->label, TextStyle{Face::ui, 46, 600, -0.46f}, MAIN_X, TITLE_Y, 56, t.textPrimary);
	if (c->has_options) { draw_options(now, CONTENT_Y); button_spots_.clear(); tile_spots_.clear(); }
	else {
		row_spots_.clear();
		// the buttons above the grid
		float x = MAIN_X;
		auto b = buttons();
		TextStyle st{Face::ui, 23, 500, 0};
		button_spots_.clear();
		for (size_t i = 0; i < b.size(); i++) {
			std::string label = b[i] == "refresh" ? tr("Refresh list", "Обновить список") : reordering_ ? tr("Done", "Готово") : c->arrange;
			const TextLine &l = text::line(label, st);
			float w = std::round(l.w + 60.f);
			bool focused = zone_ == Zone::buttons && (int)i == button_;
			Color bg = focused ? PILL : b[i] == "arrange" && reordering_ ? Color{48 / 255.f, 209 / 255.f, 88 / 255.f, 0.28f} : Color{1, 1, 1, 0.09f};
			gfx::rect({x, CONTENT_Y, w, PILL_H}, PILL_H / 2, bg);
			button_spots_.push_back({{x, CONTENT_Y, w, PILL_H}, (int)i});
			put(label, st, x + 30, CONTENT_Y + 12, 30, focused ? ON_PILL : t.textPrimary);
			x += w + 14;
		}
		if (reordering_ && count() == 0)
			put(tr("Nothing on the home screen yet.", "На главном экране пока ничего нет."), TextStyle{Face::ui, 24, 400, 0}, MAIN_X, GRID_Y, 32, t.textSecondary);
		else draw_grid(now, GRID_Y);
	}
	draw_footer();
}
