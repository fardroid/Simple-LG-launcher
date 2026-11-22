#include "ui/notices.h"

#include "data/apps.h"
#include "gfx/text.h"
#include "sys/fs.h"
#include "ui/card.h"
#include "ui/icons.h"
#include "ui/theme.h"
#include "ui/wallpaper.h"

#include <algorithm>
#include <cmath>

namespace {
const double IN_MS = 350, OUT_MS = 350, MOVE_MS = 350;
const float TOP = 26, GAP = 14;                   // the first one's top, and the space between two
const size_t MAX_SHOWN = 3;                       // more wait for a place
const float PAD = 34, PAD_V = 24, ICON = 60, ICON_GAP = 24, MAX_TEXT = 1100, TIMER = 46, MIN_H = 100;
const float TITLE_LINE = 34, TEXT_LINE = 27, LINES_GAP = 6;
const TextStyle TITLE{Face::ui, 28, 500, 0}, TEXT{Face::ui, 20, 400, 0};

double hold_of (NoticeKind k) {
	switch (k) {
		case NoticeKind::update: case NoticeKind::restart: return 8000;
		case NoticeKind::error: return 7000;
		default: return 5000;
	}
}

// A ring of 3 px around (cx, cy) at `radius` (its middle line), and over it the part `left` (0-1) of it in white, from the top
// clockwise: small round dots close together (the GPU draws rounded rectangles only).
void draw_timer (float cx, float cy, float radius, float left) {
	const float W = 3;
	float in = radius - W / 2;
	gfx::ring({cx - in, cy - in, 2 * in, 2 * in}, in, W, {1, 1, 1, 0.18f});
	if (left <= 0) return;
	float arc = 2 * (float)M_PI * left;
	int n = std::max(2, (int)std::ceil(arc * radius / 0.75f));       // a dot every 0.75 px: their overlaps make it one line
	for (int i = 0; i <= n; i++) {
		float a = -(float)M_PI / 2 + arc * i / n;
		gfx::rect({cx + radius * std::cos(a) - W / 2, cy + radius * std::sin(a) - W / 2, W, W}, W / 2, {1, 1, 1, 0.32f});
	}
}
}  // namespace

Color notice_color (NoticeKind k) {
	switch (k) {
		case NoticeKind::done: case NoticeKind::update: return Color::hex("#30D158", 0.85f);
		case NoticeKind::error: case NoticeKind::restart: return Color::hex("#FF453A", 0.9f);
		default: return Color::hex("#0A84FF", 0.9f);
	}
}

const char *notice_glyph (NoticeKind k) {
	switch (k) {
		case NoticeKind::done: return "check-bold";
		case NoticeKind::error: return "note-alert";
		case NoticeKind::download: case NoticeKind::update: return "download";
		case NoticeKind::restart: return "restart";
		default: return "note-info";
	}
}

void round_icon (Rect r, Color c, const char *glyph) {
	gfx::rect(r, r.w / 2, c);
	float g = std::round(r.w * 0.56f);
	if (TexturePtr t = icons::get(join_path(package_dir(), std::string("glyphs/") + glyph + ".png"), 40))
		gfx::image(*t, {std::round(r.x + (r.w - g) / 2), std::round(r.y + (r.h - g) / 2), g, g});
}

double Notices::gone_at (const Item &it) const { return it.at + IN_MS + it.hold + OUT_MS; }

void Notices::measure (Item &it) const {
	it.title_lines = text::wrap(it.title, TITLE, (int)MAX_TEXT, 2);
	it.text_lines = it.text.empty() ? std::vector<std::string>() : text::wrap(it.text, TEXT, (int)MAX_TEXT, 2);
	float widest = 0;
	for (auto &l : it.title_lines) widest = std::max(widest, (float)text::line(l, TITLE).w);
	for (auto &l : it.text_lines) widest = std::max(widest, (float)text::line(l, TEXT).w);
	it.w = 2 * std::ceil((PAD + ICON + ICON_GAP + widest + 2 * ICON_GAP + TIMER + PAD) / 2);
	float block = it.title_lines.size() * TITLE_LINE + (it.text_lines.empty() ? 0 : LINES_GAP + it.text_lines.size() * TEXT_LINE);
	it.h = std::max(MIN_H, std::round(block + 2 * PAD_V));
}

