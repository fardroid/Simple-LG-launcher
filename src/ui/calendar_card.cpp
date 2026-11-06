#include "ui/calendar_card.h"

#include "data/apps.h"
#include "i18n/dates.h"
#include "i18n/i18n.h"
#include "sys/fs.h"
#include "ui/icons.h"
#include "ui/theme.h"

#include <algorithm>
#include <cmath>

namespace {

const int WEEKDAYS[] = {1, 2, 3, 4, 5, 6, 0};      // Monday first

std::tm local (long long ms) {
	std::time_t t = (std::time_t)(ms / 1000);
	std::tm tm;
	localtime_r(&t, &tm);
	return tm;
}

// a line of text in a CSS line box `line` px tall starting at `top`, its left edge at x (or centred on x)
void put (const std::string &s, const TextStyle &st, float x, float top, float line, Color c, float opacity = 1, bool centre = false) {
	const TextLine &l = text::line(s, st);
	float lx = centre ? x - l.w / 2.f : x;
	text::draw(s, st, std::round(lx), std::round(top + (line - l.h) / 2), c, opacity);
}

void dots (const std::vector<int> &which, float cx, float y, float opacity) {
	size_t n = std::min<size_t>(which.size(), 4);
	float w = n * 12.f, x = cx - w / 2 + 2;
	for (size_t i = 0; i < n; i++) gfx::rect({x + i * 12, y, 8, 8}, 4, Color::hex(CAL_COLORS[which[i] % 6]).with_alpha(opacity));
}

void glyph (const char *name, Rect r, Color c) {
	TexturePtr t = icons::get(join_path(package_dir(), std::string("glyphs/") + name + ".png"), (int)r.w);
	if (t) gfx::image(*t, r, 1, 0, c);
}

}  // namespace

void CalendarState::shift_day (int days, double now) {
	int next = std::min(CAL_AHEAD_DAYS, std::max(-CAL_BACK_DAYS, offset + days));
	if (next == offset) return;
	dir = days > 0 ? 1 : -1;
	offset = next;
	scroll = 0;
	day_changed = now;
	strip.set(offset, now, 320, Ease::scroll);
	list.set(0, now, 220, Ease::scroll);
}

void CalendarState::to_today (double now) {
	if (offset == 0) return;
	dir = offset > 0 ? -1 : 1;
	offset = 0;
	scroll = 0;
	day_changed = now;
	strip.set(0, now, 320, Ease::scroll);
	list.set(0, now, 220, Ease::scroll);
}

void CalendarState::set_month (int m, double now) { month = m; months.set(m, now, 380, Ease::scroll); }
void CalendarState::set_mode (bool m, double now) { month_mode = m; layer.set(m ? 1 : 0, now, 440, Ease::scroll); }
void CalendarState::set_head (bool on, double now) { head = on; head_glow.set(on ? 1 : 0, now, 90, Ease::linear); }
void CalendarState::set_scroll (int s, double now) { scroll = s; }

void CalendarState::check_memo (const CalendarData *data, long long now_ms, bool h24) {
	// the list of a day says what is past and what is on now: it is made again every minute
	std::string key = std::to_string((long long)(data ? data->at : 0)) + "|" + std::to_string(data ? data->events.size() : 0) + "|" +
		std::to_string(now_ms / 60000) + "|" + std::to_string(offset) + "|" + (h24 ? "24" : "12") + "|" + lang();
	if (key == memo_key) return;
	memo_key = key;
	strip_dots.clear();
	month_views.clear();
	day_events.clear();
}

bool CalendarState::animating (double now) const {
	return strip.moving(now) || layer.moving(now) || months.moving(now) || list.moving(now) || head_glow.moving(now) || now - day_changed < 300;
}

