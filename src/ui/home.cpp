#include "ui/home.h"

#include "data/apps.h"
#include "gfx/text.h"
#include "i18n/i18n.h"
#include "sys/fs.h"
#include "ui/card.h"
#include "ui/clock.h"
#include "ui/weather_card.h"
#include "ui/calendar_card.h"

#include <chrono>
#include "ui/icons.h"
#include "ui/theme.h"
#include "ui/tile.h"

#include <SDL.h>

#include <algorithm>
#include <cmath>

namespace {
const double LONG_PRESS_MS = 600;       // TIMING.longPressMs
const float LIFT_PX = 24;               // how much higher a tile being moved stands
const double FAST_STEP_MS = 280;          // steps closer than this glide (a held arrow key)
const float VIEW_PAD_TOP = 230, VIEW_PAD_SIDE = 140, VIEW_PAD_BOTTOM = 130;    // .row__viewport: room for the glow
}  // namespace

int Home::tile_size () const { return (int)std::lround(layout::TILE_SIZE * cfg_.iconScale / 100.0); }
int Home::tile_gap () const { return (int)std::lround(layout::TILE_GAP * cfg_.iconScale / 100.0); }
int Home::label_font () const { return std::min(30, std::max(16, (int)std::lround(layout::TILE_LABEL * cfg_.iconScale / 100.0))); }
float Home::label_area () const { return cfg_.showLabels ? label_font() * 2.4f + 14 : 0; }

std::vector<int> Home::sizes () const { return split_rows((int)items_.size(), cfg_.rows); }

float Home::row_top (int r) const {
	auto s = sizes();
	float shift = (float)shift_y_;
	float row_h = tile_size() + label_area();
	float bottom = 1080.f - cfg_.bottomMargin;
	int n = (int)s.size();
	float total = n * row_h + (n - 1) * 14.f;
	return bottom - total + r * (row_h + 14.f) + shift;
}

void Home::set_items (const std::vector<TileModel> &items) {
	if (lifting_) { lifting_ = false; lift_.jump(0); }        // the list changed under a tile being moved: it stays where it was
	std::string selected;
	auto s = sizes();
	auto at = clamp_position(s, pos_);
	auto off = row_offsets(s);
	if (!items_.empty() && at.row < (int)off.size()) {
		int i = off[at.row] + at.cols[at.row];
		if (i < (int)items_.size()) selected = items_[i].key;
	}
	items_ = items;
	anim_.assign(items_.size(), TileAnim());
	// the highlight stays on the tile it was on (the list came again, a tile was added or removed)
	if (!selected.empty()) {
		for (size_t i = 0; i < items_.size(); i++) {
			if (items_[i].key != selected) continue;
			auto ns = sizes();
			auto no = row_offsets(ns);
			int row = 0;
			while (row < (int)ns.size() - 1 && (int)i >= no[row] + ns[row]) row++;
			pos_.row = row;
			pos_.cols.resize(2, 0);
			pos_.cols[row] = (int)i - no[row];
		}
	}
	pos_ = clamp_position(sizes(), pos_);
	for (int r = 0; r < 2; r++) {
		if (r < (int)pos_.cols.size()) rows_[r].anchor = pos_.cols[r];
		rows_[r].offset.jump(target_offset(r, rows_[r].anchor));
	}
	auto ns = sizes();
	auto no = row_offsets(ns);
	for (size_t i = 0; i < items_.size(); i++) {
		int row = 0;
		while (row < (int)ns.size() - 1 && (int)i >= no[row] + ns[row]) row++;
		bool on = row == pos_.row && (int)i - no[row] == pos_.cols[row];
		anim_[i].focus.jump(on ? 1 : 0);
	}
}

void Home::wake () {
	pointing_ = false;
	edge_dir_ = 0;
	ok_armed_ = false;
	if (lifting_) {
		// Home pressed while a tile was being moved: it goes back to its place
		items_ = lift_original_;
		anim_.assign(items_.size(), TileAnim());
		lifting_ = false;
		lift_.jump(0);
	}
	double now = now_ms();
	set_zone(Zone::dock, now);
	weather_day_ = 0;
	cal_.to_today(now);
	cal_.set_mode(false, now);
	cal_.set_head(false, now);
	cal_.set_month(0, now);
	cal_.scroll = 0;
	news_page_ = 0;
	detail_.reset();
}

const double NEWS_PAGE_MS = 20000;      // a page of headlines turns by itself this often while the remote is not on the card

int Home::news_per_page () const {
	auto c = layout().card(CardId::news);
	return c ? headlines_per_page(c->h, cfg_.cardSize / 100.0) : 1;
}

double Home::next_event (double now) const {
	double t = next_page_at_ < 0 ? -1 : std::max(0.0, next_page_at_ - now);
	if (ok_armed_ && !ok_fired_ && !lifting_) {
		double hold = std::max(0.0, ok_down_at_ + LONG_PRESS_MS - now);
		t = t < 0 ? hold : std::min(t, hold);
	}
	return t;
}

bool Home::tick (double now) {
	if (ok_armed_ && !ok_fired_ && !lifting_ && now - ok_down_at_ >= LONG_PRESS_MS) {
		ok_fired_ = true;
		ok_armed_ = false;           // its release is not a press (it goes to the menu, which ignores it too)
		int i = selected_index();
		if (zone_ == Zone::dock && on_long_press && i >= 0) on_long_press(items_[i]);
		return true;
	}
	int per = news_per_page();
	int count = news_ ? (int)news_->items.size() : 0, pages = std::max(1, (count + per - 1) / per);
	if (news_page_ >= pages) news_page_ = 0;
	bool turning = card_on(CardId::news) && zone_ != Zone::news && !detail_ && front_ && pages > 1;
	if (!turning) { next_page_at_ = -1; return false; }
	if (next_page_at_ < 0) { next_page_at_ = now + NEWS_PAGE_MS; return false; }
	if (now < next_page_at_) return false;
	news_page_ = (news_page_ + 1) % pages;
	next_page_at_ = now + NEWS_PAGE_MS;
	board_dirty_ = true;
	return true;
}

