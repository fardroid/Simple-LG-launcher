#include "data/config.h"

#include "ui/themes.inc"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <regex>

using json = nlohmann::json;

const std::vector<std::string> LANG_SETTINGS = {"auto", "en", "ru", "es", "fr", "de", "it", "pt", "tr", "uk", "zh", "ja", "ko", "pl"};
// lib/feeds.ts EDITIONS
const std::vector<std::string> NEWS_COUNTRIES = {"AE", "AR", "AU", "BG", "BR", "CA", "CL", "CN", "CO", "CZ", "DE", "EG", "ES", "FR", "GB", "GR",
	"HK", "HU", "ID", "IE", "IL", "IN", "IT", "JP", "KR", "LT", "LV", "MX", "MY", "NG", "NL", "NO", "NZ", "PE", "PH", "PL", "PT", "RO", "RS",
	"RU", "SA", "SE", "SG", "SI", "SK", "TH", "TR", "TW", "UA", "US", "VN", "ZA"};
static const char *CLOCK_FONTS[] = {"inter", "tv", "poppins", "oswald", "mono", "serif"};
static const char *CLOCK_COLORS[] = {"white", "warm", "blue", "amber", "grey", "charcoal", "black", "ember", "ocean", "dusk"};
static const size_t WALLPAPER_MAX_FILES = 400;

std::vector<int> range_values (const Range &r) {
	std::vector<int> out;
	for (int v = r.min; v <= r.max; v += r.step) out.push_back(v);
	return out;
}

namespace {

// A number within the range, snapped to its step; the default when it isn't a number.
int ranged (const json &v, const Range &r) {
	if (!v.is_number()) return r.def;
	double x = v.get<double>();
	if (!std::isfinite(x)) return r.def;
	double clamped = std::min((double)r.max, std::max((double)r.min, x));
	int snapped = r.min + (int)std::lround((clamped - r.min) / r.step) * r.step;
	return std::min(r.max, snapped);
}

std::optional<std::vector<std::string>> string_list (const json &v) {
	if (!v.is_array()) return std::nullopt;
	std::vector<std::string> out;
	for (auto &x : v) if (x.is_string()) out.push_back(x.get<std::string>());
	return out;
}

const json &field (const json &o, const char *name) {
	static const json none;
	if (!o.is_object()) return none;
	auto it = o.find(name);
	return it == o.end() ? none : *it;
}

bool not_false (const json &v) { return !(v.is_boolean() && v.get<bool>() == false); }
bool is_true (const json &v) { return v.is_boolean() && v.get<bool>(); }
std::string str (const json &v) { return v.is_string() ? v.get<std::string>() : std::string(); }
bool one_of (const std::string &s, std::initializer_list<const char *> list) {
	for (const char *x : list) if (s == x) return true;
	return false;
}
template <size_t N> bool in (const std::string &s, const char *(&list)[N]) {
	for (const char *x : list) if (s == x) return true;
	return false;
}

bool has_control (const std::string &s) {
	for (unsigned char c : s) if (c < 0x20 || c == 0x7f) return true;
	return false;
}

// the first `n` characters of a UTF-8 string (JavaScript's slice counts UTF-16 units; close enough for names)
std::string first_chars (const std::string &s, size_t n) {
	size_t count = 0, i = 0;
	while (i < s.size() && count < n) {
		unsigned char c = (unsigned char)s[i];
		i += c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4;
		count++;
	}
	return s.substr(0, std::min(i, s.size()));
}

}  // namespace

bool valid_calendar_source (const std::string &s) {
	static const std::regex url(R"(^https?://[^\s'"`$\\]+$)", std::regex::icase);
	static const std::regex path(R"(^/[A-Za-z0-9._@/-]+$)");
	if (std::regex_match(s, url) && s.size() <= 600) return true;
	return std::regex_match(s, path) && s.size() <= 300 && s.find("..") == std::string::npos;
}

std::string normalize_feed_url (const std::string &raw) {
	std::string s;
	for (char c : raw) if (!isspace((unsigned char)c)) s += c;
	static const std::regex feed2("^feed://", std::regex::icase), feed1("^feed:", std::regex::icase);
	s = std::regex_replace(s, feed2, "https://", std::regex_constants::format_first_only);
	s = std::regex_replace(s, feed1, "", std::regex_constants::format_first_only);
	return s;
}

bool valid_feed_url (const std::string &url) {
	static const std::regex re(R"(^https?://[^\s'"`<>\\]+$)", std::regex::icase);
	return std::regex_match(url, re) && url.size() <= 600;
}

