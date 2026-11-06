#include "ui/pickers.h"

#include "core/tasks.h"
#include "data/apps.h"
#include "data/cities.h"
#include "data/feeds.h"
#include "data/weather.h"
#include "gfx/text.h"
#include "i18n/i18n.h"
#include "sys/fs.h"
#include "sys/luna.h"
#include "ui/anim.h"
#include "ui/icons.h"
#include "ui/theme.h"
#include "ui/tokens.h"

#include <SDL.h>

#include <algorithm>
#include <cmath>

namespace {

const int VISIBLE_ROWS = 9;
const size_t MAX_SOURCES = 8, MAX_FEEDS = 8;
const std::string CALENDAR_FILE = "/media/developer/calendar.txt";
const std::string CALENDAR_FOLDER = "/media/developer/cals/";

// .screen--picker .settings: a sheet 1240 px wide in the middle; .picker__row 84 px
const float SHEET_X = (1920 - 1240) / 2.f, SHEET_W = 1240, SHEET_Y = 84;
const float ROW_H = 84, BOX_PAD = 6;
const Color PILL = Color::hex("#F2F2F7"), ON_PILL = Color::hex("#111216"), ON_PILL_DETAIL = Color::hex("#3C3F47"), PLACEHOLDER = Color::hex("#8A8D96");

void put (const std::string &s, const TextStyle &st, float x, float top, float line, Color c, float opacity = 1) {
	const TextLine &l = text::line(s, st);
	text::draw(s, st, std::round(x), std::round(top + (line - l.h) / 2), c, opacity);
}

Color mix (Color a, Color b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t}; }

int clampi (int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); }

std::string trim (const std::string &s) {
	size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
	return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

/** lib/calendar.ts normalizeUrl: no spaces, webcal:// read as https:// */
std::string normalize_url (const std::string &raw) {
	std::string url;
	for (char c : raw) if (!isspace((unsigned char)c)) url += c;
	std::string low = url.substr(0, 9);
	for (auto &c : low) c = (char)tolower((unsigned char)c);
	if (low == "webcal://") url = "https://" + url.substr(9);
	return url;
}

/** A short name for a calendar in the list. */
std::string calendar_name (const std::string &src) {
	if (!src.empty() && src[0] == '/') return src;
	std::string host = src;
	size_t p = src.find("://");
	if (p != std::string::npos) {
		size_t end = src.find('/', p + 3);
		host = src.substr(p + 3, end == std::string::npos ? std::string::npos : end - p - 3);
	}
	return host + " …" + (src.size() > 8 ? src.substr(src.size() - 8) : src);
}

/** A short name for a feed in the list: its site and the end of its address. */
std::string feed_name (const std::string &url) {
	size_t p = url.find("://");
	if (p == std::string::npos) return url;
	size_t slash = url.find('/', p + 3);
	std::string host = url.substr(p + 3, slash == std::string::npos ? std::string::npos : slash - p - 3);
	std::string rest = slash == std::string::npos ? "" : url.substr(slash);
	if (rest.size() > 24) rest = "…" + rest.substr(rest.size() - 22);
	return host + rest;
}

/** The last UTF-8 character of `s` taken off. */
void pop_char (std::string &s) {
	while (!s.empty()) {
		unsigned char c = (unsigned char)s.back();
		s.pop_back();
		if ((c & 0xC0) != 0x80) break;
	}
}

}  // namespace

void Picker::open (Kind kind, const std::string &current, const std::vector<std::string> &list, bool can_read_files, double now) {
	kind_ = kind;
	current_ = current;
	list_ = list;
	can_read_ = can_read_files;
	typing_ = false;
	typed_.clear();
	note_.clear();
	searching_ = busy_ = failed_ = false;
	results_.clear();
	index_ = 0;
	focus_at_ = now;
	if (kind == Kind::country) {
		countries_ = news_countries_by_name(lang());
		auto it = std::find(countries_.begin(), countries_.end(), current);
		index_ = it == countries_.end() ? 0 : (int)(it - countries_.begin());
	}
}

