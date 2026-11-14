// Tests of the logic carried over from the web version (1.x), with that version's own cases, so both
// give the same answers.   Run: make test

#include "data/calendar.h"
#include "data/config.h"
#include "data/feeds.h"
#include "data/quote.h"
#include "data/ics.h"
#include <nlohmann/json.hpp>
#include "data/order.h"
#include "data/prefs.h"
#include "data/gallery.h"
#include "data/homemode.h"
#include "data/quick.h"
#include "data/uninstall.h"
#include "data/update.h"
#include "core/sha256.h"
#include "sys/http.h"
#include "data/rows.h"
#include "sys/fs.h"
#include "sys/luna.h"
#include "data/weather.h"
#include "data/cities.h"
#include "data/inputs.h"
#include "data/storage.h"
#include "data/collection.h"
#include "data/collection_store.h"
#include "core/sha256.h"
void run_posted ();
#include "data/sun.h"
#include "i18n/i18n.h"
#include "i18n/dates.h"
#include "ui/boardlayout.h"
#include "ui/plates.h"

#include <cmath>

#include <algorithm>
#include <cstdio>
#include <unistd.h>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

static int fails = 0, passes = 0;

static std::string show (const std::vector<int> &v) {
	std::ostringstream o;
	o << "[";
	for (size_t i = 0; i < v.size(); i++) o << (i ? "," : "") << v[i];
	return o.str() + "]";
}
static std::string show (const std::vector<std::string> &v) {
	std::string o = "[";
	for (size_t i = 0; i < v.size(); i++) o += (i ? "," : "") + v[i];
	return o + "]";
}
static std::string show (const RowsPosition &p) { return "{" + std::to_string(p.row) + "," + show(p.cols) + "}"; }
static std::string show (const std::string &s) { return s; }
static std::string show (int v) { return std::to_string(v); }
static std::string show (long long v) { return std::to_string(v); }
static std::string show (bool v) { return v ? "true" : "false"; }

template <class A, class B>
static void eq (const A &a, const B &b, const char *msg);
template <class A, class B>
static void eq (const A &a, const B &b, const std::string &msg) { eq(a, b, msg.c_str()); }
template <class A, class B>
static void eq (const A &a, const B &b, const char *msg) {
	std::string x = show(a), y = show(b);
	if (x == y) { passes++; return; }
	fails++;
	printf("FAIL %s: %s != %s\n", msg, x.c_str(), y.c_str());
}

static RowsPosition P (int row, std::vector<int> cols) { RowsPosition p; p.row = row; p.cols = cols; return p; }
using S = std::vector<std::string>;
static std::function<std::string(const std::string &)> ID = [] (const std::string &s) { return s; };

static void rows () {
	eq(split_rows(5, 1), std::vector<int>{5}, "one row takes everything");
	eq(split_rows(5, 2), std::vector<int>{3, 2}, "two rows: the top row gets the extra tile");
	eq(split_rows(4, 2), std::vector<int>{2, 2}, "two rows: an even split");
	eq(split_rows(1, 2), std::vector<int>{1}, "one tile never makes a second row");
	eq(split_rows(0, 2), std::vector<int>{0}, "no tiles");
	eq(row_offsets({3, 2}), std::vector<int>{0, 3}, "where each row starts");
	std::vector<int> sizes{3, 2};
	eq(move_position(sizes, P(0, {2, 0}), Dir::right), P(0, {2, 0}), "right stops at the end of the row");
	eq(move_position(sizes, P(0, {0, 0}), Dir::left), P(0, {0, 0}), "left stops at the start");
	eq(move_position(sizes, P(0, {2, 0}), Dir::down), P(1, {2, 1}), "down keeps the column where there is one");
	eq(move_position(sizes, P(1, {2, 1}), Dir::up), P(0, {1, 1}), "up goes back, to the same column");
	eq(move_position(sizes, P(0, {1, 0}), Dir::up), P(0, {1, 0}), "up on the top row does nothing");
	eq(move_position(sizes, P(1, {0, 0}), Dir::down), P(1, {0, 0}), "down on the last row does nothing");
	eq(move_position({5}, P(0, {2, 0}), Dir::down), P(0, {2}), "with one row, up and down do nothing");
	eq(clamp_position({1}, P(1, {0, 3})), P(0, {0}), "two rows -> one");
	eq(clamp_position({4, 3}, P(0, {9, 9})), P(0, {3, 2}), "columns kept inside their rows");
	eq(move_position({0}, RowsPosition(), Dir::right), P(0, {0}), "an empty row does not break");
}

static void apporder () {
	S A{"a", "b", "c", "d"};
	using O = std::optional<S>;
	eq(visible_apps(A, O(), O(S{}), ID), S{"a", "b", "c", "d"}, "no settings: every app");
	eq(visible_apps(A, O(), O(S{"b"}), ID), S{"a", "c", "d"}, "a hidden app is left out");
	eq(visible_apps(A, O(S{"c", "a"}), O(S{}), ID), S{"c", "a", "b", "d"}, "saved order first");
	eq(visible_apps(A, O(S{"c", "a"}), O(S{"a"}), ID), S{"c", "b", "d"}, "hidden wins over the saved order");
	S withNew = A;
	withNew.push_back("new");
	eq(visible_apps(withNew, O(S{"c", "a"}), O(S{"b"}), ID), S{"c", "a", "d", "new"}, "a new app shows up at the end");
	eq(visible_apps(A, O(S{"gone", "a"}), O(S{}), ID), S{"a", "b", "c", "d"}, "an uninstalled app is ignored");
	eq(visible_apps(A, O(S{"a", "a", "b"}), O(S{}), ID), S{"a", "b", "c", "d"}, "a repeated id is shown once");
	eq(visible_apps(A, O(S{"b", "a"}), O(), ID), S{"b", "a"}, "old settings: only the saved list");
	eq(visible_apps(A, O(), O(), ID), S{"a", "b", "c", "d"}, "old settings with no list: everything");
	eq(hidden_after_upgrade(A, O(S{"b", "a"}), ID), S{"c", "d"}, "old list -> hide-list");
	eq(hidden_after_upgrade(A, O(), ID), S{}, "no old list: nothing hidden");
	eq(toggle_hidden({"a"}, "b"), S{"a", "b"}, "hiding adds");
	eq(toggle_hidden({"a", "b"}, "a"), S{"b"}, "showing takes it off");
}

static void tileorder () {
	using O = std::optional<S>;
	eq(order_tiles(S{"a", "b", "c"}, ID, O()), S{"a", "b", "c"}, "no saved order");
	eq(order_tiles(S{"a", "b", "c"}, ID, O(S{})), S{"a", "b", "c"}, "an empty saved order");
	eq(order_tiles(S{"a", "b", "c", "h1"}, ID, O(S{"h1", "c"})), S{"h1", "c", "a", "b"}, "listed first, the rest after");
	eq(order_tiles(S{"a", "b"}, ID, O(S{"gone", "b", "b", "a"})), S{"b", "a"}, "unknown and repeated keys ignored");
	auto m = moves_for({"a", "b", "c"}, "a");
	eq(show(m.start) + show(m.left) + show(m.right) + show(m.end), std::string("falsefalsetruetrue"), "first tile");
	m = moves_for({"a", "b", "c"}, "c");
	eq(show(m.start) + show(m.left) + show(m.right) + show(m.end), std::string("truetruefalsefalse"), "last tile");
	eq(move_key({"a", "b", "c", "d"}, "c", MoveAction::start), S{"c", "a", "b", "d"}, "move to start");
	eq(move_key({"a", "b", "c", "d"}, "c", MoveAction::left), S{"a", "c", "b", "d"}, "move left");
	eq(move_key({"a", "b", "c", "d"}, "c", MoveAction::right), S{"a", "b", "d", "c"}, "move right");
	eq(move_key({"a", "b"}, "a", MoveAction::left), S{"a", "b"}, "left from the start does nothing");
	eq(move_key({"a", "b", "c", "d"}, "b", MoveAction::end), S{"a", "c", "d", "b"}, "move to end");
	eq(move_key({"a", "b"}, "q", MoveAction::end), S{"a", "b"}, "an unknown key does nothing");
	eq(reslot({"a", "h1", "b", "h2"}, {"b", "a"}), S{"b", "h1", "a", "h2"}, "only the apps swap");
	eq(reslot({"a", "h1", "b", "h2"}, {"h2", "h1"}), S{"a", "h2", "b", "h1"}, "only the inputs swap");
	eq(reslot({"a", "b"}, {"b", "a", "ghost"}), S{"b", "a"}, "keys not on the row are ignored");
	auto u = order_update({"h1", "b", "a"}, {"a", "b"}, {"h1"}, S{"a", "b", "hiddenApp"}, {"h1", "h9"});
	eq(show(u.tileOrder) + show(*u.appIds) + show(u.sourceIds), std::string("[h1,b,a][b,a,hiddenApp][h1,h9]"), "order update keeps the others");
	eq(order_update({"a"}, {"a"}, {}, std::nullopt, {}).appIds.has_value(), false, "old app settings are left alone");
}

static std::vector<int> R (const BoardLayout &l, CardId id) {
	auto c = l.card(id);
	return c ? std::vector<int>{c->x, c->y, c->w, c->h} : std::vector<int>{};
}

static void boardlayout () {
	eq(board_top({80, 50, true, 1}, 14), 74, "one row, 80 % icons: equal space above and below");
	eq(board_top({80, 50, false, 1}, 14) > 74, true, "without the names there is more room");
	eq(board_top({80, 50, true, 2}, 14) < 74, true, "two rows leave less room");
	eq(board_top({80, 50, true, 2}, 1), 74, "two rows with one tile are one row");
	eq(board_top({130, 200, true, 2}, 14), 16, "never closer than 16 px to the top");
	ClockShape clock{160, true, 100, false};
	auto on = [&] (bool w, bool c, bool n, bool q) { Cards x; x.weather = w; x.calendar = c; x.news = n; x.quote = q; x.clock = clock; return x; };
	auto l = board_layout(on(true, true, false, false), 40);
	eq(R(l, CardId::clock), std::vector<int>{200, 40, 880, 184}, "two columns: the clock");
	eq(R(l, CardId::weather), std::vector<int>{200, 264, 880, 462}, "two columns: the weather");
	eq(R(l, CardId::calendar), std::vector<int>{1120, 40, 600, 686}, "two columns: the calendar");
	eq(R(board_layout(on(false, false, false, false), 40), CardId::clock), std::vector<int>{520, 40, 880, 184}, "the clock alone");
	l = board_layout(on(true, true, false, true), 40);
	eq(R(l, CardId::calendar), std::vector<int>{1120, 40, 600, 470}, "with the quote: the calendar above it");
	eq(R(l, CardId::quote), std::vector<int>{1120, 550, 600, 176}, "the quote under the calendar");
	l = board_layout(on(true, true, true, false), 40);
	eq(l.narrow, true, "news and calendar: three columns");
	eq(R(l, CardId::news), std::vector<int>{1280, 40, 560, 686}, "three columns: the news");
	eq(R(l, CardId::calendar), std::vector<int>{680, 40, 560, 686}, "three columns: the calendar");
	l = board_layout(on(true, true, true, true), 40);
	int nc = R(l, CardId::clock)[3];
	eq(nc, narrow_clock_size(clock) + 24 + 38 + 8, "narrow clock: the date under the time");
	eq(R(l, CardId::weather)[1] + R(l, CardId::weather)[3], 726, "the weather takes the rest");
	eq(narrow_clock_size({220, true, 100, false}), 181, "narrow digits shrink to fit");
	eq(narrow_clock_size({220, true, 100, true}), 157, "narrow digits with AM/PM");
	eq(narrow_clock_size({120, true, 100, false}), 120, "small digits stay");
	eq(clock_height({160, false, 100, false}, true), 184, "narrow without the date");
	eq(R(board_layout(on(false, false, false, true), 40), CardId::quote), std::vector<int>{1120, 40, 600, 686}, "a quote alone takes its column");
	eq(column_at(l, 100), 0, "the column above the icon (left)");
	eq(column_at(l, 960), 1, "the column above the icon (middle)");
	eq(column_at(l, 1900), 2, "the column above the icon (right)");
	eq((int)*entry_card(l, 2), (int)CardId::news, "the lowest card that can be selected");
	eq(beside_card(l, CardId::weather, 1).has_value() && *beside_card(l, CardId::weather, 1) == CardId::calendar, true, "right of the weather: the calendar");
	eq(beside_card(l, CardId::weather, -1).has_value(), false, "nothing left of the weather");
	eq(column_at(board_layout(on(false, false, false, true), 40), 500), -1, "nothing to step onto");
}