bool Home::news_key (bool left, bool right, bool up, bool down, bool back, bool ok, double now) {
	int per = news_per_page();
	int count = news_ ? (int)news_->items.size() : 0, pages = std::max(1, (count + per - 1) / per);
	int first = news_page_ * per, last = std::min(count, first + per) - 1;
	if (ok) {
		if (news_ && news_sel_ < count) { detail_ = news_->items[news_sel_]; detail_at_ = now; }
		return true;
	}
	if (left || right) {
		// ← → turn the pages; before the first or after the last they go on to the card beside
		int step = left ? -1 : 1, page = news_page_ + step;
		if (page < 0 || page > pages - 1) leave_side(CardId::news, step, now);
		else { news_page_ = page; news_sel_ = page * per; }
	} else if (up) { if (news_sel_ > first) news_sel_--; else enter_above(CardId::news, now); }
	else if (down) { if (news_sel_ < last) news_sel_++; else leave_down(CardId::news, now); }
	else if (back) set_zone(Zone::dock, now);
	else return false;
	return true;
}

static long long wall_now () {
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

bool Home::calendar_key (bool left, bool right, bool up, bool down, bool back, bool ok, double now) {
	long long wall = wall_now();
	auto shift_month = [&] (int months) {
		int lo = month_offset_of_day(wall, -CAL_BACK_DAYS), hi = month_offset_of_day(wall, CAL_AHEAD_DAYS);
		int next = std::min(hi, std::max(lo, cal_.month + months));
		if (next != cal_.month) cal_.set_month(next, now);
	};
	// back from the month to the days: on the day that was selected, or the first day of the month that was reached
	auto leave_month = [&] {
		if (month_offset_of_day(wall, cal_.offset) != cal_.month) {
			int first = day_offset_of_month(wall, cal_.month);
			cal_.shift_day((cal_.month == 0 ? 0 : std::min(CAL_AHEAD_DAYS, std::max(-CAL_BACK_DAYS, first))) - cal_.offset, now);
		}
		cal_.dir = 0;
		cal_.set_mode(false, now);
	};
	auto enter_month = [&] {
		cal_.set_month(month_offset_of_day(wall, cal_.offset), now);
		cal_.dir = 0;
		cal_.set_mode(true, now);
	};
	if (ok) { if (cal_.month_mode) leave_month(); else cal_.to_today(now); return true; }
	if (cal_.head) {
		if (left || right) { if (cal_.month_mode) shift_month(left ? -1 : 1); else cal_.shift_day(left ? -1 : 1, now); }
		else if (down) cal_.set_head(false, now);
		else if (up) { if (!cal_.month_mode) { enter_month(); cal_.set_head(false, now); } }
		else if (back) { if (cal_.month_mode) leave_month(); else set_zone(Zone::dock, now); }
		else return false;
		return true;
	}
	if (left || right) leave_side(CardId::calendar, left ? -1 : 1, now);
	else if (cal_.month_mode) {
		if (up) cal_.set_head(true, now);
		else if (down || back) { leave_month(); cal_.set_head(true, now); }
		else return false;
	}
	else if (up) { if (cal_.scroll > 0) cal_.scroll--; else cal_.set_head(true, now); }
	else if (down) { if (cal_.scroll < cal_.max_scroll) cal_.scroll++; else leave_down(CardId::calendar, now); }
	else if (back) set_zone(Zone::dock, now);
	else return false;
	return true;
}

bool Home::card_on (CardId id) const {
	if (id == CardId::weather) return cfg_.weather.on && weather_.has_value();
	if (id == CardId::calendar) return cfg_.calendar.on;
	if (id == CardId::news) return cfg_.news.on;
	if (id == CardId::quote) return cfg_.quote.on;
	return false;
}

BoardLayout Home::layout () const {
	Cards on;
	on.weather = card_on(CardId::weather);
	on.calendar = card_on(CardId::calendar);
	on.news = card_on(CardId::news);
	on.quote = card_on(CardId::quote);
	on.clock = {cfg_.clock.size, cfg_.clock.showDate, cfg_.clock.dateSize, !h24_ && cfg_.clock.showPeriod};
	int top = board_top({cfg_.iconScale, cfg_.bottomMargin, cfg_.showLabels, cfg_.rows}, (int)items_.size());
	return board_layout(on, top);
}

void Home::set_zone (Zone z, double now) {
	zone_ = z;
	board_dirty_ = true;
	row_dim_.set(z == Zone::dock ? 0 : 1, now, motion::FOCUS_MS, Ease::ease_out);
	Zone zones[3] = {Zone::weather, Zone::calendar, Zone::news};
	for (int i = 0; i < 3; i++) {
		card_focus_[i].set(z == zones[i] ? 1 : 0, now, 90, Ease::linear);
		card_grow_[i].set(z == zones[i] ? 1 : 0, now, 130, Ease::scroll);
	}
	// the ring on the selected tile shows only while the row has the focus
	auto s = sizes();
	if (s.empty()) return;
	auto at = clamp_position(s, pos_);
	int selected = row_offsets(s)[at.row] + at.cols[at.row];
	for (size_t i = 0; i < anim_.size(); i++) anim_[i].focus.set((int)i == selected && z == Zone::dock ? 1 : 0, now, motion::FOCUS_MS, Ease::ease_out);
}

bool Home::enter (std::optional<CardId> id, double now) {
	if (!id) return false;
	if (*id == CardId::weather) { set_zone(Zone::weather, now); return true; }
	if (*id == CardId::calendar) { cal_.set_head(false, now); set_zone(Zone::calendar, now); return true; }
	if (*id == CardId::news) { news_sel_ = news_page_ * news_per_page(); set_zone(Zone::news, now); return true; }
	return false;
}

void Home::enter_above (CardId from, double now) {
	BoardLayout l = layout();
	if (enter(next_in_column(l, from, -1), now)) return;
	int index = -1;
	for (size_t i = 0; i < l.columns.size(); i++)
		for (auto &c : l.columns[i].cards) if (c.id == from) index = (int)i;
	for (int i = index + 1; i < (int)l.columns.size(); i++) if (enter(entry_card(l, i), now)) return;
}

void Home::leave_down (CardId from, double now) {
	if (!enter(next_in_column(layout(), from, 1), now)) set_zone(Zone::dock, now);
}

void Home::leave_side (CardId from, int step, double now) { enter(beside_card(layout(), from, step), now); }

float Home::selected_tile_x () const {
	auto s = sizes();
	if (s.empty()) return 960;
	auto at = clamp_position(s, pos_);
	int step = tile_size() + tile_gap();
	double total = s[at.row] * step - tile_gap();
	float x0 = row_x0() + (total <= viewport_w() ? (float)((viewport_w() - total) / 2) : 0) - (float)rows_[at.row].offset.to;
	return x0 + at.cols[at.row] * step + tile_size() / 2.f;
}

double Home::target_offset (int r, int anchor) const {
	auto s = sizes();
	if (r >= (int)s.size()) return 0;
	int step = tile_size() + tile_gap();
	double total = s[r] * step - tile_gap();
	double vw = viewport_w();
	if (total <= vw) return 0;
	return std::min(std::max(anchor * step + tile_size() / 2.0 - vw / 2, 0.0), total - vw);
}

void Home::moved (double now) {
	auto s = sizes();
	pos_ = clamp_position(s, pos_);
	auto off = row_offsets(s);
	int selected = off[pos_.row] + pos_.cols[pos_.row];
	for (size_t i = 0; i < anim_.size(); i++) anim_[i].focus.set((int)i == selected && zone_ == Zone::dock ? 1 : 0, now, motion::FOCUS_MS, Ease::ease_out);
	if (pointing_) return;           // the row stays where it is under the pointer
	for (int r = 0; r < (int)s.size(); r++) {
		RowView &v = rows_[r];
		if (v.anchor == pos_.cols[r]) continue;
		v.fast = now - v.last_step < FAST_STEP_MS;
		v.last_step = now;
		v.anchor = pos_.cols[r];
		// a held key: short linear steps that run into one glide; otherwise the usual ease-out
		if (v.fast) v.offset.set(target_offset(r, v.anchor), now, 150, Ease::linear);
		else v.offset.set(target_offset(r, v.anchor), now, motion::SCROLL_MS, Ease::scroll);
	}
}

int Home::selected_index () const {
	auto s = sizes();
	if (s.empty()) return -1;
	auto at = clamp_position(s, pos_);
	int i = row_offsets(s)[at.row] + at.cols[at.row];
	return i < (int)items_.size() ? i : -1;
}

// ---------- moving a tile ----------

// The tiles that can be moved: every app and input (LG Home, the TV's settings and the launcher's settings end the row, in place)
int Home::movable_count () const {
	int n = 0;
	for (auto &t : items_) if (t.key != "lg-home" && t.key != "system-settings" && t.key != "settings" && t.key != "update") n++;
	return n;
}

std::vector<std::string> Home::lift_buttons () const {
	std::vector<std::string> b{"hide"};
	if (lift_trash_) b.push_back("trash");
	return b;
}

void Home::lift (bool can_uninstall, double now) {
	int i = selected_index();
	if (i < 0 || i >= movable_count()) return;
	lifting_ = true;
	lift_trash_ = can_uninstall;
	lift_btn_ = -1;
	lift_original_ = items_;
	lift_original_index_ = i;
	lift_.set(1, now, motion::FOCUS_MS, Ease::ease_out);
	board_dirty_ = true;
}

void Home::end_lift (double now) {
	if (!lifting_) return;
	lifting_ = false;
	lift_btn_ = -1;
	lift_.set(0, now, motion::FOCUS_MS, Ease::ease_out);
}

bool Home::lift_key (int sc, double now) {
	auto s = sizes();
	int i = selected_index();
	auto buttons = lift_buttons();
	if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) return true;       // on its release (key_up)
	if (sc == SDL_SCANCODE_WEBOS_BACK || sc == SDL_SCANCODE_ESCAPE) {
		// put it back where it was
		items_ = lift_original_;
		anim_.assign(items_.size(), TileAnim());
		pos_.cols[pos_.row] = lift_original_index_ - row_offsets(s)[pos_.row];
		for (size_t k = 0; k < anim_.size(); k++) anim_[k].focus.jump((int)k == lift_original_index_ ? 1 : 0);
		end_lift(now);
		moved(now);
		return true;
	}
	if (lift_btn_ >= 0) {
		if (sc == SDL_SCANCODE_LEFT) lift_btn_ = std::max(0, lift_btn_ - 1);
		else if (sc == SDL_SCANCODE_RIGHT) lift_btn_ = std::min((int)buttons.size() - 1, lift_btn_ + 1);
		else if (sc == SDL_SCANCODE_DOWN) lift_btn_ = -1;
		return true;
	}
	if (sc == SDL_SCANCODE_UP) { lift_btn_ = 0; return true; }
	int to = sc == SDL_SCANCODE_LEFT ? i - 1 : sc == SDL_SCANCODE_RIGHT ? i + 1 : -2;
	if (to == -2 || to < 0 || to >= movable_count()) return true;
	// the tile changes places with its neighbour; both glide to their new places
	int step = tile_size() + tile_gap();
	std::swap(items_[i], items_[to]);
	std::swap(anim_[i], anim_[to]);
	anim_[to].slide.jump((i - to) * step);
	anim_[to].slide.set(0, now, 200, Ease::tile);
	anim_[i].slide.jump((to - i) * step);
	anim_[i].slide.set(0, now, 200, Ease::tile);
	pos_.cols[pos_.row] = to - row_offsets(s)[pos_.row];
	moved(now);
	return true;
}

