#include "data/ics.h"

#include "data/tz.h"
#include "i18n/i18n.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <map>
#include <optional>
#include <regex>

namespace {

const long long DAY = 86400000;
const char *WEEKDAYS[] = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"};
const int MAX_WALK_DAYS = 20000;       // safety limit on how many days one repeating event is walked through

enum class Freq { daily, weekly, monthly, yearly };
struct ByDay { int n, wd; };
struct Rule {
	Freq freq;
	int interval = 1;
	std::optional<int> count;
	std::optional<long long> until;
	std::vector<ByDay> byDay;
	std::vector<int> byMonthDay, byMonth;
};

struct Prop { std::string name; std::map<std::string, std::string> params; std::string value; };

struct RawEvent {
	std::string uid, title, place, tz;
	Wall wall;
	long long start, end;
	bool allDay;
	std::optional<Rule> rule;
	std::vector<long long> exdates;
	std::optional<long long> recurrenceId;
	bool cancelled;
};

std::string upper (std::string s) { for (auto &c : s) c = (char)toupper((unsigned char)c); return s; }
std::string trim (const std::string &s) {
	size_t a = 0, b = s.size();
	while (a < b && isspace((unsigned char)s[a])) a++;
	while (b > a && isspace((unsigned char)s[b - 1])) b--;
	return s.substr(a, b - a);
}

std::vector<std::string> unfold (const std::string &text) {
	std::string t;
	t.reserve(text.size());
	for (size_t i = 0; i < text.size(); i++) {
		if (text[i] == '\r') { if (i + 1 < text.size() && text[i + 1] == '\n') continue; t += '\n'; continue; }
		t += text[i];
	}
	std::vector<std::string> lines;
	std::string cur;
	for (size_t i = 0; i < t.size(); i++) {
		if (t[i] == '\n') {
			// a line that starts with a space or a tab continues the one before
			if (i + 1 < t.size() && (t[i + 1] == ' ' || t[i + 1] == '\t')) { i++; continue; }
			lines.push_back(cur);
			cur.clear();
		} else cur += t[i];
	}
	lines.push_back(cur);
	return lines;
}

std::string unescape_text (const std::string &v) {
	std::string out;
	for (size_t i = 0; i < v.size(); i++) {
		if (v[i] == '\\' && i + 1 < v.size()) {
			char c = v[i + 1];
			if (c == 'n' || c == 'N') { out += '\n'; i++; continue; }
			if (c == ',' || c == ';' || c == '\\') { out += c; i++; continue; }
		}
		out += v[i];
	}
	return out;
}

std::optional<Prop> parse_prop (const std::string &line) {
	size_t colon = line.find(':');
	if (colon == std::string::npos || colon < 1) return std::nullopt;
	Prop p;
	std::string head = line.substr(0, colon);
	size_t i = 0;
	bool first = true;
	while (i <= head.size()) {
		size_t semi = head.find(';', i);
		std::string part = head.substr(i, semi == std::string::npos ? std::string::npos : semi - i);
		if (first) { p.name = upper(part); first = false; }
		else {
			size_t eq = part.find('=');
			if (eq != std::string::npos && eq > 0) {
				std::string v = part.substr(eq + 1);
				v.erase(std::remove(v.begin(), v.end(), '"'), v.end());
				p.params[upper(part.substr(0, eq))] = v;
			}
		}
		if (semi == std::string::npos) break;
		i = semi + 1;
	}
	p.value = line.substr(colon + 1);
	return p;
}

long long utc_of (int y, int mo, int d, int h, int mi, int s) {
	std::tm t{};
	t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = d; t.tm_hour = h; t.tm_min = mi; t.tm_sec = s;
	return (long long)timegm(&t) * 1000;
}

long long local_of (int y, int mo, int d, int h, int mi, int s) {
	std::tm t{};
	t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = d; t.tm_hour = h; t.tm_min = mi; t.tm_sec = s;
	t.tm_isdst = -1;
	return (long long)mktime(&t) * 1000;
}

struct Stamp { Wall wall; std::string tz; long long ms; bool allDay; };

std::optional<Stamp> parse_stamp (const Prop &p) {
	static const std::regex re(R"(^(\d{4})(\d{2})(\d{2})(?:T(\d{2})(\d{2})(\d{2})?)?(Z)?$)");
	std::smatch m;
	std::string v = trim(p.value);
	if (!std::regex_match(v, m, re)) return std::nullopt;
	auto it = p.params.find("VALUE");
	bool all_day = !m[4].matched || (it != p.params.end() && it->second == "DATE");
	Wall w{std::stoi(m[1]), std::stoi(m[2]), std::stoi(m[3]), all_day ? 0 : std::stoi(m[4]), all_day ? 0 : std::stoi(m[5]), all_day || !m[6].matched ? 0 : std::stoi(m[6])};
	auto tzid = p.params.find("TZID");
	std::string tz = all_day ? "" : m[7].matched ? "UTC" : tzid != p.params.end() ? tzid->second : "";
	return Stamp{w, tz, wall_to_ms(w, tz), all_day};
}

std::optional<long long> parse_duration (const std::string &v) {
	static const std::regex re(R"(^([+-])?P(?:(\d+)W)?(?:(\d+)D)?(?:T(?:(\d+)H)?(?:(\d+)M)?(?:(\d+)S)?)?$)");
	std::smatch m;
	std::string s = trim(v);
	if (!std::regex_match(s, m, re)) return std::nullopt;
	auto n = [&] (int i) { return m[i].matched ? std::stoll(m[i]) : 0LL; };
	long long ms = (n(2) * 7 + n(3)) * DAY + n(4) * 3600000 + n(5) * 60000 + n(6) * 1000;
	return m[1].matched && m[1] == "-" ? -ms : ms;
}

std::vector<int> nums (const std::string &s) {
	std::vector<int> out;
	size_t i = 0;
	while (i < s.size()) {
		size_t c = s.find(',', i);
		std::string part = s.substr(i, c == std::string::npos ? std::string::npos : c - i);
		char *end = nullptr;
		long v = strtol(part.c_str(), &end, 10);
		if (end != part.c_str()) out.push_back((int)v);
		if (c == std::string::npos) break;
		i = c + 1;
	}
	return out;
}

std::optional<Rule> parse_rule (const std::string &value) {
	std::map<std::string, std::string> part;
	size_t i = 0;
	while (i <= value.size()) {
		size_t semi = value.find(';', i);
		std::string kv = value.substr(i, semi == std::string::npos ? std::string::npos : semi - i);
		size_t eq = kv.find('=');
		if (eq != std::string::npos && eq > 0) part[upper(kv.substr(0, eq))] = kv.substr(eq + 1);
		if (semi == std::string::npos) break;
		i = semi + 1;
	}
	Rule r;
	std::string f = part["FREQ"];
	if (f == "DAILY") r.freq = Freq::daily;
	else if (f == "WEEKLY") r.freq = Freq::weekly;
	else if (f == "MONTHLY") r.freq = Freq::monthly;
	else if (f == "YEARLY") r.freq = Freq::yearly;
	else return std::nullopt;
	static const std::regex day_re(R"(^([+-]?\d+)?(SU|MO|TU|WE|TH|FR|SA)$)");
	if (part.count("BYDAY")) {
		std::string s = part["BYDAY"];
		size_t j = 0;
		while (j <= s.size()) {
			size_t c = s.find(',', j);
			std::string d = trim(s.substr(j, c == std::string::npos ? std::string::npos : c - j));
			std::smatch m;
			if (std::regex_match(d, m, day_re)) {
				int wd = 0;
				for (int k = 0; k < 7; k++) if (m[2] == WEEKDAYS[k]) wd = k;
				r.byDay.push_back({m[1].matched ? std::stoi(m[1]) : 0, wd});
			}
			if (c == std::string::npos) break;
			j = c + 1;
		}
	}
	if (part.count("UNTIL")) {
		Prop p{"UNTIL", {}, part["UNTIL"]};
		auto st = parse_stamp(p);
		if (st) r.until = st->allDay ? st->ms + DAY - 1 : st->ms;
	}
	int interval = part.count("INTERVAL") ? atoi(part["INTERVAL"].c_str()) : 1;
	r.interval = std::max(1, interval ? interval : 1);
	if (part.count("COUNT")) { int c = atoi(part["COUNT"].c_str()); if (c) r.count = c; }
	if (part.count("BYMONTHDAY")) r.byMonthDay = nums(part["BYMONTHDAY"]);
	if (part.count("BYMONTH")) r.byMonth = nums(part["BYMONTH"]);
	return r;
}

std::optional<RawEvent> build_event (const std::vector<Prop> &props) {
	auto get = [&] (const char *name) -> const Prop * {
		for (auto &p : props) if (p.name == name) return &p;
		return nullptr;
	};
	const Prop *dtstart = get("DTSTART");
	auto stamp = dtstart ? parse_stamp(*dtstart) : std::nullopt;
	if (!stamp) return std::nullopt;
	const Prop *dtend = get("DTEND");
	auto end_stamp = dtend ? parse_stamp(*dtend) : std::nullopt;
	auto dur = get("DURATION") ? parse_duration(get("DURATION")->value) : std::nullopt;
	long long end = end_stamp ? end_stamp->ms : dur ? stamp->ms + *dur : stamp->allDay ? stamp->ms + DAY : stamp->ms;
	if (end < stamp->ms) end = stamp->ms;
	RawEvent e;
	const Prop *rec = get("RECURRENCE-ID");
	auto rec_stamp = rec ? parse_stamp(*rec) : std::nullopt;
	for (auto &p : props) {
		if (p.name != "EXDATE") continue;
		size_t i = 0;
		while (i <= p.value.size()) {
			size_t c = p.value.find(',', i);
			Prop one{"EXDATE", p.params, p.value.substr(i, c == std::string::npos ? std::string::npos : c - i)};
			auto s = parse_stamp(one);
			if (s) e.exdates.push_back(s->ms);
			if (c == std::string::npos) break;
			i = c + 1;
		}
	}
	const Prop *status = get("STATUS");
	e.uid = get("UID") ? get("UID")->value : "";
	e.title = get("SUMMARY") ? trim(unescape_text(get("SUMMARY")->value)) : "";
	if (get("LOCATION")) {
		std::string place = unescape_text(get("LOCATION")->value), squeezed;
		bool space = false;
		for (char c : place) {
			if (isspace((unsigned char)c)) { space = true; continue; }
			if (space && !squeezed.empty()) squeezed += ' ';
			space = false;
			squeezed += c;
		}
		e.place = squeezed;
	}
	e.wall = stamp->wall;
	e.tz = stamp->tz;
	e.start = stamp->ms;
	e.end = end;
	e.allDay = stamp->allDay;
	if (const Prop *rr = get("RRULE")) e.rule = parse_rule(rr->value);
	if (rec_stamp) e.recurrenceId = rec_stamp->ms;
	e.cancelled = status && upper(trim(status->value)) == "CANCELLED";
	return e;
}

std::vector<RawEvent> read_events (const std::string &text) {
	std::vector<RawEvent> events;
	std::vector<Prop> props;
	bool in = false;
	int depth = 0;
	for (auto &line : unfold(text)) {
		if (line == "BEGIN:VEVENT") { props.clear(); in = true; depth = 0; continue; }
		if (line == "END:VEVENT") {
			if (in) { auto ev = build_event(props); if (ev) events.push_back(*ev); }
			in = false;
			continue;
		}
		if (!in) continue;
		// nested components (alarms) have their own properties, which must not be mistaken for the event's
		if (line.compare(0, 6, "BEGIN:") == 0) { depth++; continue; }
		if (line.compare(0, 4, "END:") == 0) { depth--; continue; }
		if (depth > 0) continue;
		auto p = parse_prop(line);
		if (p) props.push_back(*p);
	}
	return events;
}

long long day_number (int y, int mo, int d) { return (long long)std::floor(utc_of(y, mo, d, 0, 0, 0) / (double)DAY); }
int weekday_of (long long dn) { return (int)((((dn + 4) % 7) + 7) % 7); }
int days_in (int y, int mo) {
	static const int len[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
	return mo == 2 && leap ? 29 : len[mo - 1];
}
void civil (long long dn, int &y, int &mo, int &d) {
	std::time_t t = (std::time_t)(dn * 86400);
	std::tm tm;
	gmtime_r(&t, &tm);
	y = tm.tm_year + 1900; mo = tm.tm_mon + 1; d = tm.tm_mday;
}

bool day_matches (const Rule &r, const RawEvent &ev, long long dn, long long start_dn) {
	int y, mo, d;
	civil(dn, y, mo, d);
	int wd = weekday_of(dn);
	auto in_by_day = [&] {
		for (auto &b : r.byDay) {
			if (b.wd != wd) continue;
			if (b.n == 0) return true;
			if (b.n > 0 ? (d - 1) / 7 + 1 == b.n : (days_in(y, mo) - d) / 7 + 1 == -b.n) return true;
		}
		return false;
	};
	auto in_month_day = [&] {
		if (r.byMonthDay.empty()) return d == ev.wall.d;
		for (int x : r.byMonthDay) if (x > 0 ? x == d : days_in(y, mo) + x + 1 == d) return true;
		return false;
	};
	if (r.freq == Freq::daily) return (dn - start_dn) % r.interval == 0 && (r.byDay.empty() || in_by_day());
	if (r.freq == Freq::weekly) {
		long long week_start = start_dn - ((weekday_of(start_dn) + 6) % 7);
		long long week = (long long)std::floor((dn - week_start) / 7.0);
		if (week % r.interval != 0) return false;
		return r.byDay.empty() ? wd == weekday_of(start_dn) : in_by_day();
	}
	if (r.freq == Freq::monthly) {
		int months = (y - ev.wall.y) * 12 + (mo - ev.wall.mo);
		if (months % r.interval != 0) return false;
		return r.byDay.empty() ? in_month_day() : in_by_day();
	}
	if ((y - ev.wall.y) % r.interval != 0) return false;
	bool month = r.byMonth.empty() ? mo == ev.wall.mo : std::find(r.byMonth.begin(), r.byMonth.end(), mo) != r.byMonth.end();
	if (!month) return false;
	return r.byDay.empty() ? in_month_day() : in_by_day();
}

std::vector<long long> repeat_starts (const RawEvent &ev, const Rule &r, long long from, long long to, long long span) {
	std::vector<long long> out;
	long long start_dn = day_number(ev.wall.y, ev.wall.mo, ev.wall.d);
	long long last_dn = (long long)std::floor(to / (double)DAY) + 2;
	int count = 0, steps = 0;
	for (long long dn = start_dn; dn <= last_dn && steps < MAX_WALK_DAYS; dn++, steps++) {
		if (!day_matches(r, ev, dn, start_dn)) continue;
		int y, mo, d;
		civil(dn, y, mo, d);
		long long start = wall_to_ms({y, mo, d, ev.wall.h, ev.wall.mi, ev.wall.s}, ev.tz);
		if (r.until && start > *r.until) break;
		count++;
		if (r.count && count > *r.count) break;
		if (start + span > from && start < to) out.push_back(start);
	}
	return out;
}

}  // namespace

long long wall_to_ms (const Wall &w, const std::string &tz) {
	if (tz.empty()) return local_of(w.y, w.mo, w.d, w.h, w.mi, w.s);
	long long as_utc = utc_of(w.y, w.mo, w.d, w.h, w.mi, w.s);
	if (tz == "UTC") return as_utc;
	auto first = zone_offset(tz, as_utc);
	if (!first) return local_of(w.y, w.mo, w.d, w.h, w.mi, w.s);
	long long guess = as_utc - *first;
	auto second = zone_offset(tz, guess);
	return second ? as_utc - *second : guess;
}

std::vector<Occurrence> expand_ics (const std::string &text, long long from, long long to) {
	auto events = read_events(text);
	std::map<std::string, std::vector<long long>> replaced;
	for (auto &e : events) if (e.recurrenceId) replaced[e.uid].push_back(*e.recurrenceId);
	std::vector<Occurrence> out;
	std::string untitled = tr("(no title)", "Без названия");
	for (auto &e : events) {
		if (e.cancelled) continue;
		long long span = e.end - e.start;
		auto add = [&] (long long start) {
			long long end = start + span;
			if (e.allDay) {
				// an all-day event spans whole local days even when a daylight-saving change falls inside
				long long days = std::llround(span / (double)DAY);
				std::time_t t = (std::time_t)(start / 1000);
				std::tm lt;
				localtime_r(&t, &lt);
				end = local_of(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday + (int)days, 0, 0, 0);
			}
			if ((end > from && start < to) || (span == 0 && start >= from && start < to))
				out.push_back({start, end, e.allDay, e.title.empty() ? untitled : e.title, e.place, 0});
		};
		if (!e.rule) { add(e.start); continue; }
		std::vector<long long> skip = e.exdates;
		auto it = replaced.find(e.uid);
		if (it != replaced.end()) skip.insert(skip.end(), it->second.begin(), it->second.end());
		for (long long s : repeat_starts(e, *e.rule, from, to, span))
			if (std::find(skip.begin(), skip.end(), s) == skip.end()) add(s);
	}
	std::stable_sort(out.begin(), out.end(), [] (const Occurrence &a, const Occurrence &b) {
		return a.start != b.start ? a.start < b.start : a.end < b.end;
	});
	return out;
}

std::string calendar_name (const std::string &text) {
	for (auto &line : unfold(text)) {
		if (line.compare(0, 12, "X-WR-CALNAME") != 0) continue;
		size_t colon = line.find(':');
		if (colon != std::string::npos) return trim(unescape_text(line.substr(colon + 1)));
	}
	return "";
}