static void config () {
	UserConfig d = parse_config("");
	eq(d.theme, std::string("dusk"), "defaults: dusk");
	eq(d.hiddenIds.has_value(), true, "defaults: an empty hide-list");
	UserConfig c = parse_config(R"({"theme":"black","iconScale":87,"clock":{"weight":450,"font":"tv","size":999},"appIds":["a"],"wallpaper":{"mode":"pictures","paths":[]}})");
	eq(c.theme, std::string("black"), "the theme is read");
	eq(c.iconScale, 90, "a size snaps to its step");
	eq(c.clock.weight, 200, "an unknown weight is the default");
	eq(c.clock.font, std::string("tv"), "the font is read");
	eq(c.clock.size, 220, "a size is kept inside its range");
	eq(c.hiddenIds.has_value(), false, "old settings (appIds, no hiddenIds) wait to be converted");
	eq(c.wallpaper.mode, std::string("gradient"), "pictures without any picture fall back to the gradient");
	UserConfig w = parse_config(R"({"weather":{"on":true,"city":{"name":"Milano","lat":45.46,"lon":9.19}},"calendar":{"on":true,"sources":["https://x.example/a.ics","bad path"]}})");
	eq(w.weather.on, true, "the weather is on with a city");
	eq((int)w.calendar.sources.size(), 1, "a bad calendar source is dropped");
	eq(parse_config(config_json(w)).weather.city->name, std::string("Milano"), "the settings survive being saved and read again");
}

static void dates () {
	std::tm t{};
	t.tm_year = 2026 - 1900; t.tm_mon = 9; t.tm_mday = 11; t.tm_wday = 0; t.tm_hour = 15; t.tm_min = 4;
	eq(date_year("en", t), std::string("11 October 2026"), "English date");
	eq(date_year("ru", t), std::string("11 октября 2026"), "Russian date");
	eq(day_name("ru", t), std::string("Воскресенье"), "Russian day");
	eq(date_year("de", t), std::string("11. Oktober 2026"), "German date (Intl)");
	eq(time_parts(t, true).time, std::string("15:04"), "24-hour");
	eq(time_parts(t, false).time + time_parts(t, false).period, std::string("3:04PM"), "12-hour");
	eq(lang_from_tag("pt-BR"), std::string("pt"), "pt-BR -> pt");
	eq(lang_from_tag("zh-Hans-CN"), std::string("zh"), "zh-Hans-CN -> zh");
	eq(lang_from_tag("nl-NL"), std::string("en"), "a language the launcher lacks -> English");
}

static std::string A (const PlateLook &l) {
	char b[64];
	snprintf(b, sizeof b, "%d,%d,%d,%.2f", (int)std::lround(l.bg.r * 255), (int)std::lround(l.bg.g * 255), (int)std::lround(l.bg.b * 255), l.bg.a);
	return b;
}

static void plates () {
	std::vector<unsigned char> px(64 * 36 * 4);
	for (int y = 0; y < 36; y++) for (int x = 0; x < 64; x++) { int i = (y * 64 + x) * 4; px[i] = px[i + 1] = px[i + 2] = x < 32 ? 255 : 0; px[i + 3] = 255; }
	auto map = map_from_pixels(px.data());
	eq(std::vector<int>{(int)map.size(), (int)map[0], (int)map[7], (int)map[8], (int)map[15]}, std::vector<int>{144, 1, 1, 0, 0}, "a 16 x 9 grid of brightness");
	std::array<int, 4> clock{200, 40, 880, 184}, weather{200, 264, 880, 462}, calendar{1120, 40, 600, 686};
	eq(region_brightness(map, weather) > 0.8f, true, "the weather is over the bright half");
	eq(region_brightness(map, calendar) == 0.f, true, "the calendar is over the dark half");
	eq(A(look_for(0)), std::string("255,255,255,0.11"), "a dark background gets a light plate");
	eq(A(look_for(1)), std::string("12,18,46,0.55"), "a bright background gets a dark plate");
	eq(A(look_for(0.5f)) + " " + A(look_for(0.34f)) + " " + A(look_for(0.35f)), std::string("12,18,46,0.55 255,255,255,0.11 12,18,46,0.55"), "in between: light or dark, nothing else");
	eq(A(look_for(0.9f, "black")) == A(plate_look("dark", "black", {}, {clock})), true, "auto's dark is Dark's, on any theme");
	eq(look_for(1).focus.r == 1 && look_for(1).focus.a > 0, true, "a dark card in focus lights up");
	eq(A(plate_look("auto", "dusk", map, {clock, weather, calendar})), std::string("12,18,46,0.55"), "auto: one bright part makes all dark");
	auto dark_left = map;
	for (int i = 0; i < 144; i++) if (i % 16 < 8) dark_left[i] = 0;
	eq(A(plate_look("auto", "dusk", dark_left, {clock, weather, calendar})), std::string("255,255,255,0.11"), "auto: all over dark, all light");
	eq(A(plate_look("light", "dusk", map, {clock})), std::string("255,255,255,0.11"), "light: always light");
	eq(A(plate_look("dark", "dusk", map, {clock})), std::string("12,18,46,0.55"), "dark: always dark");
	auto black = theme_map("black");
	eq((int)black.size(), 144, "the black theme's grid");
	eq(*std::max_element(black.begin(), black.end()) == 0.f, true, "the black theme is dark everywhere");
	auto dusk = theme_map("dusk");
	eq(dusk[0] < dusk[143], true, "the dusk theme gets brighter towards the bottom right");
	eq(A(plate_look("auto", "dusk", {}, {clock, weather, calendar})), std::string("255,255,255,0.11"), "on the dusk theme the plates are light");
}

static void weather () {
	auto d = parse_weather(R"({"current":{"temperature_2m":29.4,"weather_code":2,"is_day":1},"daily":{"time":["2026-10-24"],"weather_code":[2],"temperature_2m_max":[31.2],"temperature_2m_min":[25.1]}})", 0, 0, 1000);
	eq(d.has_value() && d->tempC == 29.4 && *d->highC == 31.2 && *d->lowC == 25.1 && d->code == 2 && d->isDay && d->at == 1000, true, "parses a forecast reply");
	eq(parse_weather("not json", 0, 0, 0).has_value(), false, "garbage is not a forecast");
	eq(parse_weather(R"({"current":{}})", 0, 0, 0).has_value(), false, "a reply without a temperature is rejected");
	eq(parse_weather(R"({"current":{"temperature_2m":5,"weather_code":0,"is_day":0}})", 0, 0, 5)->highC.has_value(), false, "missing daily values become null");
	eq((int)parse_cities("nope").size(), 0, "garbage gives no cities");
	auto cities = parse_cities(R"({"results":[{"name":"Pune","latitude":18.5,"longitude":73.8,"admin1":"Maharashtra","country":"India"},{"name":"Nowhere"}]})");
	eq((int)cities.size(), 1, "cities without coordinates are dropped");
	eq(cities[0].name + " / " + cities[0].detail, std::string("Pune / Maharashtra, India"), "city name and detail");
	eq(format_temp(21.6, "c"), std::string("22°"), "celsius rounds");
	eq(format_temp(0, "f"), std::string("32°"), "fahrenheit conversion");
	eq(format_temp(-0.2, "c"), std::string("0°"), "no negative zero");
	eq(format_temp(-2.5, "c"), std::string("-2°"), "halves round up, as Math.round");
	eq(describe(0, true).icon + describe(0, false).icon + describe(95, true).icon + describe(61, true).icon + describe(73, true).icon, std::string("sunmoonstormrainsnow"), "icons");
	// a full 7-day reply, hourly values for two days
	nlohmann::json times = nlohmann::json::array(), temps = nlohmann::json::array(), codes = nlohmann::json::array(), is_day = nlohmann::json::array();
	for (int day = 0; day < 2; day++)
		for (int h = 0; h < 24; h++) {
			char t[32];
			snprintf(t, sizeof t, "2026-10-2%dT%02d:00", 4 + day, h);
			times.push_back(t); temps.push_back(10 + h / 2.0); codes.push_back(h < 6 ? 0 : 2); is_day.push_back(h >= 6 && h < 19 ? 1 : 0);
		}
	nlohmann::json full = {
		{"current", {{"temperature_2m", 19.2}, {"apparent_temperature", 17.8}, {"weather_code", 2}, {"is_day", 1}, {"wind_speed_10m", 3.6}}},
		{"hourly", {{"time", times}, {"temperature_2m", temps}, {"weather_code", codes}, {"is_day", is_day}}},
		{"daily", {{"time", {"2026-10-24", "2026-10-25", "2026-10-26", "2026-10-27", "2026-10-28", "2026-10-29", "2026-10-30"}},
			{"weather_code", {2, 0, 3, 61, 63, 3, 0}}, {"temperature_2m_max", {22.1, 21, 19, 18, 17, 19, 21}}, {"temperature_2m_min", {14, 14, 13, 12, 11, 12, 13}}}}};
	auto f = parse_weather(full.dump(), 40.4, 49.8, 5000);
	eq((int)f->hourly.size(), 16, "every day has 8 points");
	eq(f->hourly[7].hour == 21 && f->hourly[7].tempC == 20.5 && !f->hourly[7].isDay && f->hourly[8].day == 1, true, "one every 3 hours");
	eq((int)f->daily.size(), 7, "seven days are read");
	set_lang("ru");
	auto v = weather_view(*f, "c", "Баку", 0);
	eq(v.temp + "|" + v.text + "|" + v.detail + "|" + v.city, std::string("19°|Переменная облачность|Ощущается как 18° · ветер 4 м/с|Баку"), "the card text, in Russian");
	std::string labels;
	for (auto &h : v.hours) labels += h.label + " ";
	eq(labels, std::string("00:00 03:00 06:00 09:00 12:00 15:00 18:00 21:00 "), "hour labels");
	eq(v.hours[0].icon + v.hours[3].icon + v.hours[7].icon, std::string("moonpartlycmoon"), "night hours get the moon");
	std::string days;
	for (auto &x : v.days) days += x.label + ", ";
	eq(days, std::string("Сегодня, Вс 25, Пн 26, Вт 27, Ср 28, Чт 29, Пт 30, "), "day labels");
	eq(v.days[3].icon + v.days[3].high + v.days[3].low, std::string("rain18°12°"), "a day's icon, high and low");
	auto tomorrow = weather_view(*f, "c", "Баку", 1);
	eq(std::to_string(tomorrow.day) + "|" + tomorrow.dayName + "|" + std::to_string(tomorrow.hours.size()), std::string("1|Воскресенье, 25 октября|8"), "another day: its own name and curve");
	eq(weather_view(*f, "c", "", 99).day, 6, "a day past the forecast is clamped");
	eq(v.dayName, std::string("Сегодня"), "today is called Сегодня");
	eq(weather_view(*f, "f", "", 0).temp, std::string("67°"), "fahrenheit");
	eq(weather_from_json(weather_json(*f))->hourly.size() == f->hourly.size(), true, "the saved reading reads back");
	set_lang("en");
}

static long long at (int y, int m, int d, int h = 0, int mi = 0) {
	std::tm t{};
	t.tm_year = y - 1900; t.tm_mon = m - 1; t.tm_mday = d; t.tm_hour = h; t.tm_min = mi; t.tm_isdst = -1;
	return (long long)mktime(&t) * 1000;
}
static std::string hhmm (long long ms) { std::time_t s = ms / 1000; std::tm t; localtime_r(&s, &t); char b[8]; snprintf(b, sizeof b, "%02d:%02d", t.tm_hour, t.tm_min); return b; }
static std::string ymd (long long ms) { std::time_t s = ms / 1000; std::tm t; localtime_r(&s, &t); char b[16]; snprintf(b, sizeof b, "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday); return b; }

static void calendar () {
	// a fixed zone (UTC+4, no daylight saving), as the web version's test
	setenv("TZ", "Asia/Baku", 1);
	tzset();
	set_lang("ru");
	std::vector<std::string> lines = {
		"BEGIN:VCALENDAR", "VERSION:2.0",
		"BEGIN:VEVENT", "UID:one@x", "DTSTART:20261024T140000Z", "DTEND:20261024T153000Z", "SUMMARY:Тренировка\\, зал", "LOCATION:Улица ", " Нефтчиляр", "BEGIN:VALARM", "TRIGGER:-PT10M", "ACTION:DISPLAY", "END:VALARM", "END:VEVENT",
		"BEGIN:VEVENT", "UID:weekly@x", "DTSTART;TZID=Asia/Baku:20261001T100000", "DTEND;TZID=Asia/Baku:20261001T110000", "RRULE:FREQ=WEEKLY;BYDAY=MO,TH", "EXDATE;TZID=Asia/Baku:20261026T100000", "SUMMARY:Планёрка", "END:VEVENT",
		"BEGIN:VEVENT", "UID:weekly@x", "RECURRENCE-ID;TZID=Asia/Baku:20261029T100000", "DTSTART;TZID=Asia/Baku:20261029T160000", "DTEND;TZID=Asia/Baku:20261029T170000", "SUMMARY:Планёрка (перенос)", "END:VEVENT",
		"BEGIN:VEVENT", "UID:trip@x", "DTSTART;VALUE=DATE:20261027", "DTEND;VALUE=DATE:20261029", "SUMMARY:Командировка", "END:VEVENT",
		"BEGIN:VEVENT", "UID:gone@x", "DTSTART:20261024T100000Z", "DTEND:20261024T110000Z", "STATUS:CANCELLED", "SUMMARY:Отменено", "END:VEVENT",
		"BEGIN:VEVENT", "UID:daily@x", "DTSTART;TZID=Asia/Baku:20261024T080000", "DTEND;TZID=Asia/Baku:20261024T083000", "RRULE:FREQ=DAILY;COUNT=3", "SUMMARY:Зарядка", "END:VEVENT",
		"BEGIN:VEVENT", "UID:monthly@x", "DTSTART;TZID=Asia/Baku:20260901T190000", "DTEND;TZID=Asia/Baku:20260901T200000", "RRULE:FREQ=MONTHLY;BYDAY=2TU", "SUMMARY:Клуб", "END:VEVENT",
		"BEGIN:VEVENT", "UID:bd@x", "DTSTART;VALUE=DATE:20100105", "DTEND;VALUE=DATE:20100106", "RRULE:FREQ=YEARLY", "SUMMARY:День рождения", "END:VEVENT",
		"END:VCALENDAR"};
	std::string feed;
	for (size_t i = 0; i < lines.size(); i++) feed += (i ? "\r\n" : "") + lines[i];
	long long from = at(2026, 10, 20), to = at(2026, 11, 20);
	auto all = expand_ics(feed, from, to);
	auto on = [&] (const std::string &title) {
		std::vector<std::string> out;
		for (auto &e : all) if (e.title == title) out.push_back(ymd(e.start) + " " + hhmm(e.start) + "-" + hhmm(e.end));
		return out;
	};
	eq(on("Тренировка, зал"), S{"2026-10-24 18:00-19:30"}, "UTC time becomes local time; the escaped comma is unescaped");
	std::string place;
	for (auto &e : all) if (e.title == "Тренировка, зал") place = e.place;
	eq(place, std::string("Улица Нефтчиляр"), "a folded line is joined");
	eq(on("Планёрка"), S{"2026-10-22 10:00-11:00", "2026-11-02 10:00-11:00", "2026-11-05 10:00-11:00", "2026-11-09 10:00-11:00", "2026-11-12 10:00-11:00", "2026-11-16 10:00-11:00", "2026-11-19 10:00-11:00"}, "weekly Monday/Thursday without the skipped and the moved");
	eq(on("Планёрка (перенос)"), S{"2026-10-29 16:00-17:00"}, "the moved occurrence at its new time");
	std::string trip;
	for (auto &e : all) if (e.title == "Командировка") trip += ymd(e.start) + " " + ymd(e.end) + (e.allDay ? " allday" : "");
	eq(trip, std::string("2026-10-27 2026-10-29 allday"), "an all-day event spans whole days");
	eq(on("Отменено"), S{}, "cancelled events are left out");
	eq(on("Зарядка"), S{"2026-10-24 08:00-08:30", "2026-10-25 08:00-08:30", "2026-10-26 08:00-08:30"}, "COUNT limits a daily repeat");
	eq(on("Клуб"), S{"2026-11-10 19:00-20:00"}, "monthly on the second Tuesday");
	eq(on("День рождения"), S{}, "a yearly event outside the range");
	std::vector<std::string> jan;
	for (auto &e : expand_ics(feed, at(2027, 1, 1), at(2027, 1, 31))) if (e.title == "День рождения") jan.push_back(ymd(e.start));
	eq(jan, S{"2027-01-05"}, "a yearly all-day event repeats");
	eq((int)expand_ics("garbage", from, to).size(), 0, "garbage gives no events");

	long long now = at(2026, 10, 24, 18, 30);
	std::string named = feed;
	named.replace(named.find("BEGIN:VCALENDAR\r\n"), 17, "BEGIN:VCALENDAR\r\nX-WR-CALNAME:Работа\r\n");
	auto part = parse_calendar(named, "https://example.com/a.ics", "", 0, now);
	auto second = parse_calendar("BEGIN:VCALENDAR\r\nX-WR-CALNAME:Семья\r\nBEGIN:VEVENT\r\nUID:s\r\nDTSTART:20261024T150000Z\r\nDTEND:20261024T160000Z\r\nSUMMARY:Ужин\r\nEND:VEVENT\r\nEND:VCALENDAR", "/media/developer/cals/family.ics", "12-34", 1, now);
	auto data = join_calendars("k", {*part, *second}, now);
	eq(part->source.name + "|" + second->source.name + "|" + std::to_string(data.sources.size()), std::string("Работа|Семья|2"), "calendar names, joined");
	eq(dots_of_day(&data, start_of_day(now, 0)), std::vector<int>{0, 1}, "dots: one per calendar with events that day");
	eq(parse_calendar("<html>nope</html>", "u", "", 0, now).has_value(), false, "not a calendar");
	std::string today;
	for (auto &e : events_of_day(&data, start_of_day(now, 0), now, true)) today += e.time + " " + e.title + " " + e.state + "; ";
	eq(today, std::string("08:00 Зарядка past; 18:00 Тренировка, зал now; 19:00 Ужин next; "), "today: past, now, next");
	eq(events_of_day(&data, start_of_day(now, 0), now, true)[1].note, std::string("сейчас"), "the running event says so");
	auto trip_day = events_of_day(&data, start_of_day(now, 3), now, true);
	eq(trip_day.size() == 1 ? trip_day[0].time + " " + trip_day[0].title : std::string(), std::string("Весь день Командировка"), "all-day events first with Весь день");
	eq((int)events_of_day(&data, start_of_day(now, -1), now, true).size(), 0, "a day without events");
	eq(events_of_day(&data, start_of_day(now, 1), now, true)[0].state, std::string("next"), "a future day is not dimmed");
	eq(relative_day(0) + "|" + relative_day(1) + "|" + relative_day(-1) + "|" + relative_day(5) + "|" + relative_day(-3) + "|" + relative_day(21),
		std::string("Сегодня|Завтра|Вчера|Через 5 дней|3 дня назад|Через 21 день"), "relative day words");
	eq(day_heading(now, 0), std::string("Суббота, 24 октября"), "the day heading");
	eq(normalize_calendar_url("webcal://calendar.google.com/a b.ics"), std::string("https://calendar.google.com/ab.ics"), "webcal becomes https");
	eq(std::string(valid_calendar_path("/media/developer/cals/a.ics") ? "1" : "0") + (valid_calendar_path("/a/../b") ? "1" : "0") + (valid_calendar_path("/a b; rm") ? "1" : "0"), std::string("100"), "plain paths only");
	auto back = calendar_from_json(calendar_json(data));
	eq(back && back->events.size() == data.events.size() && back->sources[1].name == "Семья", true, "the saved copy reads back");
	set_lang("en");
	unsetenv("TZ");
	tzset();
}

static std::string fixture (const char *name) {
	std::string out;
	read_file(std::string("tests/fixtures/feeds/") + name, out);
	return out;
}

static void feeds () {
	auto g = parse_feed(fixture("google.xml"), 0);
	eq((int)g.size(), 3, "Google News: every item");
	eq(g[0].title + " | " + g[0].source, std::string("Il comune apre tre nuove piste ciclabili | Giornale di Esempio"), "the outlet is taken off the headline");
	eq(g[0].link, std::string("https://news.google.com/rss/articles/AAA111?oc=5"), "link");
	eq(g[0].at == parse_date("Wed, 28 Oct 2026 08:30:00 GMT") && g[0].at == 1793176200000LL, true, "date");
	eq(g[0].summary.find('<') == std::string::npos && g[0].summary.find("Biciclette, la mappa dei percorsi") != std::string::npos, true, "escaped HTML becomes plain text");
	eq(g[1].summary, std::string(""), "a description that only repeats the headline is dropped");
	auto r = parse_feed(fixture("rss-cdata.xml"), 1);
	eq(r[0].title + " | " + r[0].summary + " | " + r[0].source, std::string("Rail operator adds night trains & sleeper cars | New routes start in December. | Example News - World"), "CDATA, the channel's name");
	eq(r[2].at, parse_date("2026-10-28T06:15:00Z"), "dc:date is read");
	auto a = parse_feed(fixture("atom.xml"), 2);
	eq(a[0].title + " | " + a[0].link, std::string("New rules for e-scooters & bikes from Monday | https://tech.example/scooters"), "Atom: the alternate link");
	eq(a[0].at, parse_date("2026-10-28T07:20:00Z"), "Atom: published before updated");
	eq(a[1].link + " | " + std::to_string(a[1].at) + " | " + a[1].summary, std::string("https://tech.example/undated | 0 | Body text."), "Atom: a link without rel, no date, an XHTML body");
	auto rdf = parse_feed(fixture("rdf.xml"), 3);
	eq(std::to_string(rdf.size()) + " | " + rdf[0].title + " | " + rdf[0].source, std::string("1 | Tram line 14 diverted until Sunday | Old Example"), "RSS 1.0 (RDF)");
	eq((int)parse_feed("not a feed", 0).size(), 0, "garbage gives nothing");
	auto ru = parse_feed(decode_text(fixture("cp1251.xml"), ""), 0);
	eq(ru.empty() ? std::string() : ru[0].title, std::string("Трамвай пойдёт по новому маршруту"), "a windows-1251 feed reads right");
	eq(decode_text("ok", "text/plain; charset=x-unknown"), std::string("ok"), "an unknown encoding falls back to UTF-8");
	long long now = parse_date("2026-10-28T09:00:00Z");
	auto all = join_feeds({g, r, a, rdf}, now);
	bool old = false, sorted = true;
	int same = 0;
	for (size_t i = 0; i < all.size(); i++) {
		if (all[i].title.find("Vecchia notizia") == 0) old = true;
		if (all[i].title == "Il comune apre tre nuove piste ciclabili") same++;
		if (i + 1 < all.size() - 1 && all[i].at < all[i + 1].at) sorted = false;
	}
	eq(old, false, "headlines older than two days are left out");
	eq(same, 1, "the same story from two feeds shows once");
	eq(sorted, true, "newest first");
	eq(all.back().title, std::string("A headline without a date"), "headlines without a date come last");
	eq((int)join_feeds({{FeedItem{"Future", "x", "", "", now + 5 * 3600000, 0}}}, now).size(), 0, "a date far in the future is not believed");
	eq(google_news_url("IT", "ru"), std::string("https://news.google.com/rss?hl=it&gl=IT&ceid=IT%3Ait"), "Italy in Russian: the Italian edition");
	eq(google_news_url("UA", "ru"), std::string("https://news.google.com/rss?hl=ru&gl=UA&ceid=UA%3Aru"), "Ukraine in Russian");
	eq(google_news_url("UA", "en"), std::string("https://news.google.com/rss?hl=uk&gl=UA&ceid=UA%3Auk"), "Ukraine in English: its first edition");
	eq(google_news_url("BR", "pt"), std::string("https://news.google.com/rss?hl=pt-BR&gl=BR&ceid=BR%3Apt-419"), "Brazil in Portuguese");
	eq(google_news_url("XX", "en"), std::string("https://news.google.com/rss?hl=en-US&gl=US&ceid=US%3Aen"), "an unknown country is the United States");
	eq(country_of("DE", "ru-RU", "ru") + country_of("", "ru-RU", "ru") + country_of("", "en-CA", "fr"), std::string("DERUCA"), "the chosen country, or the TV's");
	eq(country_of("", "en-US", "de") + country_of("", "pl", "pl") + country_of("", "", "ja") + country_of("", "de-DE", "uk"), std::string("DEPLJPUA"), "else the language's own");
	eq(std::to_string(headlines_per_page(686, 1)) + std::to_string(headlines_per_page(470, 1)) + std::to_string(headlines_per_page(686, 1.5)), std::string("533"), "headlines per page");
	auto age = age_of(now - 5 * 60000, now), age2 = age_of(now - 3 * 3600000, now);
	eq(std::to_string(age.first) + age.second + std::to_string(age2.first) + age2.second + age_of(0, now).second, std::string("5min3h"), "the age of a headline");
	eq(country_name("IT", "ru"), std::string("Италия"), "a country's name in the language");
}

static std::string qfix (const std::string &name) {
	std::string out;
	read_file("tests/fixtures/quote/" + name, out);
	return out;
}

static size_t chars (const std::string &s) { size_t n = 0; for (unsigned char c : s) if ((c & 0xC0) != 0x80) n++; return n; }

static void quote () {
	auto daily = [] (const std::string &l) { return daily_from_page(qfix("main-" + l + ".html"), l); };
	auto en = daily("en");
	eq(en ? en->author + " | " + en->text.substr(0, 40) : std::string(), std::string("Jonas Salk | I speak about universal evolution and te"), "en: the quote and its author under the heading");
	auto es = daily("es");
	eq(es ? es->text + " | " + es->author : std::string(), std::string("Mis ventanas ya no dan a los álamos y los ríos de España | Rafael Alberti"), "es: quote marks drawn as pictures are left out");
	auto pl = daily("pl");
	eq(pl ? pl->text + " | " + pl->author : std::string(), std::string("Dla chcącego nie ma nic trudnego. | Erazm z Rotterdamu"), "pl: the author on the next line");
	auto zh = daily("zh");
	eq(zh ? zh->author : std::string(), std::string("歌德"), "zh: the author after ——");
	eq(zh && zh->text.size() >= 12 ? zh->text.substr(zh->text.size() - 12) : std::string(), std::string("人宣傳。"), "zh: the dash and the author are cut from the quote");
	auto ko = daily("ko");
	eq(ko ? ko->text + " | " + ko->author : std::string(), std::string("내일은 믿지 마라. 오늘을 즐겨라.(carpe diem) | 호라티우스, 《송시》"), "ko");
	auto tr_ = daily("tr");
	eq(tr_ ? tr_->author : std::string(), std::string("Bob Marley"), "tr: the author after a dash on its own line");
	auto pt = daily("pt");
	eq(pt ? pt->author + pt->text.substr(0, 1) : std::string(), std::string("Bill GatesD"), "pt: the quote marks taken off");
	eq(daily("uk").has_value(), false, "uk: no quote chosen today gives none");
	auto it = daily_from_feed(qfix("feed-it.xml"), "it");
	eq(it ? it->text + " | " + it->author : std::string(), std::string("La realtà è ciò che si rifiuta di sparire anche quando smetti di crederci. | Philip K. Dick"), "it: the newest entry of the feed");
	eq(daily_from_page("<p>nothing here</p>", "en").has_value(), false, "a page without the box gives none");
	eq(daily_from_page("<h2>Quote of the day</h2><p>Hi.</p><p>— X</p>", "de").has_value(), false, "a language without an editors' quote gives none");
	auto from = [] (const std::string &l) { return quotes_from_page(qfix("author-" + l + ".html"), l); };
	auto ru = from("ru"), de = from("de"), fr = from("fr"), ja = from("ja");
	eq(ru.size() > 50 && de.size() > 200 && fr.size() > 15 && ja.size() > 20, true, "every page gives quotes (ru " + std::to_string(ru.size()) + ", de " + std::to_string(de.size()) + ", fr " + std::to_string(fr.size()) + ", ja " + std::to_string(ja.size()) + ")");
	eq(std::find(ru.begin(), ru.end(), "Благородный муж — не орудие.") != ru.end(), true, "ru: a short saying is kept");
	bool about = false, verse = false, slashes = false, vers = false;
	for (auto &q : de) { if (q.rfind("Licht war seine letzte Forderung", 0) == 0) about = true; if (q.find('\n') != std::string::npos) verse = true; if (q.find("//") != std::string::npos) slashes = true; if (q.find("– Vers") != std::string::npos) vers = true; }
	for (auto &q : fr) if (q.rfind("HUGO (Victor)", 0) == 0) about = true;
	eq(about, false, "quotes about the author are left out");
	eq(verse && !slashes, true, "de: lines of verse written with // become lines");
	eq(vers, false, "de: the source after the quote is dropped");
	bool ja_ok = true;
	for (auto &q : ja) if (q.find("--") != std::string::npos || chars(q) < 6 || chars(q) > 90) ja_ok = false;
	eq(ja_ok, true, "ja: no source after --, short in characters");
	bool len_ok = true;
	for (auto *list : {&ru, &de, &fr}) for (auto &q : *list) if (chars(q) < 25 || chars(q) > 220 || q.find("http") != std::string::npos || q.find("ISBN") != std::string::npos) len_ok = false;
	eq(len_ok, true, "other languages: 25 to 220 characters, no links or book references");
	auto box = quotes_from_page(qfix("author-ru-q.html"), "ru");
	eq(box.size() > 20, true, "ru: the quotes in the boxes are read (" + std::to_string(box.size()) + ")");
	eq(std::find(box.begin(), box.end(), "Неусыпный труд все препятствия преодолевает.") != box.end(), true, "ru: a quote from a box");
	bool see = false, attributed = false, poem = false;
	for (auto &q : box) { if (q.rfind("см.", 0) == 0 || q.find("Шувалову»") != std::string::npos) see = true; if (q.rfind("А математику уже затем", 0) == 0) attributed = true; if (q.rfind("Лучи от нас склонились прочь;", 0) == 0 && q.find('\n') != std::string::npos) poem = true; }
	eq(see, false, "ru: a line that points elsewhere is not a quote");
	eq(attributed, false, "ru: the boxes under Приписываемое are left out");
	eq(poem, true, "ru: a poem in a box keeps its lines");
	auto page = [] (const std::string &s, const std::string &i) { return "<h2>" + s + "</h2><ul><li>" + i + "</li></ul>"; };
	eq((int)quotes_from_page(page("Ссылки", "x") + page("Цитаты", "Категория:Произведения Альберта Эйнштейна, одна длинная строка"), "ru").size(), 0, "ru: a category link is not a quote");
	eq(std::string(skipped_section("Общие источники") ? "1" : "0") + (skipped_section("Статьи о произведениях") ? "1" : "0") + (skipped_section("О Льве Толстом") ? "1" : "0") + (skipped_section("Про Тараса Шевченка") ? "1" : "0") + (skipped_section("О любви и дружбе") ? "1" : "0"), std::string("11110"), "ru, uk: sources and quotes about a person are skipped");
	eq(drop_source("Поступать несправедливо хуже, чем терпеть несправедливость. — «Горгий», 473a"), std::string("Поступать несправедливо хуже, чем терпеть несправедливость."), "the source after a dash is dropped");
	eq(drop_source("Я серьезно убежден, что миром правят совсем сумасшедшие. (Дневник: 12 июля 1900 г.)"), std::string("Я серьезно убежден, что миром правят совсем сумасшедшие."), "a source in brackets with a date is dropped");
	eq(drop_source("— На том стою, и не могу иначе. Аминь. (Защитная речь в Вормсе (18 апреля 1521 года))"), std::string("На том стою, и не могу иначе. Аминь."), "a dash before a line of speech");
	eq(std::string(skipped_section("О Достоевском / О произведениях", "Фёдор Михайлович Достоевский") ? "1" : "0") + (skipped_section("О России", "Фёдор Михайлович Достоевский") ? "1" : "0") + (skipped_section("О Достоевском") ? "1" : "0"), std::string("100"), "a heading about the author");
	eq((int)quotes_from_page(page("Цитаты", "Основная категория: христианин по обязанности, ну и так далее,"), "ru").size(), 0, "a line cut off mid-sentence");
	eq(drop_source("Кто ясно мыслит — ясно излагает."), std::string("Кто ясно мыслит — ясно излагает."), "a dash inside the sentence stays");
	eq(clean_quote("Я вижу что могло бы быть и спрашиваю «почему нет?»", "ru"), std::string("Я вижу что могло бы быть и спрашиваю «почему нет?»"), "a closing » inside the text stays");
	eq(skipped_section("Überprüft / Gedichte"), false, "Überprüft is not über");
	eq(skipped_section("Citations le concernant / Sainte-Beuve") && skipped_section("孔子に関する引用") && skipped_section("See also"), true, "about-the-author and link sections at any depth");
	eq(clean_quote("„Edel sei der Mensch, // Hülfreich und gut!“ – Vers 1–6", "de"), std::string("Edel sei der Mensch,\nHülfreich und gut!"), "quote marks, the source after them and //");
	eq(clean_quote("«Texte entre guillemets»", "fr"), std::string("Texte entre guillemets"), "French quote marks");
	eq(clean_quote("A sentence.[1]", "en"), std::string("A sentence."), "note numbers dropped");
	eq(author_name("Confucius (philosopher)"), std::string("Confucius"), "the author without the brackets");
	auto authors = parse_authors(qfix("authors-ru.json"));
	eq(std::to_string(authors.size()) + " " + (authors.empty() ? "" : authors[0]), std::string("30 Уильям Шекспир"), "the Wikidata reply gives the page titles");
	eq((int)parse_authors("nope").size(), 0, "a broken reply gives none");
	long long day = at(2026, 10, 28, 9), later = at(2026, 10, 28, 23, 50), next = at(2026, 10, 29, 0, 10);
	eq(*pick_author(authors, day) == *pick_author(authors, later), true, "the same author all day");
	eq(*pick_author(authors, day) != *pick_author(authors, next), true, "another author the next day");
	eq(*pick_author(authors, day, 1) != *pick_author(authors, day, 0), true, "the next try picks another author");
	eq(*pick_quote(ru, day) == *pick_quote(ru, later), true, "the same quote all day");
	eq(day_key(day), std::string("2026-10-28"), "the local day");
	eq(parse_url("ru", "Конфуций").find("page=%D0%9A") != std::string::npos, true, "the page address encodes the title");
	eq(has_daily("ru") || has_daily("fr") || has_daily("de") || has_daily("ja"), false, "ru, fr, de and ja always pick by the date");
}

static void settings () {
	set_lang("ru");
	eq(step_value(std::vector<int>{1, 2, 3}, 3, 1, true), 1, "OK wraps round");
	eq(step_value(std::vector<int>{1, 2, 3}, 3, 1, false), 3, "arrow stops at the end");
	eq(step_value(std::vector<int>{1, 2, 3}, 1, -1, false), 1, "arrow stops at the start");
	eq(step_value(std::vector<int>{1, 2, 3}, 99, 1, true), 2, "unknown value starts from the first");
	UserConfig c0;
	SystemInfo info;
	bool changed = true;
	eq(apply_pref(c0, "iconScale", Press::right).iconScale, 110, "right arrow: icon size 100 -> 110");
	eq(apply_pref(c0, "iconScale", Press::left).iconScale, 90, "left arrow: icon size 100 -> 90");
	UserConfig big = c0; big.iconScale = 140;
	eq(apply_pref(big, "iconScale", Press::right).iconScale, 140, "right arrow stops at the maximum");
	eq(apply_pref(big, "iconScale", Press::ok).iconScale, 50, "OK wraps to the minimum (50%)");
	eq(apply_pref(c0, "theme", Press::ok).theme, std::string("black"), "OK steps to the next theme");
	eq(apply_pref(c0, "showDate", Press::ok).clock.showDate, false, "OK flips a switch");
	apply_pref(c0, "showDate", Press::right, &changed);
	eq(changed, false, "arrows leave a switch alone");
	eq(apply_pref(c0, "clockSize", Press::right).clock.size, 170, "clock size steps by 10");
	eq(apply_pref(c0, "clockWeight", Press::ok).clock.weight, 300, "thickness: thin -> light");
	apply_pref(c0, "wallpaper", Press::ok, &changed);
	eq(changed, false, "the wallpaper row is an action, not a value");
	apply_pref(c0, "nonsense", Press::ok, &changed);
	eq(changed, false, "unknown option does nothing");
	UserConfig messy = c0;
	messy.theme = "grey"; messy.iconScale = 50; messy.sideMargin = 0; messy.bottomMargin = 0; messy.clockFormat = "12h"; messy.clock.size = 80; messy.clock.color = "ember"; messy.clock.font = "serif";
	messy.appIds = S{"a"}; messy.wallpaper.mode = "image"; messy.wallpaper.path = "/a.jpg";
	UserConfig r = reset_look(messy);
	eq(S{r.theme, std::to_string(r.iconScale), std::to_string(r.sideMargin), std::to_string(r.bottomMargin), r.clockFormat, std::to_string(r.clock.size), r.clock.color, r.clock.font},
		S{"dusk", "100", "80", "50", "24h", "160", "white", "inter"}, "reset restores the look, the clock and its font");
	eq(S{r.appIds ? (*r.appIds)[0] : "", r.wallpaper.mode}, S{"a", "image"}, "reset keeps apps and wallpaper");
	for (const char *tab : {"look", "widgets", "wallpaper", "home"}) {
		auto list = options_for(tab, c0, info);
		eq(list.size() >= 1 && list.size() <= 20, true, std::string(tab) + " tab has a list of options");
	}
	auto look = options_for("look", c0, info);
	S texts, groups;
	for (auto &o : look) { texts.push_back(o.value >= 0 ? "-" : o.valueText); groups.push_back(o.group); }
	eq(texts, S{"Вечернее небо", "100%", "У всех", "80 px", "50 px", "Inter (по умолчанию)", "100%", "Авто", "-", "Авто"}, "Appearance shows current values, the colour theme first (switches have no text; no frosted glass without a picture)");
	eq(groups, S{"", "Ряд приложений", "Ряд приложений", "Ряд приложений", "Ряд приложений", "Виджеты", "Виджеты", "Виджеты", "Виджеты", "Виджеты"}, "Appearance: the colour theme, then two groups");
	eq(S{c0.pixelShift, apply_pref(c0, "pixelShift", Press::ok).pixelShift, apply_pref(c0, "pixelShift", Press::left).pixelShift}, S{"auto", "on", "auto"}, "the pixel shift: OK turns it on for good");
	UserConfig pic = c0; pic.wallpaper.mode = "image"; pic.wallpaper.path = "/a.jpg";
	bool glass = false;
	for (auto &o : options_for("look", pic, info)) glass = glass || o.id == "glass";
	eq(glass, true, "frosted glass appears with a picture");
	auto picture_text = [&] (const SystemInfo &si) { for (auto &o : options_for("wallpaper", pic, si)) if (o.id == "wallpaper") return o.valueText; return std::string(); };
	SystemInfo titled = info;
	titled.picture_title = "Matterhorn from Lake Grindji";
	eq(picture_text(info) + " | " + picture_text(titled), std::string("a.jpg | Matterhorn from Lake Grindji"), "the picture by its file name, a photo of the collection by its title");
	auto l1 = apply_pref(c0, "showLabels", Press::ok), l2 = apply_pref(l1, "showLabels", Press::ok);
	eq(S{show(l1.showLabels), show(l1.labelsSelectedOnly), show(l2.showLabels)}, S{"true", "true", "false"}, "App names: all, then only the selected one, then none");
	S ids, hg;
	for (auto &o : options_for("home", c0, info)) { ids.push_back(o.id); hg.push_back(o.group); }
	eq(ids, S{"language", "resetLook", "homeMethod", "homeCleanup", "about", "checkUpdates", "update", "restartTv"}, "Home screen: language, reset, how the launcher opens, remove everything, about, the update check and its row, and Restart the TV last");
	eq(hg, S{"", "", "Запуск лаунчера", "Запуск лаунчера", "О лаунчере", "О лаунчере", "О лаунчере", ""}, "Home screen: the launch rows, and About with the update check, in their groups");
	// the update row: only while there is a new version (and the check is on), worded by its step, with a bar while it downloads
	auto update_row = [&] (const UserConfig &c, const SystemInfo &si) -> std::optional<OptionItem> {
		for (auto &o : options_for("home", c, si)) if (o.id == "update") return o;
		return std::nullopt;
	};
	SystemInfo up = info;
	up.update.info.version = "2.0.1";
	up.update.step = UpdateState::Step::available;
	auto r1 = update_row(c0, up);
	eq(r1 && r1->label == "Вышла версия 2.0.1" && r1->valueText == "Скачать" && r1->progress < 0, true, "a new version: Download");
	up.update.step = UpdateState::Step::downloading; up.update.progress = 0.45;
	auto r2 = update_row(c0, up);
	eq(r2 && r2->valueText == "45%" && std::fabs(r2->progress - 0.45f) < 1e-6, true, "downloading: the per cent and the bar");
	up.update.step = UpdateState::Step::ready;
	auto r3 = update_row(c0, up);
	eq(r3 && r3->label == "Версия 2.0.1 готова к установке" && r3->valueText == "Установить" && r3->progress < 0, true, "downloaded: Install");
	UserConfig off = c0; off.checkUpdates = false;
	eq(update_row(off, up).has_value(), false, "no row with the check off");
	auto r0 = update_row(c0, info);
	eq(r0 && r0->label == "Проверить наличие новой версии" && r0->valueText == "Проверить", true, "with the check on and nothing found yet: Check by hand");
	SystemInfo st = info;
	st.update.checking = true;
	auto rc = update_row(c0, st);
	eq(rc && rc->label == "Ищу новую версию…" && rc->valueText.empty(), true, "looking");
	st.update.checking = false; st.update.latest = true;
	eq(update_row(c0, st)->label, std::string("Установлена последняя версия"), "looked: the latest");
	st.update.unreachable = true;
	auto ru = update_row(c0, st);
	eq(ru && ru->label == "Не удалось связаться с GitHub" && ru->valueText == "Ещё раз", true, "no network: try again");
	// without root: the rows that run commands greyed out, with why
	SystemInfo nr = info;
	nr.root = (int)Root::no; nr.root_detail = "timeout";
	S grey;
	for (auto &o : options_for("home", c0, nr)) if (!o.disabled.empty()) grey.push_back(o.id);
	eq(grey, S{"homeMethod", "homeCleanup", "restartTv"}, "no root: How the launcher opens, Remove everything and Restart the TV greyed out");
	bool none_grey = true;
	for (auto &o : options_for("home", c0, info)) if (!o.disabled.empty()) none_grey = false;
	eq(none_grey, true, "root not known yet: nothing greyed out");
	eq(root_reason(Root::no, "Denied method call \"exec\" for category \"/\"").find("webos26-access.sh install") != std::string::npos, true, "webOS 26: the command to run");
	eq(root_reason(Root::no, "timeout").find("служба Homebrew Channel не запущена") != std::string::npos, true, "no answer: the Homebrew Channel is not running");
	eq(root_reason(Root::no, "Permission denied"), std::string("Нужен рутованный ТВ: ТВ отклонил команду. (ответ ТВ: Permission denied)"), "another refusal: what the TV said");
	eq(root_reason(Root::yes, "").empty() && root_reason(Root::unknown, "x").empty(), true, "root, or not known: usable");
	SystemInfo nu = nr;
	nu.update.info.version = "2.0.1"; nu.update.step = UpdateState::Step::ready;
	auto rn = update_row(c0, nu);
	eq(rn && !rn->disabled.empty() && rn->valueText.empty(), true, "no root: a new version is shown, but not installed from here");
	nu.update.step = UpdateState::Step::none;
	eq(update_row(c0, nu)->disabled.empty(), true, "no root: looking by hand still works");
	eq(apply_pref(c0, "checkUpdates", Press::ok).checkUpdates == !c0.checkUpdates && c0.checkUpdates, true, "the check is on at first, OK switches it");
	UserConfig iv = c0; iv.wallpaper.intervalSec = 900;
	eq(apply_pref(iv, "interval", Press::ok).wallpaper.intervalSec, 1800, "the slideshow interval goes 15 minutes -> 30 minutes");
	eq(interval_text(1800), std::string("30 минут"), "the 30-minute interval reads \"30 minutes\"");
	eq(interval_text(60), std::string("1 минута"), "interval text");
	eq(interval_text(3600), std::string("1 час"), "interval text hour");
	UserConfig cf = c0;
	S fonts;
	for (int i = 0; i < 6; i++) { cf = apply_pref(cf, "clockFont", Press::ok); fonts.push_back(cf.clock.font); }
	eq(fonts, S{"tv", "poppins", "oswald", "mono", "serif", "inter"}, "OK steps through the six fonts and wraps");
	UserConfig mono = c0; mono.clock.font = "mono";
	std::string font_text;
	for (auto &o : options_for("look", mono, info)) if (o.id == "clockFont") font_text = o.valueText;
	eq(font_text, std::string("Roboto Mono (цифровой)"), "the font option (in Appearance, with the widgets) reads its name");
	UserConfig w = c0;
	w = apply_pref(w, "weatherOn", Press::ok);
	eq(w.weather.on, false, "the weather stays off without a city");
	set_lang("en");
	eq(options_for("home", c0, info)[2].valueText, std::string("Unknown"), "how the launcher opens: unknown until the TV is asked");
}

static void gallery () {
	set_lang("ru");
	auto l = parse_listing("Zeta/\nalpha/\n._a.jpg\n.hidden/\nb.JPG\na.png\nnotes.txt\nclip.mp4\nsea.webp\n\n");
	S names;
	for (auto &e : l) names.push_back(e.name + (e.dir ? "/" : ""));
	eq(names, S{"alpha/", "Zeta/", "a.png", "b.JPG"}, "listing filters and sorts (webp is not decoded here, so not listed)");
	auto m = parse_mounts("rootfs / rootfs rw 0 0\n/dev/sda1 /tmp/usb/sda/sda1 vfat rw 0 0\n/dev/mmcblk0p9 /media/internal ext4 rw 0 0\n/dev/sdb1 /tmp/usb/sdb/My\\040Stick exfat rw 0 0\n");
	S roots;
	for (auto &r : m) roots.push_back(r.label + "|" + r.path);
	eq(roots, S{"USB-накопитель 1|/tmp/usb/sda/sda1", "USB-накопитель 2|/tmp/usb/sdb/My Stick"}, "mounts parse");
	m = parse_mounts("/dev/sda1 /tmp/usb/sda/sda1 vfat rw 0 0\n");
	eq(m.size() == 1 ? m[0].label : "", std::string("USB-накопитель"), "single usb label");
	eq(safe_path("/ok/path"), true, "safe path");
	eq(safe_path("/bad\npath"), false, "newline path rejected");
	eq(safe_path("relative/path"), false, "relative rejected");
	set_lang("en");
}

static void uninstall () {
	const std::string DEV = "/media/developer/apps/usr/palm/applications/org.example.app/icon.png";
	const std::string STORE = "/media/cryptofs/apps/usr/palm/applications/netflix/icon.png";
	const std::string SYS = "/usr/palm/applications/some.system.app/icon.png";
	auto v = [] (const std::string &id, const std::string &icon, int removable) { return uninstall_verdict(id, icon, removable).ok; };
	eq(v("tld.my.simplelauncher", DEV, 1), false, "the launcher itself is never offered");
	eq(v("tld.my.simplelauncher.native", DEV, 1), false, "nor the native one");
	eq(v("com.webos.app.home", "", 1), false, "LG Home is never offered");
	eq(v("org.webosbrew.hbchannel", DEV, 1), false, "the Homebrew Channel is never offered");
	eq(v("com.webos.app.settings", "", 1), false, "system apps are never offered, even if the TV says removable");
	eq(v("com.palm.app.browser", SYS, -1), false, "old Palm system apps are not offered");
	eq(v("netflix", STORE, -1), true, "an app from the LG Content Store is offered");
	eq(v("org.example.app", DEV, -1), true, "an app installed in developer mode is offered");
	eq(v("some.system.app", SYS, -1), false, "an app in the system folder is not offered");
	eq(v("x.y", "", 1), true, "the TV saying removable is enough");
	eq(v("x.y", DEV, 0), false, "the TV saying not removable wins over the folder");
	eq(v("x.y", "", -1), false, "no evidence that the user installed it: not offered");
	eq(v("bad id;rm -rf /", "", 1), false, "an ID that is not plain text is never offered");
	int refused = 0;
	for (const char *bad : {"x'; reboot; '", "a b", "", "$(id)", "a\"b", "a;b", "a`b`"}) if (uninstall_command(bad).empty()) refused++;
	eq(refused, 7, "anything that is not a plain app ID is refused (no shell injection)");
	eq(parse_uninstall("x\nremoved\n"), std::string("removed"), "result: removed");
	eq(parse_uninstall("still-installed"), std::string("still-installed"), "result: still installed");
	eq(parse_uninstall("garbage"), std::string("failed"), "result: anything else is a failure");
	// the real command against a fake luna-send, as the web test does
	char dir[] = "/tmp/fake-uninstall-XXXXXX";
	if (!mkdtemp(dir)) { eq(false, true, "temp folder"); return; }
	std::string d = dir;
	write_file(d + "/luna-send", "#!/bin/sh\ncase \"$*\" in\n  *getAppInfo*) if [ -f \"$STATE\" ]; then echo '{\"returnValue\":true}'; else echo '{\"returnValue\":false}'; fi ;;\n"
		"  *dev/remove*) echo dev >> \"$CALLS\"; [ \"$MODE\" = dev ] && rm -f \"$STATE\"; echo '{}' ;;\n  */remove*) echo store >> \"$CALLS\"; [ \"$MODE\" = store ] && rm -f \"$STATE\"; echo '{}' ;;\nesac\n");
	std::string chmod = "chmod +x " + d + "/luna-send";
	if (system(chmod.c_str()) != 0) return;
	auto run = [&] (const char *mode, bool installed) {
		std::string cmd = uninstall_command("org.example.app");
		for (size_t p; (p = cmd.find(UNINSTALL_LOG)) != std::string::npos;) cmd.replace(p, strlen(UNINSTALL_LOG), d + "/log");
		for (size_t p; (p = cmd.find("sleep 2")) != std::string::npos;) cmd.replace(p, 7, "sleep 0");
		std::string setup = "rm -f " + d + "/calls " + d + "/installed" + (installed ? "; echo 1 > " + d + "/installed" : "");
		if (system(setup.c_str()) != 0) return std::string();
		std::string quoted = "'";
		for (char c : cmd) quoted += c == '\'' ? std::string("'\\''") : std::string(1, c);
		quoted += "'";
		std::string full = "PATH=" + d + ":$PATH STATE=" + d + "/installed MODE=" + mode + " CALLS=" + d + "/calls sh -c " + quoted + " > " + d + "/out";
		if (system(full.c_str()) != 0) return std::string();
		std::string out, calls;
		read_file(d + "/out", out);
		read_file(d + "/calls", calls);
		for (auto &c : calls) if (c == '\n') c = ',';
		return parse_uninstall(out) + "|" + calls;
	};
	eq(run("dev", true), std::string("removed|dev,"), "a developer-mode app: removed by the first call");
	eq(run("store", true), std::string("removed|dev,store,"), "a store app: the second call removes it");
	eq(run("none", true), std::string("still-installed|dev,store,"), "when the TV refuses both, it says still installed");
	eq(run("dev", false), std::string("removed|"), "an app that is already gone: nothing is called");
	std::string rm = "rm -rf " + d;
	if (system(rm.c_str()) != 0) return;
}

static void homemode () {
	set_lang("en");
	auto st = parse_mode_status("mode=default\ncan_default=yes\nrelease=10.3.2\n");
	eq(st ? st->mode + "," + show(st->can_default) + "," + st->release : "", std::string("default,true,10.3.2"), "the status of the TV is read");
	st = parse_mode_status("mode=none\r\ncan_default=no\r\nrelease=\r\n");
	eq(st ? st->mode + "," + show(st->can_default) + "," + st->release : "", std::string("none,false,"), "an unknown release stays empty (and a terminal's CR is no harm)");
	eq(parse_mode_status("sh: homemode.sh: not found").has_value(), false, "no script: unknown, not \"off\"");
	auto r = parse_result("ok default\n");
	eq(r ? show(r->ok) + "," + r->mode + "," + r->why : "", std::string("true,default,"), "a switch that worked");
	r = parse_result("failed default: the TV did not keep the launcher as Home (back to none)");
	eq(r ? show(r->ok) + "," + r->mode + "," + r->why : "", std::string("false,default,the TV did not keep the launcher as Home (back to none)"), "a switch that failed keeps its reason");
	eq(parse_result("").has_value(), false, "no result yet: still running");
	eq(release_major("10.3.2"), 10, "webOS 25 is release 10");
	HomeModeInfo no_choice{"none", false, "10.3.2"};
	eq(mode_note("default", no_choice).blocked, true, "the official way is blocked where the TV cannot choose a Home app");
	eq(mode_note("default", HomeModeInfo{"none", true, "10.3.2"}).blocked, false, "and open where it can");
	eq(!mode_note("mount", std::nullopt).blocked && !mode_note("helper", std::nullopt).blocked, true, "the Swap and the helper are in the native version");
	eq(run_command("x.y", "helper"), std::string("sh '/media/developer/apps/usr/palm/applications/x.y/tools/helper.sh' run"), "the helper is switched on by helper.sh");
	eq(hook_dir("x.y") + " " + alive_file("tld.my.simplelauncher"), std::string("/media/developer/launcher-native-homehook /tmp/launcher-alive"), "the helper's folders follow the id");
	bool all = true;
	for (auto &m : HOME_MODES) all = all && !mode_text(m).pros.empty() && !mode_text(m).cons.empty();
	eq(all, true, "every way has pros and cons");
	eq(result_file("tld.my.simplelauncher.native"), std::string("/tmp/launcher-native-homemode.result"), "the native launcher keeps its own state (scripts/gen-tools.sh)");
	// the scripts in the native package: its own id and its own state, nothing left that names the web version's
	char dir[] = "/tmp/gen-tools-XXXXXX";
	if (mkdtemp(dir)) {
		std::string d = dir, cmd = "sh scripts/gen-tools.sh tld.my.simplelauncher.native " + d + "/t > /dev/null && "
			"grep -h 'simplelauncher\\|launcher-' " + d + "/t/*.sh " + d + "/t/homehook/* | grep -v 'simplelauncher\\.native\\|launcher-native-' | grep -v '^ *#' | grep -v '\\[l\\]auncher-native-' | grep -v '^PROGRAM=\\|launcher-missing\\|48-launcher-homehook.off' > " + d + "/left; "
			"for f in " + d + "/t/*.sh " + d + "/t/homehook/*.sh; do sh -n $f || echo bad >> " + d + "/left; done; rm -rf " + d + "/t";
		eq(system(cmd.c_str()), 0, "gen-tools.sh runs");
		std::string left;
		read_file(d + "/left", left);
		eq(left, std::string(""), "no line of the native scripts names the web version's id or state (and all parse)");
		std::string rm = "rm -rf " + d;
		if (system(rm.c_str()) != 0) {}
	}
	eq(status_command("tld.my.simplelauncher.native"), std::string("sh '/media/developer/apps/usr/palm/applications/tld.my.simplelauncher.native/tools/homemode.sh' status"), "the status comes from the installed app's own tools");
}

static void quick () {
	set_lang("ru");
	const QuickSetting *pic = find_quick("picture");
	eq(pic && pic->category == "picture" && pic->key == "pictureMode", true, "the picture mode is the TV's picture/pictureMode");
	auto next = next_values(*pic, "cinema");
	eq(next.size() == pic->values.size() - 1 && next[0] == "sports" && next.back() == "eco", true, "after Cinema: Sports first, round to Eco, Cinema itself left out");
	eq((int)next_values(*pic, "hdrStandard").size(), (int)pic->values.size(), "an unknown current mode: every one is tried");
	eq(quick_value_name("picture", "cinema"), std::string("Кино"), "a mode by its name");
	eq(quick_value_name("picture", "hdrCinemaBright"), std::string("hdrCinemaBright"), "an unknown one as the TV calls it");
	eq(setting_from_reply("{\"returnValue\":true,\"settings\":{\"pictureMode\":\"eco\"}}", "pictureMode"), std::string("eco"), "the current value from the reply");
	eq(setting_from_reply("{\"returnValue\":false}", "pictureMode"), std::string(""), "none when the TV says no");
	eq(set_command("picture", "pictureMode", "cinema"), std::string("luna-send -n 1 luna://com.webos.settingsservice/setSystemSettings '{\"category\":\"picture\",\"settings\":{\"pictureMode\":\"cinema\"}}'"), "the command");
	std::string desc = "\r\n{\"returnValue\":true,\"results\":[{\"key\":\"soundMode\",\"values\":{\"arrayExt\":[{\"visible\":true,\"active\":true,\"value\":\"standard\"},"
		"{\"visible\":false,\"active\":false,\"value\":\"aiSound\"},{\"visible\":true,\"value\":\"movie\",\"active\":true}]}}]}";
	auto vs = values_from_desc(desc, "soundMode");
	eq(vs.size() == 2 && vs[0] == "standard" && vs[1] == "movie", true, "the TV's own values: those its menu shows, in its order");
	eq(values_from_desc(desc, "pictureMode").empty() && values_from_desc("{\"returnValue\":false}", "soundMode").empty(), true, "none for another key or a refusal");
	eq(quick_value_name("sound", "movie"), std::string("Кино"), "the sound mode Movie");
	eq(reply_ok("\r\n{\"returnValue\":true}\n") && !reply_ok("{\"returnValue\":false,\"errorText\":\"key is not allowed\"}") && !reply_ok(""), true, "the TV's answer read");
	set_lang("en");
}

static void updates () {
	eq(sha256_hex(""), std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"), "sha256 of nothing");
	eq(sha256_hex("abc"), std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"), "sha256 of abc");
	eq(sha256_hex(std::string(1000, 'a')), std::string("41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3"), "sha256 over several blocks");
	eq(version_newer("2.0.1", "2.0.0") && version_newer("2.0.10", "2.0.9") && version_newer("2.1", "2.0.9") && version_newer("10.0.0", "9.9.9"), true, "later versions");
	eq(version_newer("2.0.0", "2.0.0") || version_newer("2.0", "2.0.0") || version_newer("1.10.1", "2.0.0") || version_newer("2.0.0-rc", "2.0.0"), false, "not later");
	// the web version's own repository file
	std::string feed;
	read_file("repo/apps.json", feed);
	auto u = parse_update_feed(feed, "tld.my.simplelauncher");
	eq(u && !u->version.empty() && u->url.rfind("https://github.com/", 0) == 0 && u->sha256.size() == 64 && u->size > 0, true, "the repository's package read");
	eq(parse_update_feed(feed, "other.app").has_value() || parse_update_feed("{}", "tld.my.simplelauncher").has_value() || parse_update_feed("nonsense", "x").has_value(), false, "no package, no update");
	eq(parse_update_feed(R"({"packages":[{"manifest":{"id":"a","version":"2.0.1","ipkUrl":"http://x/a.ipk","ipkHash":{"sha256":"0000000000000000000000000000000000000000000000000000000000000000"}}}]})", "a").has_value(), false, "an address that is not https is not offered");
	eq(parse_update_feed(R"({"packages":[{"manifest":{"id":"a","version":"2.0.1","ipkUrl":"file:///tmp/a.ipk","ipkHash":{"sha256":"0000000000000000000000000000000000000000000000000000000000000000"}}}]})", "a", true).has_value(), true, "a test repository may name a file on the TV");
	eq(update_file("/d", "2.0.1; rm"), std::string("/d/update-2.0.1rm.ipk"), "the file's name from the version, made safe");
	// a download (from a file here, through the same code)
	char dir[] = "/tmp/update-test-XXXXXX";
	if (mkdtemp(dir)) {
		std::string d = dir, src = d + "/src.bin", data;
		for (int i = 0; i < 300000; i++) data += (char)(i * 7);
		write_file(src, data);
		long last = -1, total = 0;
		HttpResult r = http_download("file://" + src, d + "/got.ipk", [&] (long done, long t) { last = done; total = t; return true; });
		eq(r.ok && sha256_file(d + "/got.ipk") == sha256_hex(data) && !file_exists(d + "/got.ipk.part"), true, "downloaded whole, the part file moved into place");
		eq(last == (long)data.size() && total == (long)data.size(), true, "progress reaches the end");
		HttpResult bad = http_download("file://" + d + "/none", d + "/x.ipk", [] (long, long) { return true; });
		HttpResult stop = http_download("file://" + src, d + "/s.ipk", [] (long, long) { return false; });
		eq(!stop.ok && stop.error == "stopped" && !file_exists(d + "/s.ipk") && !file_exists(d + "/s.ipk.part"), true, "a stopped download leaves nothing");
		eq(!bad.ok && !file_exists(d + "/x.ipk") && !file_exists(d + "/x.ipk.part"), true, "a failed download leaves nothing");
		std::string rm = "rm -rf " + d;
		if (system(rm.c_str()) != 0) {}
	}
}

// 2.1.0: the weather's hosts, the city list's order, the storage clean-up, the devices on the inputs, dimming at night
static void v210 () {
	eq(show(host_order(0)), std::string("[0,1,2]"), "weather hosts: the official one first");
	eq(show(host_order(2)), std::string("[2,0,1]"), "weather hosts: the one that answered last first");
	eq(show(host_order(9)), std::string("[0,1,2]"), "weather hosts: an unknown number is ignored");
	eq(forecast_url(1.5, 2.5, 1).rfind("https://historical-forecast-api.open-meteo.com/v1/forecast?latitude=1.500&longitude=2.500&", 0) == 0, true, "weather hosts: the same request on another host");
	eq(forecast_url(1.5, 2.5).rfind("https://api.open-meteo.com/v1/forecast?", 0) == 0, true, "weather hosts: the official one by default");
	set_weather_host(7);
	eq(weather_host(), 0, "weather hosts: a number out of the list is the first");

	set_lang("en");
	eq(region_of("ru-RU") + "," + region_of("en_gb") + "," + region_of("zh-Hans-CN") + "," + region_of("de") + "," + region_of(""), std::string("RU,GB,CN,,"), "the region of a language tag");
	auto names = [] (const std::vector<City> &list, size_t n) { std::string o; for (size_t i = 0; i < n && i < list.size(); i++) o += list[i].name + ","; return o; };
	eq(names(preset_cities("AZ", "ru"), 5), std::string("Baku,Ganja,Sumgait,Moscow,Saint Petersburg,"), "cities: the TV's country, then the language's");
	eq(names(preset_cities("IT", "ru"), 3), std::string("Rome,Milan,Moscow,"), "cities: a Russian TV in Italy starts with Italy");
	eq(names(preset_cities("AT", "de"), 4), std::string("Vienna,Berlin,Munich,Hamburg,"), "cities: the TV's country before the language's other countries");
	eq(names(preset_cities("US", "en"), 6), std::string("New York,Los Angeles,Chicago,London,Manchester,Toronto,"), "cities: English, the countries in their order");
	eq(names(preset_cities("", "ja"), 2), std::string("Tokyo,Osaka,"), "cities: without a region, the language's own");
	auto rest = preset_cities("", "ja");
	eq(rest[2].detail + "," + rest[3].detail, std::string("Armenia,Austria"), "cities: the others by the name of their country");
	eq((int)preset_cities("", "").size(), (int)preset_cities("RU", "ru").size(), "cities: the order never drops one");

	std::string dir = "/tmp/launcher-storage-test";
	system(("rm -rf " + dir + " && mkdir -p " + dir + "/icons").c_str());
	for (const char *f : {"config.json", "update-2.1.0.ipk", "update-2.1.0.ipk.part", "news.json", "weather.json", "icons/a.png", "config.json.new", "weather-host"}) write_file(dir + "/" + f, "12345");
	system(("rm -f " + dir + "/*.new.new " + dir + "/icons/*.new").c_str());
	auto short_names = [&] (std::vector<std::string> v) { for (auto &s : v) s = s.substr(dir.size() + 1); return show(v); };
	eq(short_names(rebuildable_files(dir, false)), std::string("[update-2.1.0.ipk,update-2.1.0.ipk.part,config.json.new,icons/a.png,news.json,weather.json]"), "storage: what the launcher can make again");
	eq(short_names(rebuildable_files(dir, true)), std::string("[config.json.new,icons/a.png,news.json,weather.json]"), "storage: an update being installed is kept");
	eq((int)clear_rebuildable(dir, false), 30, "storage: the bytes freed");
	eq(file_exists(dir + "/config.json") && file_exists(dir + "/weather-host") && !file_exists(dir + "/news.json"), true, "storage: the settings stay");
	eq(free_bytes(dir) > 0, true, "storage: free space is read");
	system(("rm -rf " + dir).c_str());

	auto dn = device_names(R"({"returnValue":true,"cec":{"cecDeviceList":[{"osdName":" Fire TV Stick ","portId":1},{"osdName":"PlayStation 5","physicalAddr":8192},{"osdName":"Second","portId":1},{"osdName":"","portId":3},{"portId":4}]}})");
	eq((int)dn.size(), 2, "input devices: the named ones");
	eq(dn[1] + "|" + dn[2], std::string("Fire TV Stick|PlayStation 5"), "input devices: by port id or physical address, the first on a port");
	eq((int)device_names("** Message: something\n{\"returnValue\":false}").size() + (int)device_names("").size() + (int)device_names("{\"cec\":5}").size(), 0, "input devices: another reply gives none");

	// 21 June 2026 12:00 UTC and 21 December 2026 00:00 UTC over Berlin; the equinox at noon on the equator
	double june = 1782043200, december = 1797811200, equinox = 1773964800 + 12 * 3600;
	eq(std::fabs(sun_elevation(june, 52.52, 13.405) - 58.9) < 1.5, true, "sun: high at a summer noon");
	eq(sun_elevation(december, 52.52, 13.405) < -55, true, "sun: far under the horizon on a winter night");
	eq(sun_elevation(equinox, 0, 0) > 87, true, "sun: overhead at the equator at the equinox");
	eq(night_level(10) == 0 && night_level(-3) == 0.5 && night_level(-20) == 1, true, "night: from the horizon down to 6 degrees under it");
	std::tm t{};
	t.tm_hour = 18; double six = night_level_by_clock(t);
	t.tm_hour = 19; double seven = night_level_by_clock(t);
	t.tm_hour = 6; double morning = night_level_by_clock(t);
	t.tm_hour = 7; double day = night_level_by_clock(t);
	eq(six == 0 && seven == 1 && morning == 1 && day == 0, true, "night by the clock: 19:00 to 7:00");

	UserConfig c = parse_config(R"({"wallpaper":{"mode":"image","path":"/a.jpg","dim":"night","dimStrength":57}})");
	eq(c.wallpaper.dim + "|" + std::to_string(c.wallpaper.dimStrength), std::string("night|60"), "dimming is read, its strength on a step");
	eq(parse_config(R"({"wallpaper":{"dim":"dusk"}})").wallpaper.dim + "|" + std::to_string(parse_config("{}").wallpaper.dimStrength), std::string("off|40"), "dimming: off and 40% by default");
	eq(parse_config(config_json(c)).wallpaper.dim, std::string("night"), "dimming is saved");
}

void collection () {
	std::string h64(64, 'a');
	auto photo = [&] (const std::string &id, const std::string &group, const std::string &url, const std::string &sum) {
		return "{\"id\":\"" + id + "\",\"group\":\"" + group + "\",\"title\":\"T " + id + "\",\"author\":\"A\",\"licence\":\"CC BY 4.0\",\"url\":\"" + url +
			"\",\"sha256\":\"" + sum + "\",\"thumb\":\"https://x/t.jpg\",\"thumbSha256\":\"" + h64 + "\",\"size\":700000}";
	};
	std::string list = "{\"version\":1,\"photos\":[" + photo("m1", "mountains", "https://x/m1.jpg", h64) + "," + photo("n1", "nature", "http://x/n1.jpg", h64) + "," +
		photo("s1", "space", "https://x/s1.jpg", "abc") + "," + photo("../x", "space", "https://x/x.jpg", h64) + "," + photo("c1", "cities", "https://x/c1.jpg", h64) + "," +
		photo("m1", "mountains", "https://x/again.jpg", h64) + "," + photo("s2", "space", "file:///tmp/s2.jpg", h64) + "," +
		photo("s3", "space", "http://127.0.0.1:8765/s3.jpg", h64) + "," + photo("s4", "space", "http://example.com/s4.jpg", h64) + "]}";
	auto ids = [] (const std::vector<CollectionPhoto> &v) { std::string o; for (auto &p : v) o += p.id + ","; return o; };
	eq(ids(parse_collection(list)), std::string("m1,"), "collection: only photos that can be checked, once each, in a known group");
	eq(ids(parse_collection(list, true)), std::string("m1,s2,s3,"), "collection: a test list may name files or a server on the TV, nothing else without https");
	auto c = parse_collection(list);
	eq(c[0].title + "|" + c[0].licence + "|" + std::to_string(c[0].size), std::string("T m1|CC BY 4.0|700000"), "collection: the photo's details");
	eq((int)parse_collection("").size() + (int)parse_collection("{\"photos\":5}").size(), 0, "collection: a broken list gives none");

	std::string dir = "/tmp/launcher-collection-test";
	system(("rm -rf " + dir + " && mkdir -p " + dir + "/collection " + dir + "/collection-thumbs").c_str());
	for (const char *f : {"collection/m1.jpg", "collection/n1.jpg", "collection/s1.jpg.part", "collection-thumbs/m1.jpg", "config.json"}) write_file(dir + "/" + f, "12345");
	auto unused = unused_collection_files(dir, {collection_file(dir, "m1")});
	std::sort(unused.begin(), unused.end());
	for (auto &u : unused) u = u.substr(dir.size() + 1);
	eq(show(unused), std::string("[collection/n1.jpg,collection/s1.jpg.part]"), "collection: what is not the wallpaper goes");
	auto rb = rebuildable_files(dir, false);
	eq(std::find(rb.begin(), rb.end(), dir + "/collection-thumbs/m1.jpg") != rb.end() && std::find(rb.begin(), rb.end(), dir + "/collection/m1.jpg") == rb.end(), true,
		"storage: the previews of the collection can be made again, its photos in use are kept");
	system(("rm -rf " + dir).c_str());

	// the store: wanted pictures download in the background (here, from files), the unwanted are deleted when asked
	std::string data = "/tmp/launcher-store-test", src = "/tmp/launcher-store-src";
	system(("rm -rf " + data + " " + src + " && mkdir -p " + data + " " + src).c_str());
	write_file(src + "/a.jpg", "picture a");
	write_file(src + "/b.jpg", "picture b");
	auto entry = [&] (const std::string &id) {
		return "{\"id\":\"" + id + "\",\"group\":\"space\",\"url\":\"file://" + src + "/" + id + ".jpg\",\"sha256\":\"" + sha256_file(src + "/" + id + ".jpg") +
			"\",\"thumb\":\"file://" + src + "/" + id + ".jpg\",\"thumbSha256\":\"" + h64 + "\",\"size\":9}";
	};
	write_file(data + "/list.json", "{\"photos\":[" + entry("a") + "," + entry("b") + "]}");
	write_file(data + "/collection-feed", "file://" + data + "/list.json\n");
	make_dirs(collection_dir(data));
	write_file(collection_file(data, "old"), "an old one");
	CollectionStore store;
	store.set_data_dir(data);
	std::vector<std::string> arrived;
	store.on_arrived = [&] (const std::string &path) { arrived.push_back(base_name(path)); };
	store.set_wanted({collection_file(data, "a")});
	store.set_ticked({collection_file(data, "b")});
	eq(store.missing(), 2, "store: two wanted, none on the TV");
	for (int i = 0; i < 5; i++) run_posted();
	eq(show(arrived), std::string("[b.jpg,a.jpg]"), "store: the list is read, then the gallery's ticks come first, one at a time");
	eq(store.missing() == 0 && file_exists(collection_file(data, "a")) && store.state("a") == CollectionStore::State::none, true, "store: all on the TV");
	store.set_ticked({});
	store.prune();
	run_posted();
	eq(!file_exists(collection_file(data, "b")) && !file_exists(collection_file(data, "old")) && file_exists(collection_file(data, "a")), true,
		"store: what is not wanted is deleted, the wallpaper's picture stays");

	// a photo made again (another size in the list) is downloaded again at the next start; the others stay as they are
	write_file(src + "/a.jpg", "picture a, sharper");
	std::string again = entry("a");
	again.replace(again.find("\"size\":9"), 8, "\"size\":18");
	write_file(data + "/list.json", "{\"photos\":[" + again + "," + entry("b") + "]}");
	CollectionStore next;
	next.set_data_dir(data);
	arrived.clear();
	next.on_arrived = [&] (const std::string &path) { arrived.push_back(base_name(path)); };
	next.set_wanted({collection_file(data, "a")});
	eq(next.missing(), 0, "store: the wallpaper's picture is on the TV");
	for (int i = 0; i < 5; i++) run_posted();
	std::string got;
	read_file(collection_file(data, "a"), got);
	eq(show(arrived) + " " + got, std::string("[a.jpg] picture a, sharper"), "store: the list is read even so, and a photo that changed in it comes again");
	system(("rm -rf " + data + " " + src).c_str());
}

int main () {
	v210();
	collection();
	updates();
	quick();
	homemode();
	gallery();
	uninstall();
	settings();
	quote();
	feeds();
	calendar();
	weather();
	plates();
	rows();
	apporder();
	tileorder();
	boardlayout();
	config();
	dates();
	printf("%d passed, %d failed\n", passes, fails);
	return fails ? 1 : 0;
}