void Home::lift_ok (double now) {
	int i = selected_index();
	if (i < 0) return;
	std::string key = items_[i].key;
	auto buttons = lift_buttons();
	if (lift_btn_ >= 0 && lift_btn_ < (int)buttons.size()) {
		const std::string b = buttons[lift_btn_];
		// the order as it was, then the button's own action
		items_ = lift_original_;
		anim_.assign(items_.size(), TileAnim());
		end_lift(now);
		if (b == "hide" && on_hide) on_hide(key);
		if (b == "trash" && on_uninstall) on_uninstall(key);
		return;
	}
	end_lift(now);
	std::vector<std::string> keys;
	for (auto &t : items_) keys.push_back(t.key);
	if (on_reorder) on_reorder(keys);
}

bool Home::key_up (int sc, double now) {
	if (sc != SDL_SCANCODE_RETURN && sc != SDL_SCANCODE_KP_ENTER) return false;
	if (!ok_armed_) return false;          // a release without its press (the OK that closed a screen)
	ok_armed_ = false;
	if (lifting_) { if (!ok_fired_) lift_ok(now); return true; }
	int i = selected_index();
	if (!ok_fired_ && zone_ == Zone::dock && on_activate && i >= 0) on_activate(items_[i]);
	return true;
}

bool Home::key (int sc, bool repeat, double now) {
	auto s = sizes();
	if (s.empty() || items_.empty()) return false;
	pointing_ = false;
	edge_dir_ = 0;
	if (lifting_) {
		if ((sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) && !repeat && !ok_armed_) { ok_armed_ = true; ok_fired_ = false; ok_down_at_ = now; }
		return lift_key(sc, now);
	}
	// any key may change the cards; only moving along the row leaves them as they are (put back below)
	bool was_dirty = board_dirty_;
	board_dirty_ = true;
	bool left = sc == SDL_SCANCODE_LEFT, right = sc == SDL_SCANCODE_RIGHT, up = sc == SDL_SCANCODE_UP, down = sc == SDL_SCANCODE_DOWN;
	bool back = sc == SDL_SCANCODE_WEBOS_BACK || sc == SDL_SCANCODE_ESCAPE, ok = sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER;
	if (zone_ == Zone::weather && card_on(CardId::weather)) {
		// ← → choose the day; past the first or the last day they go on to the card beside
		int days = (int)std::min(weather_->daily.size(), (size_t)(layout().narrow ? 5 : weather_->daily.size()));
		int day = std::min(weather_day_, std::max(0, days - 1));
		if (left) { if (day > 0) weather_day_ = day - 1; else leave_side(CardId::weather, -1, now); }
		else if (right) { if (day < days - 1) weather_day_ = day + 1; else leave_side(CardId::weather, 1, now); }
		else if (up) enter_above(CardId::weather, now);
		else if (down) leave_down(CardId::weather, now);
		else if (back) set_zone(Zone::dock, now);
		else if (ok) weather_day_ = 0;
		else return false;
		return true;
	}
	if (detail_) {
		// the headline over the screen: OK or Back closes it
		if (ok || back) detail_.reset();
		return true;
	}
	if (zone_ == Zone::news && card_on(CardId::news)) return news_key(left, right, up, down, back, ok, now);
	if (zone_ == Zone::calendar && card_on(CardId::calendar)) {
		if (calendar_key(left, right, up, down, back, ok, now)) return true;
		return false;
	}
	if (zone_ != Zone::dock) set_zone(Zone::dock, now);         // a card that went away
	// Up from the top row climbs to the card above the selected tile
	if (up && clamp_position(s, pos_).row == 0) {
		BoardLayout l = layout();
		if (enter(entry_card(l, column_at(l, (int)selected_tile_x())), now)) return true;
	}
	board_dirty_ = was_dirty || zone_ != Zone::dock;
	if (sc == SDL_SCANCODE_DOWN && clamp_position(s, pos_).row == (int)s.size() - 1 && on_quick) { on_quick(); return true; }
	if (sc == SDL_SCANCODE_LEFT) pos_ = move_position(s, pos_, Dir::left);
	else if (sc == SDL_SCANCODE_RIGHT) pos_ = move_position(s, pos_, Dir::right);
	else if (sc == SDL_SCANCODE_UP) pos_ = move_position(s, pos_, Dir::up);
	else if (sc == SDL_SCANCODE_DOWN) pos_ = move_position(s, pos_, Dir::down);
	else if (sc == SDL_SCANCODE_WEBOS_BACK || sc == SDL_SCANCODE_ESCAPE) pos_ = RowsPosition();     // Back jumps to the first app
	else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) {
		// acted on when let go, or after a hold (tick)
		if (!repeat && !ok_armed_) { ok_armed_ = true; ok_fired_ = false; ok_down_at_ = now; }
		return true;
	} else return false;
	moved(now);
	return true;
}

