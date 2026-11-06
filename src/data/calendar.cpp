#include "data/calendar.h"

#include "core/log.h"
#include "data/config.h"
#include "i18n/dates.h"
#include "i18n/i18n.h"
#include "sys/fs.h"
#include "sys/http.h"
#include "sys/luna.h"

#include <nlohmann/json.hpp>

#include <sys/stat.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <map>
#include <regex>

using json = nlohmann::json;

const char *CAL_COLORS[6] = {"#ffc36b", "#6fb4ff", "#ff8fb1", "#6fe3a0", "#c39bff", "#ff9d6b"};

namespace {

std::tm local (long long ms) {
	std::time_t t = (std::time_t)(ms / 1000 - (ms % 1000 < 0));
	std::tm tm;
	localtime_r(&t, &tm);
	return tm;
}

long long local_ms (int y, int mo0, int d) {
	std::tm t{};
	t.tm_year = y - 1900;
	t.tm_mon = mo0;
	t.tm_mday = d;
	t.tm_isdst = -1;
	return (long long)mktime(&t) * 1000;
}

bool on_day (const Occurrence &e, long long day_start, long long day_end) {
	return (e.end > day_start && e.start < day_end) || (e.start == e.end && e.start >= day_start && e.start < day_end);
}

std::string clock_text (long long ms, bool h24) {
	auto p = time_parts(local(ms), h24);
	return p.period.empty() ? p.time : p.time + " " + p.period;
}

}  // namespace

long long start_of_day (long long now, int offset) {
	std::tm t = local(now);
	return local_ms(t.tm_year + 1900, t.tm_mon, t.tm_mday + offset);
}

std::optional<CalendarPart> parse_calendar (const std::string &text, const std::string &src, const std::string &sig, int index, long long now) {
	if (text.find("BEGIN:VCALENDAR") == std::string::npos) return std::nullopt;
	CalendarPart p;
	p.events = expand_ics(text, start_of_day(now, -CAL_BACK_DAYS), start_of_day(now, CAL_AHEAD_DAYS + 1));
	for (auto &e : p.events) e.src = index;
	p.source = {src, calendar_name(text), sig};
	return p;
}

CalendarData join_calendars (const std::string &key, std::vector<CalendarPart> parts, long long now) {
	CalendarData d;
	d.key = key;
	d.at = (double)now;
	for (auto &p : parts) {
		d.sources.push_back(p.source);
		d.events.insert(d.events.end(), p.events.begin(), p.events.end());
	}
	std::stable_sort(d.events.begin(), d.events.end(), [] (const Occurrence &a, const Occurrence &b) { return a.start != b.start ? a.start < b.start : a.end < b.end; });
	return d;
}

std::string normalize_calendar_url (const std::string &raw) {
	std::string url;
	for (char c : raw) if (!isspace((unsigned char)c)) url += c;
	static const std::regex webcal("^webcal://", std::regex::icase);
	return std::regex_replace(url, webcal, "https://", std::regex_constants::format_first_only);
}

bool valid_calendar_path (const std::string &path) {
	static const std::regex re(R"(^/[A-Za-z0-9._@/-]+$)");
	return std::regex_match(path, re) && path.size() <= 300 && path.find("..") == std::string::npos;
}

std::vector<DayEvent> events_of_day (const CalendarData *data, long long day_start, long long now, bool h24) {
	std::vector<DayEvent> out;
	if (!data) return out;
	long long day_end = start_of_day(day_start, 1), today = start_of_day(now, 0);
	std::vector<const Occurrence *> list;
	for (auto &e : data->events) if (on_day(e, day_start, day_end)) list.push_back(&e);
	std::stable_sort(list.begin(), list.end(), [] (const Occurrence *a, const Occurrence *b) {
		if (a->allDay != b->allDay) return a->allDay;
		return a->start < b->start;
	});
	for (auto *e : list) {
		if (e->allDay) { out.push_back({tr("All day", "Весь день"), "", e->title, e->place, e->src, "allday"}); continue; }
		bool starts_today = e->start >= day_start, ends_today = e->end <= day_end;
		std::string state = "next";
		if (day_start < today) state = "past";
		else if (day_start == today) state = (e->end <= now && e->end > e->start) || (e->end == e->start && e->start < now) ? "past" : e->start <= now ? "now" : "next";
		std::string note = e->end > e->start && ends_today ? fmt(tr("until {0}", "до {0}"), clock_text(e->end, h24)) : e->end > e->start ? tr("until tomorrow", "до следующего дня") : "";
		if (state == "now") note = tr("now", "сейчас");
		out.push_back({starts_today ? clock_text(e->start, h24) : tr("since yesterday", "с вчера"), note, e->title, e->place, e->src, state});
	}
	return out;
}