std::vector<Picker::Row> Picker::rows () const {
	std::vector<Row> out;
	if (kind_ == Kind::city) {
		if (!searching_) {
			out.push_back({"search", "search", tr("Search for a city…", "Найти город…"), tr("Type the name with the on-screen keyboard", "Введите название на экранной клавиатуре")});
			auto cities = preset_cities();
			for (size_t i = 0; i < cities.size(); i++) out.push_back({"city", "pin", cities[i].name, cities[i].detail, (int)i});
		} else {
			out.push_back({"search", "search", tr("Search again…", "Искать снова…"), ""});
			out.push_back({"back", "folder", tr("Back to the list", "Назад к списку"), ""});
			if (busy_) out.push_back({"info", "info", tr("Searching…", "Поиск…"), ""});
			else if (failed_) out.push_back({"info", "info", tr("The search did not work", "Не удалось выполнить поиск"), tr("Check the TV's internet connection and try again", "Проверьте подключение ТВ к интернету и повторите")});
			else if (results_.empty()) out.push_back({"info", "info", tr("Nothing found", "Ничего не найдено"), tr("Try another spelling, or just the city name", "Попробуйте другое написание или только название города")});
			for (size_t i = 0; i < results_.size(); i++) out.push_back({"city", "pin", results_[i].name, results_[i].detail, (int)i});
		}
	} else if (kind_ == Kind::country) {
		for (size_t i = 0; i < countries_.size(); i++)
			out.push_back({"country", "pin", country_name(countries_[i], lang_tag()) + (countries_[i] == current_ ? " ✓" : ""), edition_language(countries_[i], lang()), (int)i});
	} else if (kind_ == Kind::calendars) {
		out.push_back({"type", "search", tr("Add a link…", "Добавить ссылку…"), tr("The secret address in iCal format from Google Calendar, typed with the on-screen keyboard", "Закрытый адрес в формате iCal из Google Календаря. Печатается экранной клавиатурой")});
		if (can_read_) {
			out.push_back({"folder", "folder", tr("Add a folder of calendars", "Добавить папку с календарями"), fmt(tr("Every .ics file in {0}", "Все файлы .ics из {0}"), CALENDAR_FOLDER)});
			out.push_back({"file", "drive", tr("Read the list from a file", "Взять список из файла"), fmt(tr("One calendar per line, in {0}", "По одному календарю в строке, в файле {0}"), CALENDAR_FILE)});
		}
		if (!list_.empty()) out.push_back({"clear", "folder", tr("Remove all calendars", "Убрать все календари"), tr("The calendar card leaves the home screen", "Календарь на главном экране исчезнет")});
		if (!note_.empty()) out.push_back({"info", "info", note_, ""});
		for (size_t i = 0; i < list_.size(); i++) out.push_back({"source", "picture", calendar_name(list_[i]), "", (int)i});
	} else if (kind_ == Kind::feeds) {
		out.push_back({"type", "search", tr("Add a feed…", "Добавить ленту…"), tr("The address of an RSS or Atom feed, typed with the on-screen keyboard", "Адрес RSS- или Atom-ленты. Печатается экранной клавиатурой")});
		if (!list_.empty()) out.push_back({"clear", "folder", tr("Remove all feeds", "Убрать все ленты"), tr("Only Google News stays, if it is on", "Останутся только Google Новости, если они включены")});
		if (!note_.empty()) out.push_back({"info", "info", note_, ""});
		for (size_t i = 0; i < list_.size(); i++) out.push_back({"feed", "picture", feed_name(list_[i]), tr("OK removes it", "OK убирает её"), (int)i});
	}
	return out;
}

void Picker::start_typing () {
	typing_ = true;
	typed_.clear();
	SDL_StartTextInput();          // the TV's on-screen keyboard
}

void Picker::stop_typing () {
	typing_ = false;
	SDL_StopTextInput();
}

void Picker::run_search (const std::string &text) {
	std::string q = trim(text);
	if (q.size() < 2) return;
	searching_ = true;
	busy_ = true;
	failed_ = false;
	results_.clear();
	index_ = 0;
	int id = ++search_id_;
	run_background([this, q, id] {
		bool ok = true;
		auto list = search_cities(q, &ok);
		post_main([this, list, ok, id] {
			if (id != search_id_ || kind_ != Kind::city) return;     // another search, or the sheet closed
			results_ = list;
			busy_ = false;
			failed_ = !ok;
			index_ = list.empty() ? 0 : 2;
		});
	});
}