int Home::tile_at (float x, float y, int &row) const {
	auto s = sizes();
	auto off = row_offsets(s);
	int ts = tile_size(), step = ts + tile_gap();
	for (int r = 0; r < (int)s.size(); r++) {
		float top = row_top(r);
		if (y < top || y > top + ts + label_area()) continue;
		double total = s[r] * step - tile_gap();
		float x0 = row_x0() + (total <= viewport_w() ? (float)((viewport_w() - total) / 2) : 0) - (float)rows_[r].offset.to;
		int col = (int)std::floor((x - x0 + tile_gap() / 2.0) / step);
		if (col < 0 || col >= s[r]) continue;
		row = r;
		return off[r] + col;
	}
	return -1;
}

namespace {
// Row.tsx: the zone reaches 0.75 of a tile step in from either end of what is shown (and out over the viewport's padding); the
// glide starts after 200 ms (a pointer only passing by does not move the row), 450 px/s where the zone begins, 1500 at the end
const float EDGE_STEPS = 0.75f, EDGE_DELAY_MS = 200, EDGE_SPEED_MIN = 450, EDGE_SPEED_MAX = 1500;
}

void Home::edge_zone (double now) {
	auto s = sizes();
	int ts = tile_size(), step = ts + tile_gap();
	int row = -1, dir = 0;
	float depth = 0;
	for (int r = 0; r < (int)s.size(); r++) {
		float top = row_top(r);
		if (py_ < top || py_ > top + ts + label_area()) continue;
		double total = s[r] * step - tile_gap(), max = total - viewport_w();
		if (max <= 0) break;
		float zone = VIEW_PAD_SIDE + EDGE_STEPS * step, left = row_x0() - VIEW_PAD_SIDE, right = row_x0() + viewport_w() + VIEW_PAD_SIDE;
		double cur = rows_[r].offset.at(now);
		if (px_ < left + zone && cur > 0.5) { dir = -1; depth = 1 - (px_ - left) / zone; }
		else if (px_ > right - zone && cur < max - 0.5) { dir = 1; depth = 1 - (right - px_) / zone; }
		row = r;
		break;
	}
	if (dir != edge_dir_ || row != edge_row_) { edge_since_ = now; edge_last_ = now; }
	edge_dir_ = dir;
	edge_row_ = dir ? row : -1;
	edge_depth_ = std::max(0.f, std::min(1.f, depth));
}

