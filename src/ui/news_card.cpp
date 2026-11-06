#include "ui/news_card.h"

#include "i18n/i18n.h"
#include "ui/theme.h"

#include <qrcodegen/qrcodegen.hpp>

#include <algorithm>
#include <cmath>
#include <memory>

namespace {

void put (const std::string &s, const TextStyle &st, float x, float top, float line, Color c, float opacity = 1) {
	const TextLine &l = text::line(s, st);
	text::draw(s, st, std::round(x), std::round(top + (line - l.h) / 2), c, opacity);
}

std::string join_meta (const std::string &a, const std::string &b) {
	if (a.empty()) return b;
	if (b.empty()) return a;
	return a + " · " + b;
}

struct QrCache { std::string link; TexturePtr tex; };
QrCache qr_cache;

// The QR code of the link, 300 px square, white with a margin of two squares, each pixel set exactly (no blur on the edges).
const Texture *qr_texture (const std::string &link) {
	if (qr_cache.link == link) return qr_cache.tex.get();
	qr_cache.link = link;
	qr_cache.tex.reset();
	try {
		auto qr = qrcodegen::QrCode::encodeText(link.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
		int n = qr.getSize(), total = n + 4, size = 300;
		Pixels p;
		p.w = p.h = size;
		p.rgba.assign((size_t)size * size * 4, 255);
		for (int y = 0; y < size; y++)
			for (int x = 0; x < size; x++) {
				int mx = x * total / size - 2, my = y * total / size - 2;
				if (mx >= 0 && my >= 0 && mx < n && my < n && qr.getModule(mx, my)) {
					unsigned char *o = &p.rgba[((size_t)y * size + x) * 4];
					o[0] = o[1] = o[2] = 0;
				}
			}
		qr_cache.tex = make_texture(p);
	} catch (...) {
		// too long for a QR code: none
	}
	return qr_cache.tex.get();
}

}  // namespace

std::string age_text (long long at, long long now) {
	auto age = age_of(at, now);
	if (age.second.empty()) return "";
	std::string n = std::to_string(age.first);
	if (age.second == "min") return fmt(tr("{0} min ago", "{0} мин назад"), n);
	if (age.second == "h") return fmt(tr("{0} h ago", "{0} ч назад"), n);
	return fmt(tr("{0} d ago", "{0} дн. назад"), n);
}

void draw_news (const BoardCard &card, const std::vector<FeedItem> *items, bool loading, int page, int selected, bool focused,
		int card_size, Face face, long long now) {
	const Theme &t = current_theme();
	float k = card_size / 100.f;
	int t_title = (int)std::lround(26 * k), t1 = (int)std::lround(22 * k), t2 = (int)std::lround(17 * k), t3 = (int)std::lround(14 * k);
	float x0 = card.x + 26.f, w = card.w - 52.f, y0 = card.y + 22.f, bottom = card.y + card.h - 22.f;
	int per = headlines_per_page(card.h, k);
	size_t count = items ? items->size() : 0;
	int pages = std::max(1, (int)((count + per - 1) / per));
	int p = std::min(page, pages - 1);

	// the head: "News" and the page number, on one baseline
	TextStyle title{face, t_title, 500, 0}, pages_st{face, t2, 400, 0};
	float head_h = 44 * k;
	const TextLine &tl = text::line(tr("News", "Новости"), title);
	float base = y0 + (t_title * 1.2f - tl.h) / 2 + tl.ascent;
	text::draw(tr("News", "Новости"), title, x0, std::round(base - tl.ascent), t.textPrimary);
	if (pages > 1) {
		std::string n = std::to_string(p + 1) + " / " + std::to_string(pages);
		const TextLine &pl = text::line(n, pages_st);
		text::draw(n, pages_st, std::round(x0 + w - pl.w), std::round(base - pl.ascent), t.textSecondary);
	}

	float y = y0 + head_h + 10;
	gfx::push_clip({x0, y, w, bottom - y});
	TextStyle meta{face, t3, 400, 0}, headline{face, t1, 500, 0};
	if (!count) {
		std::string empty = loading ? tr("Loading the news…", "Загружаю новости…") : items ? tr("No fresh headlines.", "Свежих новостей нет.") : tr("The news has not loaded yet.", "Новости пока не загрузились.");
		auto lines = text::wrap(empty, TextStyle{face, t1, 400, 0}, (int)w, 3);
		for (size_t i = 0; i < lines.size(); i++) {
			const TextLine &l = text::line(lines[i], TextStyle{face, t1, 400, 0});
			put(lines[i], TextStyle{face, t1, 400, 0}, x0 + (w - l.w) / 2, y + 70 + i * t1 * 1.2f, t1 * 1.2f, t.textSecondary);
		}
	}
	for (int i = 0; i < per; i++) {
		size_t index = (size_t)(p * per + i);
		if (index >= count) break;
		const FeedItem &it = (*items)[index];
		bool sel = focused && (int)index == selected;
		auto lines = text::wrap(it.title, headline, (int)(w - 28), 2);
		float h = 16 + 14 + t3 * 1.35f + 6 + lines.size() * t1 * 1.3f;
		bool line_above = i > 0 && !sel && !(focused && (int)index - 1 == selected);
		if (line_above) gfx::rect({x0 + 18, y, w - 36, 1.5f}, 0, {1, 1, 1, 0.1f});
		if (sel) gfx::rect({x0, y, w, h}, 18, {1, 1, 1, 0.16f});
		put(text::ellipsize(join_meta(it.source, age_text(it.at, now)), meta, (int)(w - 28)), meta, x0 + 14, y + 16, t3 * 1.35f, t.textSecondary);
		for (size_t l = 0; l < lines.size(); l++) put(lines[l], headline, x0 + 14, y + 16 + t3 * 1.35f + 6 + l * t1 * 1.3f, t1 * 1.3f, t.textPrimary);
		y += h;
	}
	gfx::pop_clip();
}

void draw_news_detail (const FeedItem &item, Face face, long long now, float fade) {
	const Theme &t = current_theme();
	gfx::set_alpha(fade);
	gfx::rect({0, 0, 1920, 1080}, 0, {4 / 255.f, 6 / 255.f, 18 / 255.f, 0.72f});
	const Texture *qr = item.link.empty() ? nullptr : qr_texture(item.link);
	float card_w = 1320, pad_x = 56, pad_y = 48, text_w = card_w - 2 * pad_x - (qr ? 300 + 56 : 0);
	TextStyle meta{face, 22, 400, 0}, title{face, 44, 500, 0}, summary{face, 24, 400, 0}, hint{face, 20, 400, 0}, scan{face, 20, 400, 0};
	auto title_lines = text::wrap(item.title, title, (int)text_w, 4);
	auto summary_lines = item.summary.empty() ? std::vector<std::string>() : text::wrap(item.summary, summary, (int)text_w, 6);
	float text_h = 22 * 1.2f + 14 + title_lines.size() * 44 * 1.25f + (summary_lines.empty() ? 0 : 22 + summary_lines.size() * 24 * 1.45f) + 30 + 20 * 1.2f;
	auto scan_lines = text::wrap(tr("Scan to read it on your phone", "Наведите камеру телефона, чтобы дочитать"), scan, 300, 3);
	float qr_h = qr ? 300 + 16 + scan_lines.size() * 20 * 1.35f : 0;
	float inner = std::max(text_h, qr_h), card_h = inner + 2 * pad_y;
	Rect card{(1920 - card_w) / 2, (1080 - card_h) / 2, card_w, card_h};
	gfx::shadow(card, 36, 34, 70, {0, 0, 0, 0.55f});
	gfx::rect(card, 36, t.tileBg);
	float x = card.x + pad_x, y = card.y + pad_y + (inner - text_h) / 2;
	put(join_meta(item.source, age_text(item.at, now)), meta, x, y, 22 * 1.2f, t.textSecondary);
	y += 22 * 1.2f + 14;
	for (auto &l : title_lines) { put(l, title, x, y, 44 * 1.25f, t.textPrimary); y += 44 * 1.25f; }
	if (!summary_lines.empty()) {
		y += 22;
		for (auto &l : summary_lines) { put(l, summary, x, y, 24 * 1.45f, t.textSecondary); y += 24 * 1.45f; }
	}
	y += 30;
	put(tr("OK or Back closes", "OK или «Назад» — закрыть"), hint, x, y, 20 * 1.2f, t.textMuted);
	if (qr) {
		float qx = card.x + card_w - pad_x - 300, qy = card.y + pad_y + (inner - qr_h) / 2;
		gfx::image(*qr, {qx, qy, 300, 300}, 1, 16);
		float sy = qy + 316;
		for (auto &l : scan_lines) {
			const TextLine &tl = text::line(l, scan);
			put(l, scan, qx + (300 - tl.w) / 2, sy, 20 * 1.35f, t.textSecondary);
			sy += 20 * 1.35f;
		}
	}
	gfx::set_alpha(1);
}

void draw_quote (const BoardCard &card, const Quote *q, bool loading, int card_size, Face face) {
	const Theme &t = current_theme();
	float k = card_size / 100.f;
	int t1 = (int)std::lround(22 * k), t2 = (int)std::lround(17 * k);
	float x0 = card.x + 30.f, w = card.w - 60.f, y0 = card.y + 20.f, h = card.h - 40.f;
	if (!q) {
		std::string waiting = loading ? tr("Loading the quote of the day…", "Загружаю фразу дня…") : tr("The quote of the day has not loaded yet.", "Фраза дня пока не загрузилась.");
		TextStyle st{face, t1, 400, 0};
		auto lines = text::wrap(waiting, st, (int)w, 3);
		float lh = t1 * 1.3f, y = y0 + (h - lines.size() * lh) / 2;
		for (auto &l : lines) { put(l, st, x0, y, lh, t.textSecondary); y += lh; }
		return;
	}
	// the biggest size at which the text and the author fit: from the largest down, two pixels at a time (worked out once per
	// quote and card size)
	static std::string fitted_key;
	static int fitted_font = 0;
	static std::vector<std::string> fitted_lines;
	float by_h = 8 + t2 * 1.2f;
	std::string key = q->text + "|" + std::to_string(card.w) + "x" + std::to_string(card.h) + "|" + std::to_string(card_size) + "|" + std::to_string((int)face);
	TextStyle st{face, 16, 300, 0, true};
	if (key != fitted_key) {
		int font = (int)std::lround(std::min(44 * k, card.h * 0.34f));
		std::vector<std::string> lines;
		for (;; font -= 2) {
			st.size = font;
			lines = text::wrap(q->text, st, (int)w, 1000);
			if (lines.size() * font * 1.3f + by_h <= h + 1 || font <= 16) break;
		}
		if (lines.size() * font * 1.3f + by_h > h + 1) lines = text::wrap(q->text, st, (int)w, 3);      // still too long: three lines and "…"
		fitted_key = key;
		fitted_font = font;
		fitted_lines = lines;
	}
	int font = fitted_font;
	st.size = font;
	const std::vector<std::string> &lines = fitted_lines;
	float lh = font * 1.3f, block = lines.size() * lh + by_h, y = y0 + std::max(0.f, (h - block) / 2);
	gfx::push_clip({(float)card.x, (float)card.y, (float)card.w, (float)card.h});
	for (auto &l : lines) { put(l, st, x0, y, lh, t.textPrimary); y += lh; }
	TextStyle by{face, t2, 400, 0};
	put(text::ellipsize(q->author + " · Wikiquote", by, (int)w), by, x0, y + 8, t2 * 1.2f, t.textSecondary);
	gfx::pop_clip();
}

int news_item_at (const BoardCard &card, const std::vector<FeedItem> *items, int page, int card_size, Face face, float x, float y) {
	// the same places as draw_news's
	if (!items || items->empty()) return -1;
	float k = card_size / 100.f;
	int t1 = (int)std::lround(22 * k), t3 = (int)std::lround(14 * k);
	float x0 = card.x + 26.f, w = card.w - 52.f, y0 = card.y + 22.f, bottom = card.y + card.h - 22.f;
	if (x < x0 || x > x0 + w) return -1;
	int per = headlines_per_page(card.h, k);
	int pages = std::max(1, (int)((items->size() + per - 1) / per));
	int p = std::min(page, pages - 1);
	TextStyle headline{face, t1, 500, 0};
	float top = y0 + 44 * k + 10;
	for (int i = 0; i < per; i++) {
		size_t index = (size_t)(p * per + i);
		if (index >= items->size() || top > bottom) break;
		auto lines = text::wrap((*items)[index].title, headline, (int)(w - 28), 2);
		float h = 16 + 14 + t3 * 1.35f + 6 + lines.size() * t1 * 1.3f;
		if (y >= top && y < std::min(top + h, bottom)) return (int)index;
		top += h;
	}
	return -1;
}