bool Picker::add_source (const std::string &raw) {
	std::string src = !raw.empty() && raw[0] == '/' ? trim(raw) : normalize_url(raw);
	if (!valid_calendar_source(src)) { note_ = tr("That does not look like a link. It should start with https://", "Это не похоже на ссылку. Она должна начинаться с https://"); return false; }
	if (std::find(list_.begin(), list_.end(), src) != list_.end()) { note_ = tr("This calendar is already on the list", "Этот календарь уже подключён"); return false; }
	if (list_.size() >= MAX_SOURCES) { note_ = fmt(tr("No more than {0} calendars", "Не больше {0} календарей"), std::to_string(MAX_SOURCES)); return false; }
	auto next = list_;
	next.push_back(src);
	if (on_calendars) on_calendars(next);
	return true;
}

bool Picker::add_feed (const std::string &raw) {
	std::string url = normalize_feed_url(raw);
	if (!valid_feed_url(url)) { note_ = tr("That does not look like a link. It should start with https://", "Это не похоже на ссылку. Она должна начинаться с https://"); return false; }
	if (std::find(list_.begin(), list_.end(), url) != list_.end()) { note_ = tr("This feed is already on the list", "Эта лента уже добавлена"); return false; }
	if (list_.size() >= MAX_FEEDS) { note_ = fmt(tr("No more than {0} feeds", "Не больше {0} лент"), std::to_string(MAX_FEEDS)); return false; }
	note_.clear();
	auto next = list_;
	next.push_back(url);
	if (on_feeds) on_feeds(next);
	return true;
}

void Picker::read_file () {
	note_ = tr("Reading the file…", "Читаю файл…");
	run_background([this] {
		bool ok = false;
		std::string text = hbc_exec("head -c 4000 " + shell_quote(CALENDAR_FILE), 8000, &ok);
		post_main([this, text, ok] {
			if (kind_ != Kind::calendars) return;
			if (!ok) { note_ = tr("Could not read the file. This needs a rooted TV with the Homebrew Channel", "Не удалось прочитать файл. Нужен рутованный ТВ с Homebrew Channel"); return; }
			std::vector<std::string> list;
			size_t at = 0;
			while (at <= text.size() && list.size() < MAX_SOURCES) {
				size_t nl = text.find('\n', at);
				std::string line = trim(text.substr(at, nl == std::string::npos ? std::string::npos : nl - at));
				at = nl == std::string::npos ? text.size() + 1 : nl + 1;
				if (line.empty() || line[0] == '#') continue;
				std::string src = line[0] == '/' ? line : normalize_url(line);
				if (valid_calendar_source(src)) list.push_back(src);
			}
			if (list.empty()) { note_ = fmt(tr("{0} has no links in it, or is empty", "В файле {0} нет ссылок или он пуст"), CALENDAR_FILE); return; }
			note_.clear();
			if (on_calendars) on_calendars(list);
		});
	});
}

void Picker::activate (int i) {
	auto r = rows();
	if (i < 0 || i >= (int)r.size()) return;
	const Row &row = r[i];
	index_ = i;
	if (row.key == "search" || row.key == "type") start_typing();
	else if (row.key == "back") { searching_ = false; results_.clear(); index_ = 0; search_id_++; }
	else if (row.key == "city") {
		auto cities = searching_ ? results_ : preset_cities();
		if (row.index < (int)cities.size() && on_city) on_city(cities[row.index]);
	} else if (row.key == "country") { if (on_country) on_country(countries_[row.index]); }
	else if (row.key == "file") read_file();
	else if (row.key == "folder") add_source(CALENDAR_FOLDER);
	else if (row.key == "clear") {
		if (kind_ == Kind::calendars && on_calendars) on_calendars({});
		if (kind_ == Kind::feeds && on_feeds) on_feeds({});
	} else if (row.key == "source") {
		std::vector<std::string> next;
		for (size_t n = 0; n < list_.size(); n++) if ((int)n != row.index) next.push_back(list_[n]);
		if (on_calendars) on_calendars(next);
	} else if (row.key == "feed") {
		std::vector<std::string> next;
		for (size_t n = 0; n < list_.size(); n++) if ((int)n != row.index) next.push_back(list_[n]);
		if (on_feeds) on_feeds(next);
		index_ = std::max(0, index_ - 1);
	}
}