void Home::edge_glide (double now) {
	if (!edge_dir_ || edge_row_ < 0) return;
	if (now - edge_since_ < EDGE_DELAY_MS) { edge_last_ = now; return; }
	auto s = sizes();
	if (edge_row_ >= (int)s.size()) { edge_dir_ = 0; return; }
	int ts = tile_size(), step = ts + tile_gap();
	double max = std::max(0.0, s[edge_row_] * step - tile_gap() - (double)viewport_w());
	RowView &v = rows_[edge_row_];
	double dt = std::min(50.0, now - edge_last_);
	edge_last_ = now;
	double cur = std::min(max, std::max(0.0, v.offset.at(now) + edge_dir_ * (EDGE_SPEED_MIN + (EDGE_SPEED_MAX - EDGE_SPEED_MIN) * edge_depth_) * dt / 1000));
	v.offset.jump(cur);
	// the row stays where it stopped: the keys go on from the tile in its middle
	v.anchor = std::min(s[edge_row_] - 1, std::max(0, (int)std::lround((cur + viewport_w() / 2 - ts / 2.0) / step)));
	// the tile under the resting pointer is the one that grows
	int row = 0, i = tile_at(px_, py_, row);
	if (i >= 0) {
		auto off = row_offsets(s);
		if (pos_.row != row || pos_.cols[row] != i - off[row]) {
			pos_.row = row;
			pos_.cols.resize(2, 0);
			pos_.cols[row] = i - off[row];
			moved(now);
		}
	}
	if (cur <= 0 || cur >= max) edge_dir_ = 0;
}

void Home::pointer (float x, float y, double now) {
	px_ = x;
	py_ = y;
	pointing_ = true;
	edge_zone(now);
	int row = 0, i = tile_at(x, y, row);
	if (i < 0) { pointer_card(x, y, now); return; }
	if (zone_ != Zone::dock) set_zone(Zone::dock, now);
	auto off = row_offsets(sizes());
	if (pos_.row == row && pos_.cols[row] == i - off[row]) return;
	pos_.row = row;
	pos_.cols.resize(2, 0);
	pos_.cols[row] = i - off[row];
	moved(now);
}

void Home::pointer_left () { pointing_ = false; edge_dir_ = 0; }

// Over a card the pointer gives it the focus, as the keys would (Home.tsx: onHover), and the day or the headline under it is chosen
void Home::pointer_card (float x, float y, double now) {
	if (detail_ || lifting_) return;
	for (auto &col : board_.columns)
		for (auto &c : col.cards) {
			if (x < c.x || x > c.x + c.w || y < c.y || y > c.y + c.h) continue;
			if (c.id == CardId::weather && card_on(CardId::weather)) {
				if (zone_ != Zone::weather) set_zone(Zone::weather, now);
				int d = weather_day_at(c, weather_days(), cfg_.cardSize, x, y);
				if (d >= 0 && d != weather_day_) { weather_day_ = d; board_dirty_ = true; }
			} else if (c.id == CardId::calendar && card_on(CardId::calendar)) {
				if (zone_ != Zone::calendar) { cal_.set_head(false, now); set_zone(Zone::calendar, now); }
			} else if (c.id == CardId::news && card_on(CardId::news)) {
				if (zone_ != Zone::news) { news_sel_ = news_page_ * news_per_page(); set_zone(Zone::news, now); }
				int n = news_item_at(c, news_ ? &news_->items : nullptr, news_page_, cfg_.cardSize, clock_face(cfg_.clock.font), x, y);
				if (n >= 0 && n != news_sel_) { news_sel_ = n; board_dirty_ = true; }
			}
			return;
		}
}