void Notices::show (NoticeKind kind, const std::string &title, const std::string &text, double now) {
	// the same one again: it stays longer (shown fully, its ring full again)
	for (auto &it : items_)
		if (it.title == title && it.text == text) { it.at = now - IN_MS; return; }
	for (auto &it : waiting_)
		if (it.title == title && it.text == text) return;
	Item it;
	it.kind = kind;
	it.title = title;
	it.text = text;
	it.hold = hold_of(kind);
	measure(it);
	waiting_.push_back(it);
	settle(now);
}

bool Notices::animating (double now) const {
	(void)now;
	return !items_.empty() || !waiting_.empty();
}

void Notices::settle (double now) {
	items_.erase(std::remove_if(items_.begin(), items_.end(), [&] (const Item &it) { return now >= gone_at(it); }), items_.end());
	while (items_.size() < MAX_SHOWN && !waiting_.empty()) {
		Item it = waiting_.front();
		waiting_.pop_front();
		it.at = now;
		it.y.jump(TOP);
		items_.insert(items_.begin(), it);
	}
	float y = TOP;
	for (auto &it : items_) {
		if (it.y.to != y) it.y.set(y, now, MOVE_MS, Ease::ease_in_out);
		y += it.h + GAP;
	}
}

void Notices::draw (double now, const Wallpaper *wp, const UserConfig &cfg, const PlateLook &look, const PlateLook &old_look) {
	settle(now);
	const Theme &th = theme_named(cfg.theme);
	// the oldest first, so that a new one coming in at the top is over the ones moving down from under it
	for (auto i = items_.rbegin(); i != items_.rend(); ++i) {
		const Item &it = *i;
		double t = now - it.at;
		float k = t < IN_MS ? (float)ease(Ease::ease_out, t / IN_MS)
			: t > IN_MS + it.hold ? 1 - (float)ease(Ease::ease_in_out, std::min(1.0, (t - IN_MS - it.hold) / OUT_MS)) : 1;
		if (k <= 0) continue;
		Rect r{(1920 - it.w) / 2, std::round((float)it.y.at(now) - 24 * (1 - k)), it.w, it.h};
		gfx::set_alpha(k);
		gfx::shadow(r, card_radius(cfg), 30, 80, {0, 0, 0, 0.5f});
		// under the glass, so that what is under the banner never shows through (with Glass off the plate's colour alone is not
		// opaque)
		gfx::rect(r, card_radius(cfg), th.bg);
		if (wp) draw_plate(r, *wp, cfg, look, old_look, 0, wp->change(now));
		round_icon({r.x + PAD, std::round(r.y + (it.h - ICON) / 2), ICON, ICON}, notice_color(it.kind), notice_glyph(it.kind));
		float tx = r.x + PAD + ICON + ICON_GAP;
		float block = it.title_lines.size() * TITLE_LINE + (it.text_lines.empty() ? 0 : LINES_GAP + it.text_lines.size() * TEXT_LINE);
		float ly = std::round(r.y + (it.h - block) / 2);
		for (auto &l : it.title_lines) {
			const TextLine &tl = text::line(l, TITLE);
			text::draw(l, TITLE, tx, std::round(ly + (TITLE_LINE - tl.h) / 2), {1, 1, 1, 0.96f});
			ly += TITLE_LINE;
		}
		if (!it.text_lines.empty()) ly += LINES_GAP;
		for (auto &l : it.text_lines) {
			const TextLine &bl = text::line(l, TEXT);
			text::draw(l, TEXT, tx, std::round(ly + (TEXT_LINE - bl.h) / 2), {1, 1, 1, 0.66f});
			ly += TEXT_LINE;
		}
		// the ring: the time left, running down while the banner stays
		float left = 1 - (float)std::min(1.0, std::max(0.0, (t - IN_MS) / it.hold));
		draw_timer(r.x + it.w - PAD - TIMER / 2, r.y + it.h / 2, TIMER / 2 - 1.5f, left);
		gfx::set_alpha(1);
	}
}