void Picker::submit () {
	std::string value = typed_;
	if (kind_ == Kind::city) { stop_typing(); run_search(value); return; }
	if (kind_ == Kind::calendars) {
		if (!trim(value).empty() && add_source(value)) { typed_.clear(); return; }      // the field stays open for the next link
		stop_typing();
		return;
	}
	if (kind_ == Kind::feeds) {
		if (!trim(value).empty() && add_feed(value)) { stop_typing(); return; }
		if (trim(value).empty()) stop_typing();
	}
}

void Picker::text_input (const std::string &text) {
	if (!typing_) return;
	size_t limit = kind_ == Kind::city ? 60 : 600;
	for (char c : text) if (c == '\n' || c == '\r') { submit(); return; }
	if (typed_.size() + text.size() <= limit) typed_ += text;
}

bool Picker::key_down (int sc, bool repeat, double now) {
	(void)repeat;
	bool back = sc == SDL_SCANCODE_WEBOS_BACK || sc == SDL_SCANCODE_ESCAPE;
	bool ok = sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER;
	if (typing_) {
		// the text field: OK (the keyboard's Enter) takes it, Back leaves it
		if (ok) submit();
		else if (back) stop_typing();
		else if (sc == SDL_SCANCODE_BACKSPACE) pop_char(typed_);
		return true;
	}
	int n = (int)rows().size();
	int before = index_;
	if (sc == SDL_SCANCODE_UP) index_ = clampi(index_ - 1, 0, std::max(0, n - 1));
	else if (sc == SDL_SCANCODE_DOWN) index_ = clampi(index_ + 1, 0, std::max(0, n - 1));
	else if (ok) activate(index_);
	else if (back) {
		if (kind_ == Kind::city && searching_) { searching_ = false; results_.clear(); index_ = 0; search_id_++; }
		else if (on_close) on_close();
	}
	if (index_ != before) focus_at_ = now;
	return true;
}

bool Picker::animating (double now) const { return now - focus_at_ < motion::FOCUS_MS; }