int Home::weather_days () const {
	if (!weather_) return 0;
	return (int)std::min(weather_->daily.size(), (size_t)(layout().narrow ? 5 : weather_->daily.size()));
}

bool Home::click_card (float x, float y, double now) {
	if (detail_) { detail_.reset(); board_dirty_ = true; return true; }      // a click closes the headline over the screen
	for (auto &col : board_.columns)
		for (auto &c : col.cards) {
			if (x < c.x || x > c.x + c.w || y < c.y || y > c.y + c.h) continue;
			pointer_card(x, y, now);
			board_dirty_ = true;
			if (c.id == CardId::weather && card_on(CardId::weather)) {
				int d = weather_day_at(c, weather_days(), cfg_.cardSize, x, y);
				if (d >= 0) weather_day_ = d;
			} else if (c.id == CardId::calendar && card_on(CardId::calendar)) {
				int days = 0, hit = calendar_hit(c, cal_, cfg_.cardSize, x, y, days);
				if (hit == -1 || hit == 1) {
					if (cal_.month_mode) {
						long long wall = wall_now();
						int lo = month_offset_of_day(wall, -CAL_BACK_DAYS), hi = month_offset_of_day(wall, CAL_AHEAD_DAYS);
						int next = std::min(hi, std::max(lo, cal_.month + hit));
						if (next != cal_.month) cal_.set_month(next, now);
					} else cal_.shift_day(hit, now);
				} else if (hit == 2 && days != 0) cal_.shift_day(days, now);
			} else if (c.id == CardId::news && card_on(CardId::news)) {
				int n = news_item_at(c, news_ ? &news_->items : nullptr, news_page_, cfg_.cardSize, clock_face(cfg_.clock.font), x, y);
				if (n >= 0 && news_) { news_sel_ = n; detail_ = news_->items[n]; detail_at_ = now; }
			}
			return true;
		}
	return false;
}

void Home::click (float x, float y, double now) {
	int row = 0, i = tile_at(x, y, row);
	if (i < 0) { click_card(x, y, now); return; }
	pointer(x, y, now);
	if (on_activate) on_activate(items_[i]);
}

void Home::wheel (int dy, double now) {
	auto s = sizes();
	if (s.empty()) return;
	int r = pos_.row;
	RowView &v = rows_[r];
	v.anchor = std::min(s[r] - 1, std::max(0, v.anchor + (dy > 0 ? -1 : 1)));
	v.offset.set(target_offset(r, v.anchor), now, motion::SCROLL_MS, Ease::scroll);
}

bool Home::board_animating (double now) const {
	if (card_on(CardId::calendar) && cal_.animating(now)) return true;
	for (auto &f : card_focus_) if (f.moving(now)) return true;
	return false;
}

bool Home::animating (double now) const {
	if (edge_dir_) return true;          // gliding, or about to
	for (auto &a : anim_) if (a.focus.moving(now) || a.slide.moving(now)) return true;
	if (lift_.moving(now)) return true;
	for (auto &r : rows_) if (r.offset.moving(now)) return true;
	if (row_dim_.moving(now)) return true;
	for (auto &g : card_grow_) if (g.moving(now)) return true;
	for (auto &f : card_focus_) if (f.moving(now)) return true;
	if (card_on(CardId::calendar) && cal_.animating(now)) return true;
	if (detail_ && now - detail_at_ < 200) return true;
	for (auto &f : card_focus_) if (f.moving(now)) return true;
	return false;
}

void Home::draw_tile (const TileModel &m, float x, float y, float f, double now, bool label_on) {
	(void)now;
	const Theme &t = theme_named(cfg_.theme);
	int ts = tile_size();
	float radius = (float)std::lround(layout::TILE_RADIUS * cfg_.iconScale / 100.0);
	// the frame grows from its bottom edge when focused (.row--dock .tile__frame)
	float s = 1 + (float)(motion::FOCUS_SCALE - 1) * f;
	float w = ts * s;
	Rect frame{x + (ts - w) / 2, y + ts - w, w, w};
	float r = radius * s;
	// the ring layer's dark glow, then the box's own soft shadow
	if (f > 0) gfx::shadow(frame, r, 16 * s, 36 * s, {0, 0, 0, 0.45f * f});
	gfx::shadow(frame, r, 12 * s, 28 * s, {0, 0, 0, 0.4f});
	Color box = f > 0.5f ? t.tileBgFocus : t.tileBg;
	draw_tile_art(m, frame, r, s, ts, cfg_.iconScale, t, box);
	// on the home row the ring layer has only its dark glow (.row--dock .tile__ring replaces the white ring)
	if (!cfg_.showLabels) return;
	float label_opacity = cfg_.labelsSelectedOnly ? f : 1;
	if (label_opacity <= 0) return;
	int font = label_font();
	TextStyle st{Face::ui, font, 400, 0};
	float lw = (float)(ts + tile_gap() - 8);
	float line_h = std::round(font * 1.2f);
	auto lines = text::wrap(m.label, st, (int)lw, 2);
	Color c = label_on ? t.textPrimary : t.textSecondary;
	Color mixed{t.textSecondary.r + (t.textPrimary.r - t.textSecondary.r) * f, t.textSecondary.g + (t.textPrimary.g - t.textSecondary.g) * f,
		t.textSecondary.b + (t.textPrimary.b - t.textSecondary.b) * f, 1};
	(void)c;
	float ly = y + ts + 14;
	for (size_t i = 0; i < lines.size(); i++) {
		const TextLine &l = text::line(lines[i], st);
		float lx = std::round(x + ts / 2.f - l.w / 2.f);
		text::draw(lines[i], st, lx, std::round(ly + i * line_h + (line_h - l.h) / 2), mixed, label_opacity);
	}
}