std::vector<int> dots_of_day (const CalendarData *data, long long day_start) {
	std::vector<int> out;
	if (!data) return out;
	long long day_end = start_of_day(day_start, 1);
	std::vector<bool> seen(data->sources.size(), false);
	for (auto &e : data->events) if (on_day(e, day_start, day_end) && e.src >= 0 && e.src < (int)seen.size()) seen[e.src] = true;
	for (size_t i = 0; i < seen.size(); i++) if (seen[i]) out.push_back((int)i);
	return out;
}

std::string relative_day (int offset) {
	if (offset == 0) return tr("Today", "Сегодня");
	if (offset == 1) return tr("Tomorrow", "Завтра");
	if (offset == -1) return tr("Yesterday", "Вчера");
	if (offset == 2) return tr("In 2 days", "Послезавтра");
	std::string words = count(std::abs(offset), "day", "days", "день", "дня", "дней");
	return fmt(offset > 0 ? tr("In {0}", "Через {0}") : tr("{0} ago", "{0} назад"), words);
}

std::string day_heading (long long now, int offset) {
	std::tm t = local(start_of_day(now, offset));
	return day_name(lang(), t) + ", " + date_text(lang(), t);
}

MonthView month_view (const CalendarData *data, long long now, int month_offset, long long selected_day) {
	long long today = start_of_day(now, 0);
	std::tm t = local(now);
	long long first = local_ms(t.tm_year + 1900, t.tm_mon + month_offset, 1);
	std::tm f = local(first);
	int lead = (f.tm_wday + 6) % 7;
	long long grid_start = local_ms(f.tm_year + 1900, f.tm_mon, 1 - lead), grid_end = start_of_day(grid_start, 42);
	// which calendars have events on each day of the grid
	std::map<long long, std::vector<int>> by;
	if (data)
		for (auto &e : data->events) {
			long long last = start_of_day(std::max(e.start, e.end - 1), 0);
			for (long long day = start_of_day(e.start, 0); day <= last; day = start_of_day(day, 1)) {
				if (day < grid_start || day >= grid_end) continue;
				auto &list = by[day];
				if (std::find(list.begin(), list.end(), e.src) == list.end()) list.push_back(e.src);
			}
		}
	MonthView v;
	v.name = month_name(lang(), f.tm_mon);
	v.year = f.tm_year + 1900;
	for (int w = 0; w < 6; w++) {
		std::vector<MonthCell> row;
		for (int d = 0; d < 7; d++) {
			long long start = start_of_day(grid_start, w * 7 + d);
			std::tm c = local(start);
			std::vector<int> dots = by.count(start) ? by[start] : std::vector<int>();
			std::sort(dots.begin(), dots.end());
			row.push_back({start, c.tm_mday, c.tm_mon == f.tm_mon, start == today, start == selected_day, dots});
		}
		v.weeks.push_back(row);
	}
	return v;
}

int month_offset_of_day (long long now, int day_offset) {
	std::tm a = local(now), b = local(start_of_day(now, day_offset));
	return (b.tm_year - a.tm_year) * 12 + b.tm_mon - a.tm_mon;
}

int day_offset_of_month (long long now, int month_offset) {
	std::tm t = local(now);
	long long first = local_ms(t.tm_year + 1900, t.tm_mon + month_offset, 1);
	return (int)std::lround((first - start_of_day(now, 0)) / 86400000.0);
}

std::string calendar_json (const CalendarData &d) {
	json events = json::array(), sources = json::array();
	for (auto &e : d.events) events.push_back({{"start", e.start}, {"end", e.end}, {"allDay", e.allDay}, {"title", e.title}, {"place", e.place}, {"src", e.src}});
	for (auto &s : d.sources) sources.push_back({{"src", s.src}, {"name", s.name}, {"sig", s.sig}});
	return json({{"key", d.key}, {"at", d.at}, {"sources", sources}, {"events", events}}).dump();
}

std::optional<CalendarData> calendar_from_json (const std::string &raw) {
	json j = json::parse(raw, nullptr, false);
	if (!j.is_object() || !j["key"].is_string() || !j["at"].is_number() || !j["events"].is_array() || !j["sources"].is_array()) return std::nullopt;
	CalendarData d;
	d.key = j["key"];
	d.at = j["at"];
	for (auto &e : j["events"])
		if (e.is_object() && e["start"].is_number() && e["end"].is_number() && e["title"].is_string())
			d.events.push_back({e["start"].get<long long>(), e["end"].get<long long>(), e.value("allDay", false), e["title"], e.value("place", std::string()), e.value("src", 0)});
	for (auto &s : j["sources"])
		if (s.is_object() && s["src"].is_string()) d.sources.push_back({s["src"], s.value("name", std::string()), s.value("sig", std::string())});
	return d;
}

std::optional<CalendarData> usable_calendar (const std::optional<CalendarData> &d, const std::string &key, double now) {
	if (d && d->key == key && now - d->at < CAL_SHOWN_MS && now >= d->at) return d;
	return std::nullopt;
}

