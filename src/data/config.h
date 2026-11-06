// The user's choices, as the web version keeps them (lib/storage.ts): the same fields, defaults and checks, the same JSON.
// The native launcher reads the web version's copy (/media/developer/launcher-config.json) until it has its own.
#pragma once

#include <optional>
#include <string>
#include <vector>

struct Range { int min, max, step, def; };
namespace ranges {
constexpr Range iconScale{50, 140, 10, 100};
constexpr Range sideMargin{0, 300, 10, 80};
constexpr Range bottomMargin{0, 200, 10, 50};
constexpr Range clockSize{80, 220, 10, 160};
constexpr Range clockOpacity{10, 100, 10, 100};
constexpr Range dateSize{60, 160, 5, 100};
constexpr Range cardSize{60, 160, 5, 100};
constexpr Range frost{30, 100, 10, 100};
}  // namespace ranges

struct City {
	std::string name;
	double lat = 0, lon = 0;
	std::string detail;      // empty when none
};

struct ClockConfig {
	int size = ranges::clockSize.def;
	int weight = 200;                // 200 300 400 500 600
	std::string font = "inter";      // inter tv poppins oswald mono serif
	std::string color = "white";     // lib/clockcolor.ts
	int opacity = ranges::clockOpacity.def;
	bool showDate = true;
	int dateSize = ranges::dateSize.def;
	bool showPeriod = true;
};

struct WeatherConfig { bool on = false; std::optional<City> city; std::string unit = "c"; bool showCity = true; };
struct CalendarConfig { bool on = false; std::vector<std::string> sources; };
struct NewsConfig { bool on = false; bool google = true; std::string country; std::vector<std::string> feeds; };
struct QuoteConfig { bool on = false; };

struct WallpaperConfig {
	std::string mode = "gradient";   // gradient image folder pictures
	std::string path;
	std::vector<std::string> paths;
	int intervalSec = 60;
	bool shuffle = true;
};

struct UserConfig {
	std::string language = "auto";
	std::optional<std::vector<std::string>> appIds;      // null: the TV's order
	std::optional<std::vector<std::string>> hiddenIds = std::vector<std::string>();   // null: old settings, see apporder
	std::vector<std::string> sourceIds;
	std::optional<std::vector<std::string>> tileOrder;
	bool useHomebrew = true;
	bool closeHome = true;
	bool replaceHome = false;
	bool checkUpdates = true;    // look for new versions on GitHub now and then (Settings → Home screen → About)
	bool hookBoot = true;
	std::string theme = "dusk";      // black grey dusk
	int iconScale = ranges::iconScale.def;
	bool showLabels = true;
	bool labelsSelectedOnly = false;
	int rows = 1;
	int sideMargin = ranges::sideMargin.def;
	int bottomMargin = ranges::bottomMargin.def;
	int cardSize = ranges::cardSize.def;
	std::string plates = "auto";     // auto light dark
	bool glass = true;
	int frost = ranges::frost.def;
	bool shadows = true;
	std::string pixelShift = "auto"; // auto on off
	std::string clockFormat = "24h"; // tv 12h 24h
	ClockConfig clock;
	WeatherConfig weather;
	CalendarConfig calendar;
	NewsConfig news;
	QuoteConfig quote;
	WallpaperConfig wallpaper;
};

/** Settings from their saved text, checked value by value (the defaults for anything missing or wrong), as parseConfig. */
UserConfig parse_config (const std::string &raw);
std::string config_json (const UserConfig &c);
std::vector<int> range_values (const Range &r);

extern const std::vector<std::string> LANG_SETTINGS;
extern const std::vector<std::string> NEWS_COUNTRIES;
bool valid_calendar_source (const std::string &s);
std::string normalize_feed_url (const std::string &raw);
bool valid_feed_url (const std::string &url);