UserConfig parse_config (const std::string &raw) {
	UserConfig d;
	if (raw.empty()) return d;
	json p = json::parse(raw, nullptr, false);
	if (!p.is_object()) return d;
	UserConfig c;
	const json &w = field(p, "wallpaper"), &ck = field(p, "clock"), &wx = field(p, "weather"), &cal = field(p, "calendar"), &nw = field(p, "news");

	std::vector<std::string> feeds;
	if (field(nw, "feeds").is_array())
		for (auto &f : field(nw, "feeds")) {
			if (!f.is_string()) continue;
			std::string u = normalize_feed_url(f.get<std::string>());
			if (valid_feed_url(u) && feeds.size() < 8) feeds.push_back(u);
		}
	bool google = not_false(field(nw, "google"));
	std::vector<std::string> sources;
	const json &legacy = field(cal, "url");
	if (field(cal, "sources").is_array()) { for (auto &x : field(cal, "sources")) if (x.is_string()) sources.push_back(x.get<std::string>()); }
	else if (legacy.is_string()) sources.push_back(legacy.get<std::string>());
	std::vector<std::string> cal_sources;
	for (auto &s : sources) if (valid_calendar_source(s) && cal_sources.size() < 8) cal_sources.push_back(s);

	std::optional<City> city;
	const json &wc = field(wx, "city");
	if (wc.is_object() && field(wc, "name").is_string() && !str(field(wc, "name")).empty() && field(wc, "lat").is_number() && field(wc, "lon").is_number()) {
		double lat = field(wc, "lat").get<double>(), lon = field(wc, "lon").get<double>();
		if (std::isfinite(lat) && std::isfinite(lon) && std::fabs(lat) <= 90 && std::fabs(lon) <= 180) {
			City ct;
			ct.name = first_chars(str(field(wc, "name")), 60);
			ct.lat = lat;
			ct.lon = lon;
			if (field(wc, "detail").is_string()) ct.detail = first_chars(str(field(wc, "detail")), 80);
			city = ct;
		}
	}

	std::vector<std::string> paths;
	if (field(w, "paths").is_array())
		for (auto &x : field(w, "paths")) {
			if (!x.is_string()) continue;
			std::string s = x.get<std::string>();
			if (!s.empty() && s[0] == '/' && !has_control(s) && paths.size() < WALLPAPER_MAX_FILES) paths.push_back(s);
		}
	std::string wmode = str(field(w, "mode"));
	std::string mode = (wmode == "image" || wmode == "folder" || (wmode == "pictures" && !paths.empty())) ? wmode : "gradient";
	std::string path = mode == "pictures" ? paths[0] : str(field(w, "path"));
	const json &iv = field(w, "intervalSec");
	int interval = iv.is_number() && iv.get<double>() >= 5 ? (int)iv.get<double>() : d.wallpaper.intervalSec;

	std::string lang = str(field(p, "language"));
	c.language = std::find(LANG_SETTINGS.begin(), LANG_SETTINGS.end(), lang) != LANG_SETTINGS.end() ? lang : "auto";
	c.appIds = string_list(field(p, "appIds"));
	auto hidden = string_list(field(p, "hiddenIds"));
	// older settings listed the apps to SHOW: they are converted once the app list is known
	c.hiddenIds = hidden ? hidden : (string_list(field(p, "appIds")) ? std::nullopt : std::optional<std::vector<std::string>>(std::vector<std::string>()));
	c.sourceIds = string_list(field(p, "sourceIds")).value_or(std::vector<std::string>());
	c.tileOrder = string_list(field(p, "tileOrder"));
	c.useHomebrew = not_false(field(p, "useHomebrew"));
	c.closeHome = not_false(field(p, "closeHome"));
	c.replaceHome = is_true(field(p, "replaceHome"));
	c.checkUpdates = not_false(field(p, "checkUpdates"));
	c.hookBoot = not_false(field(p, "hookBoot"));
	std::string theme = str(field(p, "theme"));
	c.theme = theme_colors(theme).id;          // an unknown theme: the first, Evening sky
	c.iconScale = ranged(field(p, "iconScale"), ranges::iconScale);
	c.showLabels = not_false(field(p, "showLabels"));
	c.labelsSelectedOnly = is_true(field(p, "labelsSelectedOnly"));
	c.rows = field(p, "rows").is_number() && field(p, "rows").get<double>() == 2 ? 2 : 1;
	c.sideMargin = ranged(field(p, "sideMargin"), ranges::sideMargin);
	c.bottomMargin = ranged(field(p, "bottomMargin"), ranges::bottomMargin);
	c.cardSize = ranged(field(p, "cardSize"), ranges::cardSize);
	std::string plates = str(field(p, "plates"));
	c.plates = plates == "light" || plates == "dark" ? plates : "auto";
	c.glass = not_false(field(p, "glass"));
	c.frost = ranged(field(p, "frost"), ranges::frost);
	c.shadows = not_false(field(p, "shadows"));
	std::string shift = str(field(p, "pixelShift"));
	c.pixelShift = shift == "on" || shift == "off" ? shift : "auto";
	std::string fmt = str(field(p, "clockFormat"));
	c.clockFormat = fmt == "tv" || fmt == "12h" ? fmt : "24h";

	c.clock.size = ranged(field(ck, "size"), ranges::clockSize);
	const json &wt = field(ck, "weight");
	int weight = wt.is_number() ? (int)wt.get<double>() : 0;
	c.clock.weight = weight == 300 || weight == 400 || weight == 500 || weight == 600 ? weight : 200;
	std::string font = str(field(ck, "font"));
	c.clock.font = in(font, CLOCK_FONTS) ? font : "inter";
	std::string color = str(field(ck, "color"));
	c.clock.color = in(color, CLOCK_COLORS) ? color : "white";
	c.clock.opacity = ranged(field(ck, "opacity"), ranges::clockOpacity);
	c.clock.showDate = not_false(field(ck, "showDate"));
	c.clock.dateSize = ranged(field(ck, "dateSize"), ranges::dateSize);
	c.clock.showPeriod = not_false(field(ck, "showPeriod"));

	c.weather.on = is_true(field(wx, "on")) && city.has_value();
	c.weather.city = city;
	c.weather.unit = str(field(wx, "unit")) == "f" ? "f" : "c";
	c.weather.showCity = not_false(field(wx, "showCity"));

	c.calendar.on = is_true(field(cal, "on")) && !cal_sources.empty();
	c.calendar.sources = cal_sources;

	c.news.on = is_true(field(nw, "on")) && (google || !feeds.empty());
	c.news.google = google;
	std::string country = str(field(nw, "country"));
	c.news.country = std::find(NEWS_COUNTRIES.begin(), NEWS_COUNTRIES.end(), country) != NEWS_COUNTRIES.end() ? country : "";
	c.news.feeds = feeds;

	c.quote.on = is_true(field(field(p, "quote"), "on"));

	c.wallpaper.mode = path.empty() ? "gradient" : mode;
	c.wallpaper.path = path;
	c.wallpaper.paths = mode == "pictures" ? paths : std::vector<std::string>();
	c.wallpaper.intervalSec = interval;
	c.wallpaper.shuffle = not_false(field(w, "shuffle"));
	(void)one_of;
	return c;
}