void draw_calendar (const BoardCard &card, CalendarState &st, const CalendarData *data, bool loading, int card_size, Face face,
		long long now_ms, bool h24, bool focused, float focus) {
	const Theme &t = current_theme();
	const Color AMBER = t.accent;
	double now = ::now_ms();
	float k = card_size / 100.f;
	int t_title = (int)std::lround(26 * k), t1 = (int)std::lround(22 * k), t2 = (int)std::lround(17 * k), t3 = (int)std::lround(14 * k);
	float x0 = card.x + 28.f, w = card.w - 56.f, y0 = card.y + 26.f, bottom = card.y + card.h - 26.f;
	std::string lang_now = lang();

	st.check_memo(data, now_ms, h24);
	auto month_of = [&] (int m) -> const MonthView & {
		auto it = st.month_views.find(m);
		if (it == st.month_views.end()) it = st.month_views.emplace(m, month_view(data, now_ms, m, start_of_day(now_ms, st.offset))).first;
		return it->second;
	};
	// ---- the head: the arrows and the date between them ----
	const MonthView &view = month_of(st.month);
	std::string title = st.month_mode ? std::to_string(view.year) : relative_day(st.offset);
	std::string heading = st.month_mode ? view.name : day_heading(now_ms, st.offset);
	TextStyle rel{face, t3, 600, t3 * 0.12f}, full{face, t_title, 500, 0};
	float title_h = t3 + 6 + t_title * 1.2f, head_h = std::max(52.f, title_h);
	float hg = (float)st.head_glow.at(now) * (focused ? 1 : 0);
	if (hg > 0) gfx::rect({x0 - 8, y0 - 8, w + 16, head_h + 16}, 34, {1, 1, 1, 0.12f * hg});
	float ny = y0 + (head_h - 52) / 2;
	gfx::rect({x0, ny, 52, 52}, 26, {1, 1, 1, 0.1f + 0.14f * hg});
	gfx::rect({x0 + w - 52, ny, 52, 52}, 26, {1, 1, 1, 0.1f + 0.14f * hg});
	glyph("chev-left", {x0 + 14, ny + 14, 24, 24}, t.textPrimary);
	glyph("chev-right", {x0 + w - 38, ny + 14, 24, 24}, t.textPrimary);
	float tx = x0 + 52 + 16, tw = w - 104 - 32, ty = y0 + (head_h - title_h) / 2;
	put(text::upper(title), rel, tx, ty, (float)t3, AMBER);
	put(text::ellipsize(heading, full, (int)tw), full, tx, ty + t3 + 6, t_title * 1.2f, t.textPrimary);

	// ---- the stage: the day view and the month, one above the other ----
	float stage_y = y0 + head_h + 14, stage_h = bottom - stage_y;
	if (stage_h <= 0) return;
	gfx::push_clip({x0, stage_y, w, stage_h});
	float lt = (float)st.layer.at(now);
	float day_y = stage_y + stage_h * lt, month_y = stage_y - stage_h * (1 - lt);

	if (lt < 1) {
		// the strip of days: the chosen one stays in the middle under the highlight
		float strip_h = t3 + t1 * 1.2f + 42, cell = w / 7;
		gfx::rect({x0 + 3 * cell, day_y, cell, strip_h}, 22, {1, 1, 1, 0.16f + (st.head ? 0.06f : 0)});
		double shown = st.strip.at(now);
		gfx::push_clip({x0, day_y, w, strip_h});
		TextStyle ds{face, t3, 400, 0}, db{face, t1, 600, 0};
		for (int o = st.offset - 9; o <= st.offset + 9; o++) {
			float cx = x0 + (float)((o + 3 - shown) * cell) + cell / 2;
			if (cx < x0 - cell || cx > x0 + w + cell) continue;
			// the day's date by plain arithmetic from today (local midnight is not needed for its name and number)
			std::tm d = local(now_ms + (long long)o * 86400000);
			Color sc = o == st.offset || o == 0 ? AMBER : t.textSecondary, bc = o == 0 ? AMBER : t.textPrimary;
			float y = day_y + 12;
			put(day_short(lang_now, d.tm_wday), ds, cx, y, (float)t3, sc, 1, true);
			y += t3 + 4;
			put(std::to_string(d.tm_mday), db, cx, y, t1 * 1.2f, bc, 1, true);
			y += t1 * 1.2f + 4;
			auto dit = st.strip_dots.find(o);
			if (dit == st.strip_dots.end()) dit = st.strip_dots.emplace(o, dots_of_day(data, start_of_day(now_ms, o))).first;
			dots(dit->second, cx, y, 1);
		}
		gfx::pop_clip();

		// the day's events, in a list that scrolls an event at a time
		float ev_y = day_y + strip_h + 14, ev_h = day_y + stage_h - ev_y;
		auto eit = st.day_events.find(st.offset);
		if (eit == st.day_events.end()) eit = st.day_events.emplace(st.offset, events_of_day(data, start_of_day(now_ms, st.offset), now_ms, h24)).first;
		const auto &events = eit->second;
		bool many = data && data->sources.size() > 1;
		double since = now - st.day_changed;
		float slide = since < 300 ? (float)ease(Ease::scroll, since / 300) : 1;
		float sx = st.dir * 36.f * (1 - slide);
		TextStyle time_st{face, t1, 600, 0}, note_st{face, t3, 400, 0}, name_st{face, t1, 500, 0}, place_st{face, t2, 400, 0};
		float time_w = 84 * k;
		// the rows' heights, and how far the list can scroll so that its last row is in view
		std::vector<float> heights, tops;
		float total = 0;
		for (auto &e : events) {
			float left = t1 * 1.15f + (e.note.empty() ? 0 : 4 + t3 * 1.2f);
			std::string place = e.place;
			if (many && e.src < (int)data->sources.size() && !data->sources[e.src].name.empty()) place += (place.empty() ? "" : " · ") + data->sources[e.src].name;
			float right = t1 * 1.2f + (place.empty() ? 0 : 4 + t2 * 1.2f);
			tops.push_back(total);
			heights.push_back(std::max(left, right) + 24);
			total += heights.back() + 12;
		}
		int max = 0;
		while (max < (int)events.size() - 1 && total - 12 - tops[max] > ev_h) max++;
		st.max_scroll = max;
		int first = std::min(st.scroll, max);
		float top = first < (int)tops.size() ? tops[first] : 0;
		st.list.set(top, now, 220, Ease::scroll);
		float shown_top = (float)st.list.at(now);
		bool above = shown_top > 0.5f, below = total - 12 - shown_top > ev_h + 1;
		gfx::push_clip({x0, ev_y, w, ev_h});
		for (size_t i = 0; i < events.size(); i++) {
			const DayEvent &e = events[i];
			float ry = ev_y + tops[i] - shown_top, rh = heights[i];
			if (ry > ev_y + ev_h || ry + rh < ev_y) continue;
			// the list fades out over 56 px at an edge with more beyond it
			float fade = 1;
			if (above) fade = std::min(fade, std::max(0.f, (ry + rh / 2 - ev_y) / 56));
			if (below) fade = std::min(fade, std::max(0.f, (ev_y + ev_h - (ry + rh / 2)) / 56));
			float op = fade * slide * (e.state == "past" ? 0.42f : 1);
			Rect row{x0 + sx, ry, w, rh};
			if (e.state == "now") {
				gfx::rect(row, 20, {1, 0xc3 / 255.f, 0x6b / 255.f, 0.18f * op});
				gfx::ring({row.x + 2, row.y + 2, row.w - 4, row.h - 4}, 18, 2, {1, 0xc3 / 255.f, 0x6b / 255.f, 0.55f * op});
			} else gfx::rect(row, 20, {1, 1, 1, 0.07f * op});
			float cx = row.x + 14, cy = row.y + 12;
			float left_h = t1 * 1.15f + (e.note.empty() ? 0 : 4 + t3 * 1.2f), left_y = cy + (rh - 24 - left_h) / 2;
			put(e.time, time_st, cx, left_y, t1 * 1.15f, t.textPrimary, op);
			if (!e.note.empty()) put(e.note, note_st, cx, left_y + t1 * 1.15f + 4, t3 * 1.2f, e.state == "now" ? AMBER : t.textSecondary, op);
			cx += time_w + 16;
			gfx::rect({cx, row.y + 12, 5, rh - 24}, 3, Color::hex(CAL_COLORS[e.src % 6]).with_alpha(op));
			cx += 5 + 16;
			float body_w = row.x + row.w - 18 - cx;
			std::string place = e.place;
			if (many && e.src < (int)data->sources.size() && !data->sources[e.src].name.empty()) place += (place.empty() ? "" : " · ") + data->sources[e.src].name;
			float right_h = t1 * 1.2f + (place.empty() ? 0 : 4 + t2 * 1.2f), right_y = row.y + (rh - right_h) / 2;
			put(text::ellipsize(e.title, name_st, (int)body_w), name_st, cx, right_y, t1 * 1.2f, t.textPrimary, op);
			if (!place.empty()) put(text::ellipsize(place, place_st, (int)body_w), place_st, cx, right_y + t1 * 1.2f + 4, t2 * 1.2f, t.textSecondary, op);
		}
		gfx::pop_clip();
		if (events.empty()) {
			std::string empty = loading ? tr("Loading the calendar…", "Загружаю календарь…") : data ? tr("Nothing on this day.", "В этот день событий нет.") : tr("The calendar has not loaded yet.", "Календарь пока не загрузился.");
			put(empty, TextStyle{face, t1, 400, 0}, x0 + w / 2 + sx, ev_y + 70, t1 * 1.2f, t.textSecondary, slide, true);
		}
	}

	if (lt > 0) {
		// the month: the weekdays, then the months side by side sliding past
		TextStyle wd{face, t3, 400, 0}, num{face, t1, 500, 0}, today_num{face, t1, 600, 0};
		float cell = w / 7, wd_h = t3 + 16;
		for (int i = 0; i < 7; i++) put(day_short(lang_now, WEEKDAYS[i]), wd, x0 + i * cell + cell / 2, month_y + 6, (float)t3, t.textSecondary, 1, true);
		float grid_y = month_y + wd_h, grid_h = month_y + stage_h - grid_y, row_h = grid_h / 6;
		double shown = st.months.at(now);
		gfx::push_clip({x0, grid_y, w, grid_h});
		for (int m = st.month - 1; m <= st.month + 1; m++) {
			float px = x0 + (float)((m - shown) * w);
			if (px > x0 + w || px + w < x0) continue;
			const MonthView &mv = month_of(m);
			for (int r = 0; r < 6; r++)
				for (int c = 0; c < 7; c++) {
					const MonthCell &mc = mv.weeks[r][c];
					Rect box{px + c * cell + 2, grid_y + r * row_h + 2, cell - 4, row_h - 4};
					float op = mc.inMonth ? 1 : 0.3f;
					if (mc.selected) gfx::rect(box, 20, {1, 1, 1, 0.16f * op});
					float block = t1 * 1.1f + 12, by = box.y + (box.h - block) / 2;
					put(std::to_string(mc.day), mc.today ? today_num : num, box.x + box.w / 2, by, t1 * 1.1f, mc.today ? AMBER : t.textPrimary, op, true);
					dots(mc.dots, box.x + box.w / 2, by + t1 * 1.1f + 4, op);
				}
		}
		gfx::pop_clip();
	}
	gfx::pop_clip();
	(void)focus;
}

int calendar_hit (const BoardCard &card, const CalendarState &st, int card_size, float x, float y, int &days) {
	// the same places as draw_calendar's
	float k = card_size / 100.f;
	int t_title = (int)std::lround(26 * k), t1 = (int)std::lround(22 * k), t3 = (int)std::lround(14 * k);
	float x0 = card.x + 28.f, w = card.w - 56.f, y0 = card.y + 26.f;
	float head_h = std::max(52.f, t3 + 6 + t_title * 1.2f), ny = y0 + (head_h - 52) / 2;
	if (y >= ny && y <= ny + 52) {
		if (x >= x0 && x <= x0 + 52) return -1;
		if (x >= x0 + w - 52 && x <= x0 + w) return 1;
	}
	if (st.month_mode) return 0;
	float day_y = y0 + head_h + 14, strip_h = t3 + t1 * 1.2f + 42, cell = w / 7;
	if (y < day_y || y > day_y + strip_h || x < x0 || x > x0 + w) return 0;
	days = (int)std::floor((x - x0) / cell) - 3;
	return 2;
}