int Home::live_mask (double now) const {
	// the cards the remote is on, or that are still letting go of it, are drawn every frame over the picture of the board
	int mask = 0;
	for (int i = 0; i < 3; i++) if (card_focus_[i].at(now) > 0 || card_grow_[i].at(now) > 0) mask |= 1 << i;
	return mask;
}

static int card_index (CardId id) { return id == CardId::weather ? 0 : id == CardId::calendar ? 1 : id == CardId::news ? 2 : -1; }

void Home::prepare_board () {
	// lib/pixelshift.ts: a small loop of whole-pixel offsets, one step a minute
	static const int STEPS[8][2] = {{0, 0}, {4, 2}, {8, 0}, {4, -2}, {0, -4}, {-4, -2}, {-8, 0}, {-4, 2}};
	bool shift_on = cfg_.pixelShift == "on" || (cfg_.pixelShift == "auto" && oled_);
	long long minute = wall_now() / 60000;
	const int *shift = STEPS[((minute % 8) + 8) % 8];
	shift_x_ = shift_on ? shift[0] : 0;
	shift_y_ = shift_on ? shift[1] : 0;
	board_ = layout();
	for (auto &col : board_.columns) for (auto &c : col.cards) { c.x += shift_x_; c.y += shift_y_; }
	// the plates: one look for all, from the brightest place behind any of them
	std::vector<std::array<int, 4>> rects;
	for (auto &col : board_.columns) for (auto &c : col.cards) rects.push_back({c.x, c.y, c.w, c.h});
	look_ = plate_look(cfg_.plates, cfg_.theme, wp_ ? wp_->glass().map : BrightnessMap(), rects);
	old_look_ = plate_look(cfg_.plates, cfg_.theme, wp_ ? wp_->old_glass().map : BrightnessMap(), rects);
}

void Home::draw_card (const BoardCard &c, double now, const std::tm &clock) {
	Face face = clock_face(cfg_.clock.font);
	int i = card_index(c.id);
	float focus = i >= 0 ? (float)card_focus_[i].at(now) : 0;
	// .weather--focus, .cal--focus, .news--focus: the card grows by 1.02 (its text a little soft meanwhile, as on the web version)
	float grow = i >= 0 ? 1 + 0.02f * (float)card_grow_[i].at(now) : 1;
	if (grow != 1) gfx::set_grow(c.x + c.w / 2.f, c.y + c.h / 2.f, grow);
	if (wp_) draw_plate({(float)c.x, (float)c.y, (float)c.w, (float)c.h}, *wp_, cfg_, look_, old_look_, focus, now);
	if (c.id == CardId::clock) draw_clock(c, clock, cfg_.clock, lang_, h24_, board_.narrow);
	else if (c.id == CardId::weather && weather_) {
		WeatherView v = weather_view(*weather_, cfg_.weather.unit, "", weather_day_);     // no city on the card
		// in three columns the card is narrow: five days, so the names and temperatures keep their room
		if (board_.narrow && v.days.size() > 5) v.days.resize(5);
		draw_weather(c, v, cfg_.cardSize, face, clock, focus);
	} else if (c.id == CardId::calendar)
		draw_calendar(c, cal_, calendar_ ? &*calendar_ : nullptr, cal_loading_, cfg_.cardSize, face, wall_now(), h24_, zone_ == Zone::calendar, focus);
	else if (c.id == CardId::news)
		draw_news(c, news_ ? &news_->items : nullptr, news_loading_, news_page_, news_sel_, zone_ == Zone::news, cfg_.cardSize, face, wall_now());
	else if (c.id == CardId::quote)
		draw_quote(c, quote_ ? &*quote_ : nullptr, quote_loading_, cfg_.cardSize, face);
	if (grow != 1) gfx::set_grow(0, 0, 1);
}

void Home::draw_board (double now, const std::tm &clock) {
	board_dirty_ = false;
	prepare_board();
	drawn_live_ = live_mask(now);
	for (auto &col : board_.columns)
		for (auto &c : col.cards) {
			int i = card_index(c.id);
			if (i >= 0 && (drawn_live_ & (1 << i))) continue;      // drawn over the picture, every frame
			draw_card(c, now, clock);
		}
}

void Home::draw_live (double now, const std::tm &clock) {
	for (auto &col : board_.columns)
		for (auto &c : col.cards) {
			int i = card_index(c.id);
			if (i >= 0 && (drawn_live_ & (1 << i))) draw_card(c, now, clock);
		}
}

bool Home::board_needed (double now) const {
	if (board_dirty_ || live_mask(now) != drawn_live_) return true;
	// a card that moves while it is not drawn live (the calendar settling after the remote left it)
	return card_on(CardId::calendar) && !(drawn_live_ & 2) && cal_.animating(now);
}