void Picker::draw (double now) {
	const Theme &t = theme_named(theme_);
	gfx::rect({0, 0, 1920, 1080}, 0, t.screen.with_alpha(t.screen.a * SCREEN_OPACITY));      // the wallpaper a little through it
	std::string title, path;
	if (kind_ == Kind::city) {
		title = tr("Choose a city", "Выберите город");
		path = !searching_ ? tr("Popular cities", "Популярные города") : tr("Search results", "Результаты поиска");
	} else if (kind_ == Kind::country) {
		title = tr("Country of the news", "Страна новостей");
		path = fmt(tr("Google News editions: {0}", "Редакций Google Новостей: {0}"), std::to_string(countries_.size()));
	} else if (kind_ == Kind::calendars) {
		title = tr("Calendars", "Календари");
		path = !list_.empty() ? fmt(tr("Connected: {0}", "Подключено: {0}"), std::to_string(list_.size())) : tr("No calendars yet", "Календарей пока нет");
	} else {
		title = tr("Your feeds", "Свои ленты");
		path = !list_.empty() ? fmt(tr("Added: {0}", "Добавлено: {0}"), std::to_string(list_.size())) : tr("No feeds of your own yet", "Своих лент пока нет");
	}
	put(title, TextStyle{Face::ui, 46, 600, -0.46f}, SHEET_X, SHEET_Y, 56, t.textPrimary);
	float path_y = SHEET_Y + 56 + 34;
	TextStyle pst{Face::ui, 19, 600, 19 * 0.08f};
	put(text::ellipsize(text::upper(path), pst, (int)SHEET_W - 26), pst, SHEET_X + 26, path_y, 24, t.textMuted);

	auto r = rows();
	int n = (int)r.size();
	index_ = clampi(index_, 0, std::max(0, n - 1));
	int start = clampi(index_ - VISIBLE_ROWS / 2, 0, std::max(0, n - VISIBLE_ROWS));
	int end = std::min(n, start + VISIBLE_ROWS);
	float box_y = path_y + 24 + 12;
	gfx::rect({SHEET_X, box_y, SHEET_W, (end - start) * ROW_H + 2 * BOX_PAD}, 28, {1, 1, 1, 0.075f});
	float f = (float)ease(Ease::ease_out, std::min(1.0, (now - focus_at_) / motion::FOCUS_MS));
	TextStyle lst{Face::ui, 26, 400, 0}, dst{Face::ui, 20, 400, 0}, info{Face::ui, 23, 400, 0};
	for (int i = start; i < end; i++) {
		const Row &row = r[i];
		Rect box{SHEET_X + BOX_PAD, box_y + BOX_PAD + (i - start) * ROW_H, SHEET_W - 2 * BOX_PAD, ROW_H};
		bool focused = i == index_;
		bool is_info = row.key == "info";
		float pill = focused ? f : 0;
		if (i > start && !focused && i - 1 != index_) gfx::rect({box.x + 92, box.y, box.w - 92 - 24, 1}, 0, {1, 1, 1, 0.09f});
		float opacity = is_info ? 0.8f : 1;
		if (pill > 0 && !is_info) gfx::rect(box, 22, PILL.with_alpha(pill));
		Color icon_c = mix(t.textSecondary, ON_PILL, is_info ? 0 : pill);
		if (TexturePtr g = icons::get(join_path(package_dir(), "glyphs/" + row.icon + ".png"), 40))
			gfx::image(*g, {box.x + 28, box.y + (ROW_H - 40) / 2, 40, 40}, opacity, 0, icon_c);
		float tx = box.x + 28 + 40 + 28, tw = box.x + box.w - 28 - tx;
		bool editing = typing_ && focused && (row.key == "search" || row.key == "type");
		Color label_c = mix(t.textPrimary, ON_PILL, pill), detail_c = mix(t.textSecondary, ON_PILL_DETAIL, pill);
		if (editing) {
			// the text field on the light pill: dark text, a grey placeholder, and the caret after the text
			std::string placeholder = kind_ == Kind::city ? tr("City name", "Название города") : kind_ == Kind::feeds ? "https://example.com/rss.xml" : "https://calendar.google.com/calendar/ical/…";
			float ty = box.y + (ROW_H - 34) / 2;
			float caret_x = tx;
			if (typed_.empty()) put(placeholder, lst, tx, ty, 34, PLACEHOLDER, opacity);
			else {
				// the end of a long text stays in view
				std::string shown = typed_;
				while (!shown.empty() && text::width(shown, lst) > tw - 8) {
					size_t k = 1;
					while (k < shown.size() && ((unsigned char)shown[k] & 0xC0) == 0x80) k++;
					shown.erase(0, k);
				}
				put(shown, lst, tx, ty, 34, ON_PILL);
				caret_x = tx + text::width(shown, lst) + 2;
			}
			gfx::rect({std::round(caret_x), ty + 3, 2, 28}, 0, ON_PILL);
			continue;
		}
		if (is_info) {
			auto lines = text::wrap(row.label, info, (int)tw, 2);
			float h = lines.size() * 30.f + (row.detail.empty() ? 0 : 4 + 26);
			float y = box.y + (ROW_H - h) / 2;
			for (auto &l : lines) { put(l, info, tx, y, 30, t.textSecondary, opacity); y += 30; }
			if (!row.detail.empty()) put(text::ellipsize(row.detail, dst, (int)tw), dst, tx, y + 4, 26, t.textSecondary, opacity);
			continue;
		}
		float h = 34 + (row.detail.empty() ? 0 : 4 + 26);
		float y = box.y + (ROW_H - h) / 2;
		put(text::ellipsize(row.label, lst, (int)tw), lst, tx, y, 34, label_c);
		if (!row.detail.empty()) put(text::ellipsize(row.detail, dst, (int)tw), dst, tx, y + 34 + 4, 26, detail_c);
	}
}