std::string config_json (const UserConfig &c) {
	auto list = [] (const std::optional<std::vector<std::string>> &v) { return v ? json(*v) : json(nullptr); };
	json city = nullptr;
	if (c.weather.city) {
		city = {{"name", c.weather.city->name}, {"lat", c.weather.city->lat}, {"lon", c.weather.city->lon}};
		if (!c.weather.city->detail.empty()) city["detail"] = c.weather.city->detail;
	}
	json j = {
		{"language", c.language}, {"appIds", list(c.appIds)}, {"hiddenIds", list(c.hiddenIds)}, {"sourceIds", c.sourceIds},
		{"tileOrder", list(c.tileOrder)}, {"useHomebrew", c.useHomebrew}, {"closeHome", c.closeHome}, {"replaceHome", c.replaceHome}, {"checkUpdates", c.checkUpdates},
		{"hookBoot", c.hookBoot}, {"theme", c.theme}, {"iconScale", c.iconScale}, {"showLabels", c.showLabels},
		{"labelsSelectedOnly", c.labelsSelectedOnly}, {"rows", c.rows}, {"sideMargin", c.sideMargin},
		{"bottomMargin", c.bottomMargin}, {"cardSize", c.cardSize}, {"plates", c.plates}, {"glass", c.glass}, {"frost", c.frost},
		{"shadows", c.shadows}, {"pixelShift", c.pixelShift}, {"clockFormat", c.clockFormat},
		{"clock", {{"size", c.clock.size}, {"weight", c.clock.weight}, {"font", c.clock.font}, {"color", c.clock.color},
			{"opacity", c.clock.opacity}, {"showDate", c.clock.showDate}, {"dateSize", c.clock.dateSize}, {"showPeriod", c.clock.showPeriod}}},
		{"weather", {{"on", c.weather.on}, {"city", city}, {"unit", c.weather.unit}, {"showCity", c.weather.showCity}}},
		{"calendar", {{"on", c.calendar.on}, {"sources", c.calendar.sources}}},
		{"news", {{"on", c.news.on}, {"google", c.news.google}, {"country", c.news.country}, {"feeds", c.news.feeds}}},
		{"quote", {{"on", c.quote.on}}},
		{"wallpaper", {{"mode", c.wallpaper.mode}, {"path", c.wallpaper.path}, {"paths", c.wallpaper.paths},
			{"intervalSec", c.wallpaper.intervalSec}, {"shuffle", c.wallpaper.shuffle}}}
	};
	return j.dump();
}