bool calendar_fresh (const std::optional<CalendarData> &d, const std::string &key, double now) {
	return d && d->key == key && now - d->at < CAL_FRESH_MS && now >= d->at;
}

namespace {

bool ics_name (const std::string &n) {
	static const std::regex re(R"(^[A-Za-z0-9._@-]+\.ics$)", std::regex::icase);
	return std::regex_match(n, re);
}

// A file on the TV: read directly when this app may; else through the Homebrew Channel.
bool read_tv_file (const std::string &path, std::string &out) {
	if (read_file(path, out, 16u << 20)) return true;
	if (!valid_calendar_path(path)) return false;
	bool ok = false;
	out = hbc_exec("cat " + shell_quote(path), 30000, &ok);
	return ok && !out.empty();
}

std::string signature (const std::string &path) {
	struct stat st;
	if (stat(path.c_str(), &st) == 0) return std::to_string((long long)st.st_size) + "-" + std::to_string((long long)st.st_mtime);
	bool ok = false;
	std::string out = hbc_exec("stat -c '%s-%Y' " + shell_quote(path), 5000, &ok);
	while (!out.empty() && isspace((unsigned char)out.back())) out.pop_back();
	return ok ? out : "";
}

std::vector<std::string> expand_sources (const std::vector<std::string> &sources) {
	std::vector<std::string> out;
	for (auto &s : sources) {
		if (s.empty() || s.back() != '/') { out.push_back(s); continue; }
		if (!valid_calendar_path(s)) continue;
		std::vector<std::string> names;
		auto entries = list_dir(s);
		if (!entries.empty()) { for (auto &e : entries) if (!e.dir && ics_name(e.name)) names.push_back(e.name); }
		else {
			bool ok = false;
			std::string listing = hbc_exec("ls -1 " + shell_quote(s), 10000, &ok);
			size_t i = 0;
			while (ok && i < listing.size()) {
				size_t nl = listing.find('\n', i);
				std::string line = listing.substr(i, nl == std::string::npos ? std::string::npos : nl - i);
				while (!line.empty() && isspace((unsigned char)line.back())) line.pop_back();
				if (ics_name(line)) names.push_back(line);
				if (nl == std::string::npos) break;
				i = nl + 1;
			}
		}
		std::sort(names.begin(), names.end());
		for (auto &n : names) out.push_back(s + n);
	}
	return out;
}

}  // namespace

std::optional<CalendarData> fetch_calendars (const std::vector<std::string> &sources, const std::string &key, const std::optional<CalendarData> &known, std::string *why) {
	long long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	std::vector<std::string> list;
	for (auto &s : sources) list.push_back(!s.empty() && s[0] == '/' ? s : normalize_calendar_url(s));
	list = expand_sources(list);
	std::vector<CalendarPart> parts;
	int failed = 0;
	for (size_t i = 0; i < list.size(); i++) {
		const std::string &src = list[i];
		std::optional<CalendarPart> part;
		bool stale = false;          // not read now: what it had last time
		if (src[0] != '/') {
			HttpResult r = http_get(src, 30, 16u << 20);
			if (r.ok) part = parse_calendar(r.body, src, "", (int)i, now);
			// which one, by number only: the addresses are secret
			if (!part) logf("calendar %zu of %zu: %s", i + 1, list.size(), r.ok ? "not a calendar" : r.error.c_str());
			if (!part && known) {
				// what it had last time stays on the card until it can be read again (the network may not be up yet)
				for (size_t k = 0; k < known->sources.size(); k++)
					if (known->sources[k].src == src) {
						CalendarPart p;
						p.source = known->sources[k];
						for (auto &e : known->events) if (e.src == (int)k) p.events.push_back(e);
						part = p;
						stale = true;
						break;
					}
			}
		} else {
			std::string sig = signature(src);
			int old = -1;
			if (known && !sig.empty())
				for (size_t k = 0; k < known->sources.size(); k++) if (known->sources[k].src == src && known->sources[k].sig == sig) old = (int)k;
			if (old >= 0) {
				// an unchanged file is not read again
				CalendarPart p;
				p.source = known->sources[old];
				for (auto &e : known->events) if (e.src == old) p.events.push_back(e);
				part = p;
			} else {
				std::string text;
				if (read_tv_file(src, text)) part = parse_calendar(text, src, sig, (int)i, now);
			}
		}
		if (part) parts.push_back(*part);
		if (!part || stale) failed++;
	}
	if (parts.empty()) { if (why) *why = failed ? "nothing could be read" : "no calendars"; return std::nullopt; }
	// some could not be read: the data is shown, and `why` asks for another try soon
	if (failed && why) *why = std::to_string(failed) + " of " + std::to_string(list.size()) + " could not be read";
	for (size_t i = 0; i < parts.size(); i++) for (auto &e : parts[i].events) e.src = (int)i;
	return join_calendars(key, parts, now);
}