void Home::draw_lift (float x, float top_y, double now) {
	// the buttons over the lifted tile: round, with their pictures; the focused one light, its name over it
	float lift = (float)lift_.at(now);
	int ts = tile_size();
	auto buttons = lift_buttons();
	const float B = 76, GAP = 22;
	float total = buttons.size() * B + (buttons.size() - 1) * GAP;
	float by = top_y - LIFT_PX - ts * (float)(motion::FOCUS_SCALE - 1) - 36 - B;
	float bx = std::round(x + ts / 2.f - total / 2);
	gfx::set_alpha(lift);
	for (size_t k = 0; k < buttons.size(); k++) {
		Rect b{bx + k * (B + GAP), by, B, B};
		bool on = lift_btn_ == (int)k;
		gfx::shadow(b, B / 2, 8, 24, {0, 0, 0, 0.45f});
		gfx::rect(b, B / 2, on ? Color::hex("#F2F2F7") : Color{1, 1, 1, 0.16f});
		if (TexturePtr g = icons::get(join_path(package_dir(), "glyphs/" + buttons[k] + ".png"), 40))
			gfx::image(*g, {b.x + (B - 40) / 2, b.y + (B - 40) / 2, 40, 40}, 1, 0, on ? Color::hex("#111216") : Color{1, 1, 1, 1});
		if (on) {
			std::string name = buttons[k] == "trash" ? tr("Uninstall", "Удалить") : tr("Hide from the home screen", "Скрыть с главного экрана");
			TextStyle st{Face::ui, 24, 500, 0};
			const TextLine &l = text::line(name, st);
			text::draw(name, st, std::round(b.x + B / 2 - l.w / 2.f), std::round(b.y - 14 - l.h), {1, 1, 1, 1});
		}
	}
	gfx::set_alpha(1);
}

void Home::draw_row (double now) {
	edge_glide(now);          // the row glides under the resting pointer
	auto s = sizes();
	auto off = row_offsets(s);
	int ts = tile_size(), step = ts + tile_gap();
	// moving a tile: the screen above the row darkens, more towards the top
	float lift = (float)lift_.at(now);
	if (lift > 0 && !s.empty()) {
		float bottom = row_top(0) + ts * 0.6f;
		GradientStop stops[] = {{{0, 0, 0, 0.82f}, 0}, {{0, 0, 0, 0.6f}, 0.6f}, {{0, 0, 0, 0}, 1}};
		gfx::gradient({0, 0, 1920, bottom}, 180, stops, 3, lift);
	}
	for (int r = 0; r < (int)s.size(); r++) {
		float top_y = row_top(r);
		double total = s[r] * step - tile_gap();
		bool fits = total <= viewport_w();
		float x0 = row_x0() + (fits ? (float)((viewport_w() - total) / 2) : 0) - (float)std::round(rows_[r].offset.at(now));
		gfx::push_clip({row_x0() - VIEW_PAD_SIDE, top_y - VIEW_PAD_TOP, viewport_w() + 2 * VIEW_PAD_SIDE, ts + label_area() + VIEW_PAD_TOP + VIEW_PAD_BOTTOM});
		// the row fades to 0.55 while the remote is on the cards
		gfx::set_alpha(1 - (float)(1 - motion::UNFOCUSED_ROW_OPACITY) * (float)row_dim_.at(now));
		int focused = -1;
		for (int c = 0; c < s[r]; c++) {
			int i = off[r] + c;
			float x = x0 + c * step + (float)anim_[i].slide.at(now);
			if (x + ts < -VIEW_PAD_SIDE || x > 1920 + VIEW_PAD_SIDE) continue;
			float f = (float)anim_[i].focus.at(now);
			if (f > 0.01f && focused < 0) { focused = i; continue; }        // drawn last, above its neighbours
			draw_tile(items_[i], x, top_y, f, now, false);
		}
		float lift = (float)lift_.at(now);
		if (focused >= 0) {
			float fx = x0 + (focused - off[r]) * step + (float)anim_[focused].slide.at(now);
			draw_tile(items_[focused], fx, top_y - std::round(LIFT_PX * lift), (float)anim_[focused].focus.at(now), now, true);
		}
		gfx::pop_clip();
		gfx::set_alpha(1);
		if (focused >= 0 && lift > 0) draw_lift(x0 + (focused - off[r]) * step + (float)anim_[focused].slide.at(now), top_y, now);
	}
}

// a pill of dark glass with a green dot, right-aligned with the row, just over it (not while a tile is being moved)
void Home::draw_notice () {
	if (notice_.empty() || lifting_) return;
	TextStyle st{Face::ui, 21, 500, 0};
	std::string text = text::ellipsize(notice_, st, 700);
	const TextLine &l = text::line(text, st);
	const float H = 40, PAD = 18, DOT = 10;
	float w = PAD + DOT + 12 + l.w + PAD;
	float x = 1920.f - cfg_.sideMargin - w + shift_x_, y = row_top(0) - 12 - H;
	gfx::rect({x, y, w, H}, H / 2, {0, 0, 0, 0.45f});
	gfx::rect({x + PAD, y + (H - DOT) / 2, DOT, DOT}, DOT / 2, Color::hex("#30D158"));
	text::draw(text, st, std::round(x + PAD + DOT + 12), std::round(y + (H - l.h) / 2), {1, 1, 1, 0.92f});
}

void Home::draw_overlay (double now) {
	draw_notice();
	if (detail_) draw_news_detail(*detail_, clock_face(cfg_.clock.font), wall_now(), (float)ease(Ease::ease_out, std::min(1.0, (now - detail_at_) / 200)));}

void Home::draw (double now, const std::tm &clock) {
	draw_board(now, clock);
	draw_live(now, clock);
	draw_row(now);
	draw_overlay(now);
}

