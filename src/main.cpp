// Simple LG launcher, native: the window, the main loop and the pieces put together (app.tsx of the web version).
// Draws only when something changed or moves (the compositor shows at most 48 frames a second here anyway), with vsync on.

#include "appid.h"          // build/inc, from the Makefile
#include "core/log.h"
#include "core/tasks.h"
#include "gfx/image.h"
#include "data/apps.h"
#include "data/config.h"
#include "data/order.h"
#include "data/prefs.h"
#include "data/homemode.h"
#include "data/inputs.h"
#include "data/quick.h"
#include "data/uninstall.h"
#include "data/storage.h"
#include "data/collection.h"
#include "data/collection_store.h"
#include "data/sun.h"
#include "data/update.h"
#include "data/weather.h"
#include "data/calendar.h"
#include "data/cities.h"
#include "data/feeds.h"
#include "data/quote.h"
#include "gfx/gfx.h"
#include "gfx/text.h"
#include "i18n/dates.h"
#include "i18n/i18n.h"
#include "sys/fs.h"
#include "core/sha256.h"
#include "sys/http.h"
#include "sys/luna.h"
#include "ui/anim.h"
#include "ui/home.h"
#include "ui/app_menu.h"
#include "ui/gallery.h"
#include "ui/home_method.h"
#include "ui/quick_panel.h"
#include "ui/icons.h"
#include "ui/pickers.h"
#include "ui/settings.h"
#include "ui/toast.h"
#include "ui/wallpaper.h"

#include <SDL.h>
#include <nlohmann/json.hpp>

#include <signal.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utime.h>

#include <chrono>
#include <cmath>
#include <map>
#include <mutex>
#include <thread>
#include <cstring>
#include <ctime>

using json = nlohmann::json;

namespace {

const char *APP_ID = APP_ID_TEXT;                 // make ID=…: the development id, or the web version's (build/inc/appid.h)
// The id it runs under: its own, or LG Home's while the Swap has put it in LG Home's place (the app manager starts it with
// {"appId":"com.webos.app.home"}). It registers under this one; the tools and the data stay its own (APP_ID).
std::string RUN_ID = APP_ID;
bool AS_HOME = false;                             // RUN_ID is LG Home's: the Swap is on
const std::string DATA_DIR = DATA_DIR_TEXT;
const std::string WEB_CONFIG = "/media/developer/launcher-config.json";

struct App {
	SDL_Window *win = nullptr;
	UserConfig cfg;
	std::string lang = "en", tv_lang = "en", tv_tag;
	std::string tv_country;                    // the country set on the TV ("IT"), for the order of the cities
	bool h24 = true, tv_h24 = true;
	std::vector<AppEntry> apps;
	std::vector<InputSource> inputs;
	std::map<int, std::string> device_names;   // HDMI port -> the name of the device on it (read_device_names)
	std::string devices_for;                   // the plugged-in inputs those names were read for
	bool devices_busy = false;
	Home home;
	Wallpaper wallpaper;
	// what is on the screen: the home screen, Settings, or a sheet opened from Settings over it
	enum class Screen { home, settings, picker, gallery, home_method } screen = Screen::home;
	HomeMethod home_method;
	std::optional<HomeModeInfo> home_mode;     // how the launcher opens on Home (tools/homemode.sh), read when Settings opens
	std::string mode_busy;                     // the way being switched to
	double name_at = -1;                       // the Swap and the Helper: when to name it the last input (it came to the front)
	bool default_home = false;                 // the official way is on (defaulthome.sh refresh said so, or a switch to it)
	bool helper_on = false;                    // the helper is on (helper.sh refresh said so, or a switch to it)
	UpdateState update;                        // a new version of the launcher (data/update.h)
	Root root = Root::unknown;                 // can the TV run commands as root (told when Settings opens: read_mode)
	std::string root_detail;                   // what it said when it could not
	double update_next = -1;                   // when to look for one next (-1: not while the setting is off)
	bool update_busy = false;
	std::string front_app;                     // the app in front (getForegroundAppInfo), "" between two
	double lg_home_by_us = -1;                 // when the launcher itself last opened LG Home (its tile)
	Settings settings;
	Picker picker;
	Gallery gallery;
	CollectionStore collection;      // the wallpaper collection: its list and its downloads, which go on in the background
	AppMenu menu;                    // over the home screen: OK held on a tile
	QuickPanel quick;                // over the home screen: Down from the row
	std::string menu_key;            // the tile it is for
	Toast toast;
	std::string version;
	bool manual_refresh = false;     // the "Refresh list" button: say what was found
	bool dirty = true, running = true, front = true;
	int last_minute = -1;
	// the weather: what is shown, when to fetch next, and how many tries have failed in a row
	std::optional<WeatherData> weather;
	double weather_next = 0;
	int weather_fails = 0;
	bool weather_busy = false;
	int weather_host_saved = 0;      // the Open-Meteo host in the file weather-host (the one that answered last)
	std::optional<CalendarData> calendar;
	double cal_next = 0;
	int cal_fails = 0;
	bool cal_busy = false;
	bool cal_partial = false;        // some calendars could not be read last time: not fresh, whatever its age
	std::optional<NewsData> news;
	double news_next = 0;
	int news_fails = 0;
	bool news_busy = false;
	std::optional<Quote> quote;
	double quote_next = 0;
	int quote_fails = 0;
	bool quote_busy = false;
};

// a widget that could not fetch tries again after these waits (the last one repeats); short at first, as right after the TV
// starts its network takes a while to come up (TIMING.retryMs)
const double RETRY_MS[] = {15000, 30000, 60000, 120000, 600000, 1800000};

double wall_ms () {
	return (double)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

App app;

void apply_lang () {
	std::string was = app.lang;
	app.lang = app.cfg.language == "auto" ? app.tv_lang : app.cfg.language;
	set_lang(app.lang);
	// the quote is in the launcher's language: another one (the TV's language read only now, after a start in English) is
	// fetched at once, and the one in the old language is not shown meanwhile
	if (app.lang != was) {
		app.quote_next = 0;
		app.quote_fails = 0;
		app.home.set_quote(app.quote && app.quote->lang == app.lang ? app.quote : std::nullopt, app.cfg.quote.on);
	}
	app.h24 = app.cfg.clockFormat == "tv" ? app.tv_h24 : app.cfg.clockFormat == "24h";
	app.home.set_lang(app.lang, app.h24);
}

TileModel app_tile (const AppEntry &a) { return {a.id, a.title, a.iconPath, "", "", a.color, false, ""}; }
// An input's picture from the kind of device the TV names for it (the name of LG's own icon file): the native version's own line
// icons on one colour for all the inputs
std::string input_glyph (const std::string &lg_icon) {
	std::string n = lg_icon.substr(0, lg_icon.find('.'));
	if (n == "tv" || n == "antenna" || n == "satellite") return "in-antenna";
	if (n == "gameconsole") return "in-gamepad";
	if (n == "streamingbox" || n == "settopbox" || n == "bluray" || n == "dvd" || n == "vcr" || n == "portaltv") return "in-settop";
	if (n == "hometheater" || n == "sound") return "in-soundbar";
	if (n == "pc") return "in-pc";
	return "in-hdmi";
}

const char *INPUT_COLOR = "#1B8FA6";
TileModel input_tile (const InputSource &in) {
	TileModel m{in.appId, in.label, "", "", input_glyph(in.icon), INPUT_COLOR, false, ""};
	m.dimmed = !in.connected;
	// the device's own name, unless the TV's label for the input already says it
	if (in.connected && !in.device.empty() && in.device != in.label) m.note = in.device;
	return m;
}

/** The hide-list in use: settings from an older version listed the apps to show instead. */
std::vector<std::string> hidden_ids () {
	std::function<std::string(const AppEntry &)> app_id = [] (const AppEntry &a) { return a.id; };
	return app.cfg.hiddenIds ? *app.cfg.hiddenIds : hidden_after_upgrade(app.apps, app.cfg.appIds, app_id);
}

std::vector<AppEntry> shown_apps () {
	std::function<std::string(const AppEntry &)> app_id = [] (const AppEntry &a) { return a.id; };
	return visible_apps(app.apps, app.cfg.appIds, std::optional<std::vector<std::string>>(hidden_ids()), app_id);
}

/** The inputs chosen in Settings, in their order. */
std::vector<InputSource> shown_inputs () {
	std::vector<InputSource> out;
	for (auto &id : app.cfg.sourceIds)
		for (auto &in : app.inputs)
			if (in.appId == id) out.push_back(in);
	return out;
}

/** Apps and inputs share one order (Move to start / left / right / end). */
std::vector<TileModel> ordered_tiles () {
	std::vector<TileModel> tiles;
	for (auto &a : shown_apps()) tiles.push_back(app_tile(a));
	for (auto &in : shown_inputs()) tiles.push_back(input_tile(in));
	std::function<std::string(const TileModel &)> key = [] (const TileModel &t) { return t.key; };
	return order_tiles(tiles, key, app.cfg.tileOrder);
}

void refresh_settings ();
void rebuild_items ();

void rebuild_items () {
	std::vector<TileModel> tiles = ordered_tiles();
	// the fixed tiles at the end: each on its own colour, as the sections of Settings
	// with the Swap on, LG Home's id opens the launcher itself: no tile for it then
	if (!AS_HOME) tiles.push_back({"lg-home", "LG Home", "", "", "sys-home", "#A50034", false, ""});
	// a new version of the launcher: its tile opens Settings on the update row (and starts the download)
	// (not on a TV that cannot install it: the row says to update in the Homebrew Channel)
	if (app.cfg.checkUpdates && app.update.step != UpdateState::Step::none && app.root != Root::no) tiles.push_back({"update", tr("Update the launcher", "Обновить лаунчер"), "", "", "sys-update", "#248A3D", false, ""});
	tiles.push_back({"system-settings", tr("TV settings", "Настройки ТВ"), "", "", "sys-gear", "#636366", false, ""});
	tiles.push_back({"settings", tr("Launcher settings", "Настройки лаунчера"), "", "", "sys-launcher", "#5245D0", false, ""});      // the app icon's colour
	app.home.set_items(tiles);
	refresh_settings();
	app.dirty = true;
}

// ---------- the start-up picture ----------
// What the TV shows while the launcher starts is appinfo.json's splashBackground: always the packaged logo card
// (splash-wallpaper.jpg, a copy of splash-default.jpg). Builds before 2.0.0 could put the wallpaper or black there, a choice
// in Settings; an update brings the package's picture back, and their note of what was written is removed.

void forget_splash () { remove(join_path(DATA_DIR, "splash.key").c_str()); remove(join_path(DATA_DIR, "splash.jpg").c_str()); }

// How much the wallpaper is dimmed now (Settings → Wallpaper → Dim the picture): the chosen strength, always or by how far into the
// night it is: by the sun over the weather's city, or by the clock (19:00 to 7:00) when no city is chosen.
float wallpaper_dim () {
	const WallpaperConfig &w = app.cfg.wallpaper;
	if (w.mode == "gradient" || w.dim == "off") return 0;
	double level = 1;
	if (w.dim == "night") {
		if (app.cfg.weather.city) level = night_level(sun_elevation(wall_ms() / 1000.0, app.cfg.weather.city->lat, app.cfg.weather.city->lon));
		else {
			std::time_t tt = std::time(nullptr);
			std::tm lt;
			localtime_r(&tt, &lt);
			level = night_level_by_clock(lt);
		}
	}
	return (float)(std::round(w.dimStrength * level) / 100.0);
}

void apply_dim () {
	if (!app.wallpaper.set_dim(wallpaper_dim())) return;
	app.home.mark_board();
	app.dirty = true;
}

/** The collection's pictures the wallpaper shows are wanted: the ones not on the TV are downloaded in the background (also after
 *  a restart), and, with `prune`, the ones it no longer shows are deleted. A slideshow of the collection's folder keeps them all. */
void sync_collection (bool prune) {
	const WallpaperConfig &w = app.cfg.wallpaper;
	std::vector<std::string> wanted;
	if (w.mode == "image") wanted.push_back(w.path);
	if (w.mode == "pictures") wanted = w.paths;
	app.collection.set_wanted(wanted);
	if (prune && !(w.mode == "folder" && w.path == collection_dir(DATA_DIR))) app.collection.prune();
}

void load_config () {
	std::string raw;
	if (read_file(join_path(DATA_DIR, "config.json"), raw)) logf("config: own");
	else if (read_file(WEB_CONFIG, raw)) logf("config: from the web version");
	else logf("config: defaults");
	app.cfg = parse_config(raw);
	// taken over from the web version: kept as this version's own at once (that file is the web version's, and stays as it was)
	if (!file_exists(join_path(DATA_DIR, "config.json")) && !raw.empty() && !write_file(join_path(DATA_DIR, "config.json"), config_json(app.cfg)))
		logf("config: could not save");
	app.home.set_config(app.cfg);
	app.wallpaper.configure(app.cfg.wallpaper, app.cfg.theme, app.cfg.frost);
	apply_dim();
	apply_lang();
	sync_collection(true);
}

// The names the devices on the HDMI inputs announce (HDMI-CEC). The TV's list of them is not open to apps: it is asked through the
// root shell, and only when what is plugged in has changed. A TV that cannot be asked (no root, another webOS) shows no names.
void read_device_names (const std::string &plugged) {
	if (app.devices_busy || plugged == app.devices_for) return;
	app.devices_busy = true;
	run_background([plugged] {
		bool ok = false;
		std::string why;
		std::string reply = hbc_exec(std::string("luna-send -n 1 ") + CEC_LIST_URI + " '{}'", 6000, &ok, &why);
		post_main([plugged, ok, why, reply] {
			app.devices_busy = false;
			app.devices_for = plugged;
			auto names = ok ? device_names(reply) : std::map<int, std::string>();
			// the reply's start goes to the log once per change: what this TV answers is how the names are known to work on it
			logf("input devices: %s%s -> %d named", ok ? "" : "not read: ", (ok ? reply : why).substr(0, 300).c_str(), (int)names.size());
			if (names == app.device_names) return;
			app.device_names = names;
			for (auto &in : app.inputs) {
				auto it = in.id.rfind("HDMI", 0) == 0 ? names.find(in.port) : names.end();
				in.device = it != names.end() ? it->second : "";
			}
			rebuild_items();
		});
	});
}

// The inputs with what is plugged in: read at the start and each time the launcher comes back to the front (a console switched
// on meanwhile). The row is built again only when something changed.
void refresh_inputs () {
	list_inputs(tr("Live TV", "Прямой эфир"), tr("Launcher", "Лаунчер"), [] (std::vector<InputSource> inputs) {
		std::string plugged, now_text, was_text;
		for (auto &in : inputs) {
			bool hdmi = in.id.rfind("HDMI", 0) == 0;
			auto it = hdmi ? app.device_names.find(in.port) : app.device_names.end();
			if (in.connected && it != app.device_names.end()) in.device = it->second;
			if (hdmi && in.connected) plugged += in.id + " ";
			now_text += in.appId + "|" + in.label + "|" + in.icon + "|" + (in.connected ? "1" : "0") + "|" + in.device + "\n";
		}
		for (auto &in : app.inputs) was_text += in.appId + "|" + in.label + "|" + in.icon + "|" + (in.connected ? "1" : "0") + "|" + in.device + "\n";
		if (now_text != was_text) {
			logf("inputs: %d, plugged in: %s", (int)inputs.size(), plugged.empty() ? "none" : plugged.c_str());
			app.inputs = inputs;
			rebuild_items();
		}
		if (plugged.empty()) app.devices_for.clear();
		else read_device_names(plugged);
	});
}

void refresh_apps () {
	list_apps([] (std::vector<AppEntry> apps) {
		bool manual = app.manual_refresh;
		app.manual_refresh = false;
		if (apps.empty()) {
			if (manual) app.toast.show(tr("Could not read the app list", "Не удалось прочитать список приложений"), true, now_ms());
			return;
		}
		if (manual) {
			long added = 0;
			for (auto &a : apps) {
				bool known = false;
				for (auto &b : app.apps) known = known || a.id == b.id;
				if (!known) added++;
			}
			app.toast.show(added > 0 ? fmt(tr("Found: {0}", "Найдено: {0}"), count(added, "new app", "new apps", "новое приложение", "новых приложения", "новых приложений")) : tr("The app list is up to date", "Список приложений актуален"), false, now_ms());
		}
		app.apps = apps;
		cache_apps(join_path(DATA_DIR, "apps.json"), apps);
		rebuild_items();
	});
	refresh_inputs();
}

void ask_tv () {
	// the TV's language (for "auto") and its 12/24-hour setting (for "TV")
	luna_call("luna://com.webos.settingsservice/getSystemSettings", R"({"keys":["localeInfo"]})", [] (const std::string &r) {
		json j = json::parse(r, nullptr, false);
		std::string ui;
		if (j.is_object() && j["settings"].is_object() && j["settings"]["localeInfo"].is_object() && j["settings"]["localeInfo"]["locales"].is_object())
			ui = j["settings"]["localeInfo"]["locales"].value("UI", std::string());
		if (!ui.empty()) { app.tv_tag = ui; app.tv_lang = lang_from_tag(ui); logf("TV language %s -> %s", ui.c_str(), app.tv_lang.c_str()); }
		apply_lang();
		rebuild_items();
	});
	// the country the TV is set to (it can differ from its language's: a Russian TV in Italy)
	luna_call("luna://com.webos.settingsservice/getSystemSettings", R"({"category":"option","keys":["smartServiceCountryCode2"]})", [] (const std::string &r) {
		json j = json::parse(r, nullptr, false);
		std::string c = j.is_object() && j["settings"].is_object() ? j["settings"].value("smartServiceCountryCode2", std::string()) : "";
		if (c.size() == 2 && isalpha((unsigned char)c[0]) && isalpha((unsigned char)c[1])) {
			app.tv_country = {(char)toupper((unsigned char)c[0]), (char)toupper((unsigned char)c[1])};
			logf("TV country %s", app.tv_country.c_str());
		}
	});
	// an OLED panel? its model name says so (OLED65C1PUB…), lib/panel.ts
	luna_call("luna://com.webos.service.tv.systemproperty/getSystemInfo", R"({"keys":["modelName"]})", [] (const std::string &r) {
		json j = json::parse(r, nullptr, false);
		std::string model = j.is_object() ? j.value("modelName", std::string()) : "";
		size_t a = model.find_first_not_of(' ');
		bool oled = a != std::string::npos && strncasecmp(model.c_str() + a, "OLED", 4) == 0;
		logf("model %s: %s", model.c_str(), oled ? "OLED" : "not OLED");
		app.home.set_oled(oled);
		app.dirty = true;
	});
	luna_call("luna://com.webos.service.systemservice/getPreferences", R"({"keys":["timeFormat"]})", [] (const std::string &r) {
		json j = json::parse(r, nullptr, false);
		std::string f = j.is_object() ? j.value("timeFormat", std::string()) : "";
		app.tv_h24 = f != "HH12";
		apply_lang();
		app.dirty = true;
	});
}

// Fetches the weather when it is due (on a worker); the card shows the saved reading meanwhile.
void weather_tick (double now) {
	if (!app.cfg.weather.on || !app.cfg.weather.city || app.weather_busy || now < app.weather_next) return;
	City city = *app.cfg.weather.city;
	if (weather_fresh(app.weather, city.lat, city.lon, wall_ms())) { app.weather_next = now + WEATHER_FRESH_MS; return; }
	app.weather_busy = true;
	run_background([city] {
		std::string why;
		auto d = fetch_weather(city, &why);
		post_main([d, why] {
			app.weather_busy = false;
			double now = now_ms();
			if (!d) {
				logf("weather: %s", why.c_str());
				app.weather_next = now + RETRY_MS[std::min(app.weather_fails, 5)];
				app.weather_fails++;
				return;
			}
			if (app.weather_fails) logf("weather: read after %d failed tries", app.weather_fails);
			app.weather_fails = 0;
			app.weather = d;
			app.weather_next = now + WEATHER_FRESH_MS;
			write_file(join_path(DATA_DIR, "weather.json"), weather_json(*d));
			// the host that answered is tried first after a restart too
			if (weather_host() != app.weather_host_saved) {
				logf("weather: from %s", FORECAST_HOSTS[weather_host()].c_str());
				app.weather_host_saved = weather_host();
				write_file(join_path(DATA_DIR, "weather-host"), std::to_string(app.weather_host_saved));
			}
			app.home.set_weather(app.weather);
			app.dirty = true;
		});
	});
}

// the calendars as the settings list them: a different list means different data
std::string calendar_key () {
	std::string k;
	for (auto &s : app.cfg.calendar.sources) k += s + "\n";
	return k;
}

void calendar_tick (double now) {
	if (!app.cfg.calendar.on || app.cal_busy || now < app.cal_next) return;
	std::string key = calendar_key();
	if (!app.cal_partial && calendar_fresh(app.calendar, key, wall_ms())) { app.cal_next = now + CAL_FRESH_MS; return; }
	app.cal_busy = true;
	app.home.set_calendar(app.calendar, !app.calendar);
	auto sources = app.cfg.calendar.sources;
	auto known = app.calendar;
	run_background([sources, key, known] {
		std::string why;
		auto d = fetch_calendars(sources, key, known, &why);
		post_main([d, why] {
			app.cal_busy = false;
			double now = now_ms();
			if (!d) {
				logf("calendar: %s", why.c_str());
				app.cal_next = now + RETRY_MS[std::min(app.cal_fails, 5)];
				app.cal_fails++;
			} else {
				app.calendar = d;
				app.cal_partial = !why.empty();
				if (!why.empty()) {
					// some could not be read (they keep what they had): those are tried again soon
					logf("calendar: %s", why.c_str());
					app.cal_next = now + RETRY_MS[std::min(app.cal_fails, 5)];
					app.cal_fails++;
				} else {
					app.cal_fails = 0;
					app.cal_next = now + CAL_FRESH_MS;
				}
				write_file(join_path(DATA_DIR, "calendar.json"), calendar_json(*d));
				logf("calendar: %zu events from %zu calendars", d->events.size(), d->sources.size());
			}
			app.home.set_calendar(app.calendar, false);
			app.dirty = true;
		});
	});
}

void load_calendar () {
	std::string raw;
	if (!read_file(join_path(DATA_DIR, "calendar.json"), raw)) return;
	app.calendar = usable_calendar(calendar_from_json(raw), calendar_key(), wall_ms());
	app.home.set_calendar(app.calendar, false);
}

// the news: Google News for the country in use, then the user's own feeds
std::vector<std::string> news_sources () {
	std::vector<std::string> out;
	if (app.cfg.news.google) out.push_back(google_news_url(country_of(app.cfg.news.country, app.tv_tag, app.lang), app.lang));
	out.insert(out.end(), app.cfg.news.feeds.begin(), app.cfg.news.feeds.end());
	return out;
}

std::string news_key () {
	std::string k;
	for (auto &s : news_sources()) k += s + "\n";
	return k;
}

void news_tick (double now) {
	if (!app.cfg.news.on || app.news_busy || now < app.news_next) return;
	std::string key = news_key();
	if (news_fresh(app.news, key, wall_ms())) { app.news_next = now + NEWS_FRESH_MS; return; }
	app.news_busy = true;
	bool have = app.news && app.news->key == key;
	app.home.set_news(have ? app.news : std::nullopt, !have);
	auto urls = news_sources();
	run_background([urls, key] {
		std::string why;
		auto d = fetch_news(urls, key, &why);
		post_main([d, why, key] {
			app.news_busy = false;
			double now = now_ms();
			if (!d) {
				logf("news: %s", why.c_str());
				app.news_next = now + RETRY_MS[std::min(app.news_fails, 5)];
				app.news_fails++;
			} else {
				app.news_fails = 0;
				app.news = d;
				app.news_next = now + NEWS_FRESH_MS;
				write_file(join_path(DATA_DIR, "news.json"), news_json(*d));
				logf("news: %zu headlines", d->items.size());
			}
			bool have = app.news && app.news->key == key;
			app.home.set_news(have ? app.news : std::nullopt, false);
			app.dirty = true;
		});
	});
}

// ms until the next local midnight (the quote of the day changes then)
double until_midnight () {
	std::time_t t = std::time(nullptr);
	std::tm lt;
	localtime_r(&t, &lt);
	return ((23 - lt.tm_hour) * 3600 + (59 - lt.tm_min) * 60 + (60 - lt.tm_sec)) * 1000.0 + 2000;
}

void quote_tick (double now) {
	if (!app.cfg.quote.on || app.quote_busy || now < app.quote_next) return;
	long long wall = (long long)wall_ms();
	if (app.quote && app.quote->lang == app.lang && app.quote->day == day_key(wall)) { app.quote_next = now + until_midnight(); return; }
	app.quote_busy = true;
	bool shown = app.quote && app.quote->lang == app.lang;      // yesterday's stays until today's is in
	app.home.set_quote(shown ? app.quote : std::nullopt, !shown);
	std::string lang = app.lang;
	run_background([lang, wall] {
		std::string why;
		auto q = fetch_quote(lang, wall, join_path(DATA_DIR, "quote-authors.json"), &why);
		post_main([q, why] {
			app.quote_busy = false;
			double now = now_ms();
			if (!q) {
				logf("quote: %s", why.c_str());
				app.quote_next = now + RETRY_MS[std::min(app.quote_fails, 5)];
				app.quote_fails++;
			} else {
				app.quote_fails = 0;
				app.quote = q;
				app.quote_next = now + until_midnight();
				write_file(join_path(DATA_DIR, "quote.json"), quote_json(*q));
				logf("quote: %s (%s)", q->author.c_str(), q->kind.c_str());
			}
			bool shown = app.quote && app.quote->lang == app.lang;
			app.home.set_quote(shown ? app.quote : std::nullopt, false);
			app.dirty = true;
		});
	});
}

void load_news_and_quote () {
	std::string raw;
	if (read_file(join_path(DATA_DIR, "news.json"), raw)) app.news = news_from_json(raw);
	if (read_file(join_path(DATA_DIR, "quote.json"), raw)) app.quote = quote_from_json(raw);
	app.home.set_news(app.news && app.news->key == news_key() ? app.news : std::nullopt, false);
	app.home.set_quote(app.quote && app.quote->lang == app.lang ? app.quote : std::nullopt, false);
}

void load_weather () {
	std::string raw;
	if (read_file(join_path(DATA_DIR, "weather-host"), raw, 16)) { set_weather_host(atoi(raw.c_str())); app.weather_host_saved = weather_host(); }
	if (!app.cfg.weather.city || !read_file(join_path(DATA_DIR, "weather.json"), raw)) return;
	app.weather = usable_weather(weather_from_json(raw), app.cfg.weather.city->lat, app.cfg.weather.city->lon, wall_ms());
	app.home.set_weather(app.weather);
}

// The TV's time zone for this process (localtime and mktime), from the TV's own setting: a native app may start with another
// zone than the one the TV shows. Logged both ways, so the log tells whether it was needed.
void apply_tv_zone () {
	std::time_t t = std::time(nullptr);
	std::tm before;
	localtime_r(&t, &before);
	std::string r = luna_call_sync("luna://com.webos.service.systemservice/time/getSystemTime", "{}", 4000);
	logf("system time: %s", r.substr(0, 300).c_str());
	json j = json::parse(r, nullptr, false);
	std::string zone = j.is_object() ? j.value("timezone", std::string()) : "";
	if (!zone.empty() && file_exists(join_path("/usr/share/zoneinfo", zone))) {
		setenv("TZ", (":" + zone).c_str(), 1);
		tzset();
	} else if (j.is_object() && j["offset"].is_object() && j["offset"]["value"].is_number()) {
		// minutes east of UTC; a POSIX zone counts west
		int m = j["offset"]["value"].get<int>();
		char posix[32];
		snprintf(posix, sizeof posix, "UTC%c%d:%02d", m > 0 ? '-' : '+', std::abs(m) / 60, std::abs(m) % 60);
		setenv("TZ", posix, 1);
		tzset();
	}
	std::tm after;
	localtime_r(&t, &after);
	logf("time zone: %s, local time %02d:%02d before, %02d:%02d after", zone.c_str(), before.tm_hour, before.tm_min, after.tm_hour, after.tm_min);
}

// ---------- Settings (app.tsx: the tabs, changeOption, onToggle, onReorder and the pickers' choices) ----------

SystemInfo system_info () {
	SystemInfo info;
	info.version = app.version;
	info.tv_tag = app.tv_tag;
	info.update = app.update;
	info.root = (int)app.root;
	info.root_detail = app.root_detail;
	if (app.home_mode) info.home_mode = app.home_mode->mode;
	return info;
}

void refresh_settings () {
	if (app.screen == App::Screen::home) return;
	SystemInfo info = system_info();
	std::vector<SettingsTab> tabs(6);
	tabs[0].id = "apps"; tabs[0].icon = "apps"; tabs[0].label = tr("Apps", "Приложения"); tabs[0].arrange = tr("Arrange", "Порядок"); tabs[0].refresh = true;
	for (auto &a : app.apps) tabs[0].items.push_back(app_tile(a));
	for (auto &a : shown_apps()) tabs[0].shown.push_back(app_tile(a));
	tabs[1].id = "sources"; tabs[1].icon = "inputs"; tabs[1].label = tr("Inputs", "Входы"); tabs[1].arrange = tr("Arrange", "Порядок");
	for (auto &in : app.inputs) tabs[1].items.push_back(input_tile(in));
	for (auto &in : shown_inputs()) tabs[1].shown.push_back(input_tile(in));
	const char *ids[] = {"look", "widgets", "wallpaper", "home"};
	std::string labels[] = {tr("Appearance", "Оформление"), tr("Widgets", "Виджеты"), tr("Wallpaper", "Обои"), tr("Home screen", "Главный экран")};
	for (int i = 0; i < 4; i++) {
		SettingsTab &t = tabs[2 + i];
		t.id = ids[i]; t.icon = ids[i]; t.label = labels[i];
		t.has_options = true;
		t.options = options_for(ids[i], app.cfg, info);
	}
	app.settings.set_tabs(tabs);
	app.settings.set_theme(app.cfg.theme);
	app.picker.set_theme(app.cfg.theme);
	app.gallery.set_theme(app.cfg.theme);
	app.home_method.set_theme(app.cfg.theme);
	app.dirty = true;
}

// ---------- new versions (data/update.h) ----------

using Step = UpdateState::Step;
// the version a download is for, read by the download's worker: another one there stops it
std::mutex update_lock;
std::string update_wanted;

/** The update shown everywhere: its row in Settings, the notice on the home screen and its tile in the row. */
void show_update () {
	const UpdateState &u = app.update;
	if (u.step != Step::downloading) { std::lock_guard<std::mutex> l(update_lock); update_wanted.clear(); }
	std::string notice;
	if (app.cfg.checkUpdates) {
		if (u.step == Step::available || u.step == Step::failed) notice = fmt(tr("Version {0} of the launcher is out", "Вышла версия лаунчера {0}"), u.info.version);
		else if (u.step == Step::downloading) notice = fmt(tr("Downloading version {0}… {1}%", "Скачиваю версию {0}… {1}%"), {u.info.version, std::to_string((int)(u.progress * 100))});
		else if (u.step == Step::ready) notice = fmt(tr("Version {0} is ready to install", "Версия {0} готова к установке"), u.info.version);
	}
	app.home.set_notice(notice);
	rebuild_items();
	refresh_settings();
	app.dirty = true;
}

// What to do once a look is back: nothing more (the daily look, or Check by hand), or what the user asked for on the row or the
// tile (download, or install), which is done for the latest version the look found: a newer one than the one downloaded wins.
enum class AfterCheck { nothing, go_on };
void start_download ();
void ask_install ();

/** Looks in the repository for a later version (in the background). The latest one always wins: a downloaded or downloading
 *  older one is dropped for it, and downloaded files of any other version are deleted. */
void check_update (AfterCheck after = AfterCheck::nothing) {
	if (app.update.checking) return;
	app.update_busy = true;
	app.update.checking = true;
	app.update_next = -1;
	show_update();
	std::string current = app.version;
	run_background([current, after] {
		// a test repository instead of GitHub's, when the data folder names one (update-feed: its address, https or file://)
		std::string feed = UPDATE_FEED, test;
		if (read_file(join_path(DATA_DIR, "update-feed"), test)) {
			while (!test.empty() && isspace((unsigned char)test.back())) test.pop_back();
			if (!test.empty()) { feed = test; logf("update check: the test repository %s", feed.c_str()); }
		}
		HttpResult r = feed.rfind("file://", 0) == 0 ? HttpResult() : http_get(feed, 20, 1 << 20);
		if (feed.rfind("file://", 0) == 0) { r.ok = read_file(feed.substr(7), r.body); if (!r.ok) r.error = "cannot read " + feed; }
		auto u = r.ok ? parse_update_feed(r.body, APP_ID, feed != UPDATE_FEED) : std::nullopt;
		bool newer = u && version_newer(u->version, current);
		// downloads of any other version than the latest are of no use (an older one, or one the TV has now); unknown while offline
		if (r.ok)
			for (auto &e : list_dir(DATA_DIR))
				if (e.name.rfind("update-", 0) == 0 && e.name.find(".ipk") != std::string::npos && (!newer || e.name.rfind(update_file("", u->version).substr(1), 0) != 0)) unlink(join_path(DATA_DIR, e.name).c_str());   // the latest's .part too, if it is being downloaded
		// downloaded before (and the install put off): ready at once, if it is whole
		bool have = newer && sha256_file(update_file(DATA_DIR, u->version)) == u->sha256;
		post_main([r, u, current, newer, have, after] {
			app.update_busy = false;
			app.update.checking = false;
			// the next look a day later, whatever this one found (offline: the next day will do)
			if (app.cfg.checkUpdates) app.update_next = now_ms() + UPDATE_EVERY_MS;
			app.update.unreachable = !r.ok;
			if (!r.ok) {
				logf("update check: %s", r.error.c_str());
				// what was found before stays; asked to go on with it, it does
				if (after == AfterCheck::go_on && app.update.step == Step::ready) ask_install();
				else if (after == AfterCheck::go_on && (app.update.step == Step::available || app.update.step == Step::failed)) start_download();
				show_update();
				return;
			}
			if (!newer) {
				logf("update check: %s is the latest", current.c_str());
				app.update = {};
				app.update.latest = true;
				show_update();
				return;
			}
			bool same = app.update.info.version == u->version;
			if (!same || (app.update.step != Step::downloading && app.update.step != Step::ready)) {
				// a newer one than the one being downloaded or downloaded: it takes its place (the old download stops by itself)
				if (!same && app.update.step != Step::none) logf("update check: %s replaces %s", u->version.c_str(), app.update.info.version.c_str());
				else logf("update check: version %s is out%s", u->version.c_str(), have ? " (downloaded already)" : "");
				app.update = {};
				app.update.info = *u;
				app.update.step = have ? Step::ready : Step::available;
			}
			if (after == AfterCheck::go_on) {
				if (app.update.step == Step::ready) ask_install();
				else if (app.update.step == Step::available) start_download();
			}
			show_update();
		});
	});
}

void install_update ();

// For testing with no remote ({"open":"update:install"}): the question answered with Install by itself
bool test_install = false;

void ask_install () {
	if (test_install) { install_update(); return; }
	app.menu.open_question(fmt(tr("Install version {0}?", "Установить версию {0}?"), app.update.info.version),
		tr("The launcher closes for a moment, the new version is installed and starts by itself. Your settings stay.",
			"Лаунчер ненадолго закроется, новая версия установится и запустится сама. Настройки сохранятся."),
		tr("Install", "Установить"), [] { install_update(); }, now_ms());
	app.dirty = true;
}

void start_download () {
	if (app.update.step == Step::downloading || app.update.info.url.empty()) return;
	UpdateInfo info = app.update.info;
	{ std::lock_guard<std::mutex> l(update_lock); update_wanted = info.version; }
	app.update.step = Step::downloading;
	app.update.progress = 0;
	app.update.why.clear();
	show_update();
	logf("update: downloading %s", info.version.c_str());
	run_background([info] {
		std::string file = update_file(DATA_DIR, info.version);
		HttpResult r = http_download(info.url, file, [info] (long done, long total) {
			if (total <= 0) total = info.size;
			double p = total > 0 ? std::min(1.0, (double)done / total) : 0;
			post_main([p, info] {
				if (app.update.step == Step::downloading && app.update.info.version == info.version && (int)(p * 100) != (int)(app.update.progress * 100)) { app.update.progress = p; show_update(); }
			});
			// a newer version found meanwhile: this one is not wanted any more
			std::lock_guard<std::mutex> l(update_lock);
			return update_wanted == info.version;
		});
		if (!r.ok && r.error == "stopped") { unlink(file.c_str()); logf("update: download of %s stopped for a newer one", info.version.c_str()); return; }
		std::string why = r.ok ? "" : r.error;
		if (r.ok && sha256_file(file) != info.sha256) { why = tr("the file came damaged", "файл пришёл повреждённым"); unlink(file.c_str()); }
		post_main([why, info] {
			if (app.update.info.version != info.version) return;
			if (!why.empty()) {
				logf("update: download failed: %s", why.c_str());
				app.update.step = Step::failed;
				app.update.why = why;
				show_update();
				app.toast.show(fmt(tr("Could not download the update: {0}", "Не удалось скачать обновление: {0}"), why), true, now_ms());
				return;
			}
			logf("update: %s downloaded", info.version.c_str());
			app.update.step = Step::ready;
			show_update();
			ask_install();
		});
	});
}

/** Installs the downloaded version as the Homebrew Channel does (the TV's installer, through the root shell). The installer closes
 *  the launcher, so the rest runs on the TV by itself: it waits for the new version's appinfo.json and opens the launcher again. */
void install_update () {
	std::string version = app.update.info.version, file = update_file(DATA_DIR, version);
	if (!file_exists(file)) { app.update.step = Step::available; show_update(); return; }
	app.menu.open_working(tr("Installing the update…", "Устанавливаю обновление…"), tr("In a few seconds the launcher closes and starts again as the new version.",
		"Через несколько секунд лаунчер закроется и запустится уже новой версией."), now_ms());
	app.dirty = true;
	logf("update: installing %s", version.c_str());
	std::string result = "/tmp/" + std::string(APP_ID) + "-update", appinfo = join_path(package_dir(), "appinfo.json");
	std::string payload = "{\"id\":\"com.ares.defaultName\",\"ipkUrl\":\"" + file + "\",\"subscribe\":true}";
	std::string open = "{\"id\":\"" + RUN_ID + "\"}";
	std::string script =
		"luna-send -w 300000 -i luna://com.webos.appInstallService/dev/install " + shell_quote(payload) + " > " + result + ".log 2>&1 & p=$!; i=0; "
		"until grep -q '\"version\": *\"" + version + "\"' " + shell_quote(appinfo) + " || grep -q errorText " + result + ".log || [ $i -ge 240 ]; do sleep 1; i=$((i + 1)); done; "
		"sleep 2; kill $p 2>/dev/null; "
		"if grep -q '\"version\": *\"" + version + "\"' " + shell_quote(appinfo) + "; then echo installed > " + result + "; rm -f " + shell_quote(file) + "; "
		"luna-send -n 1 luna://com.webos.applicationManager/launch " + shell_quote(open) + "; else echo failed > " + result + "; fi";
	run_background([script, result] {
		bool ok = false;
		hbc_exec("rm -f " + result + " " + result + ".log; ( setsid nohup sh -c " + shell_quote(script) + " </dev/null >/dev/null 2>&1 & ); echo started", 8000, &ok);
		// still here after a while: it did not take
		for (int i = 0; ok && i < 130; i++) {
			std::this_thread::sleep_for(std::chrono::seconds(2));
			std::string out = hbc_exec("cat " + result + " 2>/dev/null; true", 4000);
			if (out.find("failed") != std::string::npos) { ok = false; break; }
			if (out.find("installed") != std::string::npos) return;
		}
		std::string why = ok ? tr("no answer from the TV's installer", "установщик ТВ не ответил") : tr("the TV's installer refused it", "установщик ТВ отказал");
		post_main([why] {
			logf("update: install failed: %s", why.c_str());
			app.menu.close();
			app.toast.show(fmt(tr("Could not install the update: {0}", "Не удалось установить обновление: {0}"), why), true, now_ms());
			show_update();
		});
	});
}

/** The update row, or the update tile: look by hand; download; or install what was downloaded (at once: asked to install, it
 *  installs). Before a download the repository is looked at once more, so it is always the latest version that is downloaded. */
void update_pressed () {
	if (app.update.checking || app.update.step == Step::downloading) return;
	if (app.root == Root::no && app.update.step != Step::none) return;      // installed only with root (the row says so)
	if (app.update.step == Step::ready) { ask_install(); return; }
	check_update(app.update.step == Step::none ? AfterCheck::nothing : AfterCheck::go_on);
}

/** The settings written to disk. A write that fails (the TV's storage is full) is tried again after the files the launcher can
 *  make again are removed; the file on disk is replaced only by a complete new one (write_file), so a failure leaves the settings
 *  saved before, never half a file. When it still fails, the screen says so: the change lasts until the launcher is loaded anew. */
void save_config (const UserConfig &cfg) {
	std::string file = join_path(DATA_DIR, "config.json"), text = config_json(cfg);
	if (write_file(file, text)) return;
	long long before = free_bytes(DATA_DIR);
	bool keep_update = app.update.step == UpdateState::Step::downloading || app.update.step == UpdateState::Step::ready;
	long long freed = clear_rebuildable(DATA_DIR, keep_update);
	logf("config: could not save (%lld bytes free); %lld bytes of saved copies removed", before, freed);
	if (write_file(file, text)) {
		app.toast.show(tr("The TV's storage is full. The launcher removed its saved copies to keep your settings.", "Память ТВ заполнена. Лаунчер удалил свои сохранённые копии, чтобы записать настройки."), true, now_ms());
		return;
	}
	logf("config: still could not save (%lld bytes free)", free_bytes(DATA_DIR));
	app.toast.show(before >= 0 && before < 65536
		? tr("The TV's storage is full: the setting was not saved. Free some space on the TV.", "Память ТВ заполнена: настройка не сохранена. Освободите место на ТВ.")
		: tr("The setting could not be saved", "Не удалось сохранить настройку"), true, now_ms());
}

/** New settings: kept on disk and put to use (the screens, the wallpaper, the language, and the widgets read again when what they
 *  show changed). */
void update_config (const UserConfig &next) {
	UserConfig old = app.cfg;
	app.cfg = next;
	save_config(next);
	app.home.set_config(next);
	const auto &a = old.wallpaper, &b = next.wallpaper;
	if (a.mode != b.mode || a.path != b.path || a.paths != b.paths || a.intervalSec != b.intervalSec || a.shuffle != b.shuffle || old.theme != next.theme || old.frost != next.frost)
		app.wallpaper.configure(next.wallpaper, next.theme, next.frost);
	if (a.mode != b.mode || a.path != b.path || a.paths != b.paths) sync_collection(app.screen != App::Screen::gallery);
	apply_dim();
	if (old.language != next.language || old.clockFormat != next.clockFormat) apply_lang();
	if (old.checkUpdates != next.checkUpdates) {
		app.update_next = next.checkUpdates ? now_ms() : -1;
		if (!next.checkUpdates && app.update.step != UpdateState::Step::downloading) app.update = {};
		show_update();
	}
	double now = now_ms();
	bool city_changed = old.weather.city.has_value() != next.weather.city.has_value() ||
		(next.weather.city && (old.weather.city->lat != next.weather.city->lat || old.weather.city->lon != next.weather.city->lon));
	if (city_changed) {
		app.weather = next.weather.city ? usable_weather(app.weather, next.weather.city->lat, next.weather.city->lon, wall_ms()) : std::nullopt;
		app.home.set_weather(app.weather);
	}
	if (city_changed || (next.weather.on && !old.weather.on)) { app.weather_next = now; app.weather_fails = 0; }
	if (old.calendar.sources != next.calendar.sources || (next.calendar.on && !old.calendar.on)) {
		app.calendar = usable_calendar(app.calendar, calendar_key(), wall_ms());
		app.home.set_calendar(app.calendar, next.calendar.on && !app.calendar);
		app.cal_next = now;
		app.cal_fails = 0;
	}
	// the news key follows the sources and the language
	bool have = app.news && app.news->key == news_key();
	app.home.set_news(have ? app.news : std::nullopt, next.news.on && !have);
	if (!have || (next.news.on && !old.news.on)) { app.news_next = now; app.news_fails = 0; }
	if (next.quote.on && (!old.quote.on || old.language != next.language)) { app.quote_next = now; app.quote_fails = 0; }
	rebuild_items();
	app.dirty = true;
}

void open_picker (Picker::Kind kind) {
	std::vector<std::string> list = kind == Picker::Kind::calendars ? app.cfg.calendar.sources : kind == Picker::Kind::feeds ? app.cfg.news.feeds : std::vector<std::string>();
	// the country set on the TV; the region of its language when it did not say
	app.picker.set_tv_country(!app.tv_country.empty() ? app.tv_country : region_of(app.tv_tag));
	app.picker.open(kind, country_of(app.cfg.news.country, app.tv_tag, app.lang), list, app.cfg.useHomebrew, now_ms());
	app.screen = App::Screen::picker;
	app.dirty = true;
}

/** How the launcher opens on Home, read from the TV (the Settings row and the screen of the ways show it). */
void read_mode () {
	run_background([] {
		bool root = false;
		std::string why;
		auto info = read_home_mode(APP_ID, &root, &why);
		post_main([info, root, why] {
			if (app.root != (root ? Root::yes : Root::no)) logf("root: %s%s%s", root ? "yes" : "no", why.empty() ? "" : ": ", why.c_str());
			app.root = root ? Root::yes : Root::no;
			app.root_detail = why;
			app.home_mode = info;
			if (info) { app.default_home = info->mode == "default"; app.helper_on = info->mode == "helper"; }
			app.home_method.set_info(info, false);
			refresh_settings();
		});
	});
}

void cleanup_home ();

void change_option (const std::string &id, Press press) {
	const Pref *pref = find_pref(id);
	if (!pref) return;
	double now = now_ms();
	if (id == "weatherOn" && !app.cfg.weather.city) { app.toast.show(tr("Choose a city first", "Сначала выберите город"), false, now); open_picker(Picker::Kind::city); return; }
	if (id == "calendarOn" && app.cfg.calendar.sources.empty()) { app.toast.show(tr("Add a calendar first", "Сначала подключите календарь"), false, now); open_picker(Picker::Kind::calendars); return; }
	if (id == "newsOn" && !app.cfg.news.google && app.cfg.news.feeds.empty()) { app.toast.show(tr("Turn on Google News or add a feed first", "Сначала включите Google Новости или добавьте ленту"), false, now); open_picker(Picker::Kind::feeds); return; }
	if (pref->kind == "info") return;
	if (pref->kind == "action") {
		if (id == "weatherCity") open_picker(Picker::Kind::city);
		else if (id == "calendarUrl") open_picker(Picker::Kind::calendars);
		else if (id == "newsFeeds") open_picker(Picker::Kind::feeds);
		else if (id == "newsCountry") open_picker(Picker::Kind::country);
		else if (id == "restartTv") {
			app.menu.open_question(tr("Restart the TV?", "Перезагрузить ТВ?"), tr("The TV switches off and starts again, which takes about a minute.", "Телевизор выключится и запустится снова, это займёт около минуты."),
				tr("Restart", "Перезагрузить"), [] {
					logf("restart the TV");
					app.menu.open_working(tr("Restarting the TV…", "Перезагрузка ТВ…"), tr("The TV switches off in a few seconds and starts again in about a minute.",
						"Через несколько секунд телевизор выключится и примерно через минуту запустится снова."), now_ms());
					run_background([] {
						// a full restart (not standby) through the TV's power service, as its own menu does, which shows the TV's logo
						// as it starts again; the plain "reboot" of the root shell if it says no
						bool ok = false;
						std::string out = hbc_exec("luna-send -n 1 luna://com.webos.service.tvpower/power/reboot '{\"reason\":\"reset\"}'", 10000, &ok);
						logf("restart: %s", out.substr(0, 160).c_str());
						if (!ok || !reply_ok(out)) { ok = false; hbc_exec("reboot", 10000, &ok); }
						if (!ok) post_main([] { app.menu.close(); app.toast.show(tr("Could not restart the TV. This needs the Homebrew Channel (root)", "Не удалось перезагрузить ТВ. Нужен Homebrew Channel (рут)"), true, now_ms()); app.dirty = true; });
					});
				}, now);
			app.dirty = true;
		}
		else if (id == "update") update_pressed();
		else if (id == "resetLook") { update_config(reset_look(app.cfg)); app.toast.show(tr("Look and clock reset", "Вид и часы сброшены"), false, now); }
		else if (id == "wallpaper") {
			std::vector<std::string> chosen = app.cfg.wallpaper.mode == "pictures" ? app.cfg.wallpaper.paths : app.cfg.wallpaper.mode == "image" ? std::vector<std::string>{app.cfg.wallpaper.path} : std::vector<std::string>();
			app.gallery.open(chosen, now);
			app.screen = App::Screen::gallery;
			app.dirty = true;
		}
		else if (id == "homeCleanup") {
			app.menu.open_question(tr("Remove everything from the TV?", "Убрать всё с ТВ?"), tr("The way of opening the launcher is switched off and what it put on the TV is removed. The launcher itself stays installed.",
				"Способ открытия лаунчера выключится, а всё, что он ставил на ТВ, будет удалено. Сам лаунчер останется установленным."),
				tr("Remove", "Убрать"), [] { cleanup_home(); }, now);
			app.dirty = true;
		}
		else if (id == "homeMethod") {
			app.home_method.open(now);
			app.home_method.set_info(app.home_mode, true);
			app.screen = App::Screen::home_method;
			read_mode();
			app.dirty = true;
		}
		return;
	}
	bool changed = false;
	UserConfig next = apply_pref(app.cfg, id, press, &changed);
	if (changed) update_config(next);
}

void setup_settings () {
	app.settings.on_close = [] { app.screen = App::Screen::home; app.home.mark_board(); app.dirty = true; };
	app.settings.on_option = change_option;
	app.settings.on_refresh = [] {
		app.manual_refresh = true;
		app.toast.show(tr("Looking for new apps…", "Ищем новые приложения…"), false, now_ms());
		refresh_apps();
	};
	app.settings.on_toggle = [] (const std::string &tab, const std::string &key) {
		UserConfig next = app.cfg;
		if (tab == "apps") next.hiddenIds = toggle_hidden(hidden_ids(), key);     // apps are shown unless hidden
		else {
			auto &ids = next.sourceIds;
			auto it = std::find(ids.begin(), ids.end(), key);
			if (it == ids.end()) ids.push_back(key); else ids.erase(it);
		}
		update_config(next);
	};
	app.settings.on_reorder = [] (const std::string &tab, const std::vector<std::string> &keys) {
		// the tiles of this tab take the places they already hold on the shared row, in their new order
		std::vector<std::string> row, apps_shown, inputs_shown;
		for (auto &t : ordered_tiles()) row.push_back(t.key);
		for (auto &a : shown_apps()) apps_shown.push_back(a.id);
		for (auto &in : shown_inputs()) inputs_shown.push_back(in.appId);
		auto update = order_update(reslot(row, keys), apps_shown, inputs_shown, app.cfg.appIds, app.cfg.sourceIds);
		UserConfig next = app.cfg;
		next.tileOrder = update.tileOrder;
		next.hiddenIds = hidden_ids();
		if (tab == "apps") {
			std::vector<std::string> ids = keys;
			if (app.cfg.appIds) for (auto &id : *app.cfg.appIds) if (std::find(keys.begin(), keys.end(), id) == keys.end()) ids.push_back(id);
			next.appIds = ids;
		} else next.appIds = update.appIds;
		next.sourceIds = tab == "sources" ? keys : update.sourceIds;
		update_config(next);
	};
	auto back = [] { app.screen = App::Screen::settings; refresh_settings(); };
	app.picker.on_close = back;
	app.picker.on_city = [back] (const City &city) {
		UserConfig next = app.cfg;
		next.weather.city = city;
		next.weather.on = true;
		update_config(next);
		back();
		app.toast.show(fmt(tr("Weather for {0}", "Погода для города: {0}"), city.name), false, now_ms());
	};
	app.picker.on_country = [back] (const std::string &country) {
		UserConfig next = app.cfg;
		next.news.country = country;
		update_config(next);
		back();
	};
	app.picker.on_calendars = [] (const std::vector<std::string> &sources) {
		UserConfig next = app.cfg;
		next.calendar.sources = sources;
		next.calendar.on = !sources.empty();
		update_config(next);
		app.picker.set_list(sources);
		app.toast.show(!sources.empty() ? tr("Connecting the calendars…", "Календари подключаются…") : tr("Calendars removed", "Календари убраны"), false, now_ms());
	};
	app.picker.on_feeds = [] (const std::vector<std::string> &feeds) {
		UserConfig next = app.cfg;
		next.news.feeds = feeds;
		next.news.on = next.news.on && (next.news.google || !feeds.empty());
		update_config(next);
		app.picker.set_list(feeds);
	};
}

// ---------- the wallpaper gallery and the menu of a tile (app.tsx onChooseWallpaper, moveTile, hideTile, removeApp) ----------

void setup_gallery_and_menu () {
	app.gallery.set_store(&app.collection);
	app.gallery.on_close = [] { app.gallery.release(); app.screen = App::Screen::settings; refresh_settings(); sync_collection(true); };
	app.gallery.on_choose = [] (const WallpaperChoice &choice) {
		WallpaperConfig w = app.cfg.wallpaper;        // the interval and the shuffle stay
		if (choice.mode == "gradient") { w.mode = "gradient"; w.path.clear(); w.paths.clear(); }
		else if (choice.mode == "pictures" && choice.paths.size() == 1) { w.mode = "image"; w.path = choice.paths[0]; w.paths.clear(); }
		else if (choice.mode == "pictures") { w.mode = "pictures"; w.path = choice.paths[0]; w.paths = choice.paths; }
		else { w.mode = choice.mode; w.path = choice.path; w.paths.clear(); }
		UserConfig next = app.cfg;
		next.wallpaper = w;
		update_config(next);
		app.gallery.release();
		app.screen = App::Screen::settings;
		refresh_settings();
		sync_collection(true);
		int coming = app.collection.missing();
		if (coming > 0)
			app.toast.show(fmt(tr("The wallpaper downloads in the background: {0}", "Обои скачиваются в фоне: {0}"),
				count(coming, "picture", "pictures", "картинка", "картинки", "картинок")), false, now_ms());
		else app.toast.show(choice.mode == "gradient" ? tr("Default background restored", "Фон по умолчанию восстановлен") : w.mode == "image" ? tr("Loading the wallpaper…", "Загружаем обои…") : tr("Starting the slideshow…", "Запускаем слайд-шоу…"), false, now_ms());
	};
	// OK held on a tile: it is lifted to be moved, with buttons over it to hide it and (an app that can be removed) uninstall it
	app.home.on_long_press = [] (const TileModel &t) {
		if (t.key == "lg-home" || t.key == "settings" || t.key == "system-settings" || t.key == "update") return;        // they stay in place
		bool removable = false;
		for (auto &a : app.apps) if (a.id == t.key) removable = uninstall_verdict(a.id, a.iconPath, a.removable).ok;
		app.home.lift(removable, now_ms());
		app.dirty = true;
	};
	app.home.on_reorder = [] (const std::vector<std::string> &all) {
		// the row as the home screen shows it now: the apps and inputs in their new order (the fixed tiles are left out)
		std::vector<std::string> keys, apps_shown, inputs_shown;
		for (auto &k : all) if (k != "lg-home" && k != "settings" && k != "system-settings" && k != "update") keys.push_back(k);
		for (auto &a : shown_apps()) apps_shown.push_back(a.id);
		for (auto &in : shown_inputs()) inputs_shown.push_back(in.appId);
		std::optional<std::vector<std::string>> old_ids;
		if (app.cfg.hiddenIds) old_ids = app.cfg.appIds ? *app.cfg.appIds : std::vector<std::string>();
		auto update = order_update(keys, apps_shown, inputs_shown, old_ids, app.cfg.sourceIds);
		UserConfig next = app.cfg;
		next.tileOrder = update.tileOrder;
		if (update.appIds) next.appIds = update.appIds;
		next.sourceIds = update.sourceIds;
		update_config(next);
	};
	app.home.on_hide = [] (const std::string &key) {
		std::string title;
		bool is_app = false;
		for (auto &a : app.apps) if (a.id == key) { is_app = true; title = a.title; }
		for (auto &in : app.inputs) if (in.appId == key) title = in.label;
		UserConfig next = app.cfg;
		if (is_app) next.hiddenIds = toggle_hidden(hidden_ids(), key);
		else next.sourceIds.erase(std::remove(next.sourceIds.begin(), next.sourceIds.end(), key), next.sourceIds.end());
		update_config(next);
		app.toast.show(fmt(tr("{0} hidden. Bring it back in Settings → {1}", "{0} скрыто. Вернуть можно в Настройки → {1}"), {title, is_app ? tr("Apps", "Приложения") : tr("Inputs", "Входы")}), false, now_ms());
	};
	app.home.on_uninstall = [] (const std::string &key) {
		std::string title;
		for (auto &a : app.apps) if (a.id == key) title = a.title;
		app.menu_key = key;
		app.menu.open_confirm(title, now_ms());
		app.dirty = true;
	};
	app.menu.on_close = [] { app.menu.close(); app.dirty = true; };
	app.menu.on_uninstall = [] {
		std::string id = app.menu_key, title;
		for (auto &a : app.apps) if (a.id == id) title = a.title;
		run_background([id, title] {
			std::string result = uninstall_app(id);
			post_main([id, title, result] {
				logf("uninstall %s: %s", id.c_str(), result.c_str());
				app.menu.close();
				if (result == "removed") {
					UserConfig next = app.cfg;
					if (next.appIds) next.appIds->erase(std::remove(next.appIds->begin(), next.appIds->end(), id), next.appIds->end());
					if (next.hiddenIds) next.hiddenIds->erase(std::remove(next.hiddenIds->begin(), next.hiddenIds->end(), id), next.hiddenIds->end());
					app.apps.erase(std::remove_if(app.apps.begin(), app.apps.end(), [&id] (const AppEntry &a) { return a.id == id; }), app.apps.end());
					update_config(next);
					app.toast.show(fmt(tr("{0} uninstalled", "{0} удалено"), title), false, now_ms());
				} else if (result == "still-installed")
					app.toast.show(fmt(tr("The TV did not remove {0} (details: {1})", "ТВ не удалил {0} (подробности: {1})"), {title, UNINSTALL_LOG}), true, now_ms());
				else app.toast.show(tr("Could not uninstall the app", "Не удалось удалить приложение") + tr(". The TV did not accept the command", ". ТВ не принял команду"), true, now_ms());
				refresh_apps();         // the list again, to see whether it is really gone
				app.dirty = true;
			});
		});
	};
}

/** Removes everything the ways of opening put on the TV: the way switched off (homemode.sh, as "Off"), then the folders of
 *  their state and copies (scripts/gen-tools.sh: launcher-… for the web version's id, launcher-native-… for the native one). */
void cleanup_home () {
	app.menu.open_working(tr("Removing…", "Убираю…"), tr("This takes a few seconds.", "Это займёт несколько секунд."), now_ms());
	app.dirty = true;
	logf("home mode: cleaning up");
	run_background([] {
		hbc_exec(restore_last_input_command(APP_ID), 10000);
		SwitchResult r = switch_home_mode(APP_ID, "none");
		std::string prefix = std::string(APP_ID) == "tld.my.simplelauncher" ? "/media/developer/launcher-" : "/media/developer/launcher-native-";
		bool ok = false;
		std::string out;
		if (r.ok) out = hbc_exec("rm -rf " + shell_quote(prefix + "defaulthome") + " " + shell_quote(prefix + "homehook") + " && echo removed", 15000, &ok);
		auto info = read_home_mode(APP_ID);
		logf("home mode: cleanup %s%s%s", r.ok ? "ok" : "failed: ", r.why.c_str(), r.ok ? (" " + out).c_str() : "");
		bool done = r.ok && ok;
		post_main([r, info, done] {
			app.menu.close();
			app.home_mode = info;
			if (info) { app.default_home = info->mode == "default"; app.helper_on = info->mode == "helper"; }
			refresh_settings();
			app.toast.show(done ? tr("Everything is removed. Home opens LG Home", "Всё убрано. Home открывает LG Home")
				: fmt(tr("Could not remove everything: {0}", "Не удалось убрать всё: {0}"), r.why.empty() ? tr("the Homebrew Channel did not answer", "Homebrew Channel не ответил") : r.why), !done, now_ms());
			app.dirty = true;
		});
	});
}

void setup_home_method () {
	app.home_method.on_close = [] { app.screen = App::Screen::settings; refresh_settings(); };
	app.home_method.on_pick = [] (const std::string &mode) {
		// switched on the TV by tools/homemode.sh (the helper by tools/helper.sh), which first switches off whatever is on
		app.mode_busy = mode;
		app.home_method.set_busy(mode);
		app.menu.open_working(fmt(tr("Switching: {0}…", "Переключаю: {0}…"), mode_text(mode).name), mode == "none"
			? tr("This takes a few seconds. The launcher closes and LG Home opens.", "Это займёт несколько секунд. Лаунчер закроется, откроется LG Home.")
			: mode == "helper" ? tr("This takes a few seconds.", "Это займёт несколько секунд.")
			: tr("This takes a few seconds. The screen blinks once: the launcher starts again.", "Это займёт несколько секунд. Экран один раз моргнёт: лаунчер запустится заново."), now_ms());
		app.dirty = true;
		logf("home mode: switching to %s", mode.c_str());
		run_background([mode] {
			// off the Swap and the Helper: the TV's own last input back first (so the official way keeps that one to put back, and
			// names the launcher there itself)
			if (mode != "mount" && mode != "helper") hbc_exec(restore_last_input_command(APP_ID), 10000);
			SwitchResult r = switch_home_mode(APP_ID, mode);
			auto info = read_home_mode(APP_ID);
			post_main([mode, r, info] {
				logf("home mode: %s %s%s%s", r.ok ? "ok" : "failed", mode.c_str(), r.why.empty() ? "" : ": ", r.why.c_str());
				app.mode_busy.clear();
				app.home_method.set_busy("");
				app.menu.close();
				app.home_mode = info;
				if (info) { app.default_home = info->mode == "default"; app.helper_on = info->mode == "helper"; }
				app.home_method.set_info(info, false);
				refresh_settings();
				std::string name = mode_text(mode).name;
				app.toast.show(r.ok ? fmt(tr("Done: {0}", "Готово: {0}"), name) : fmt(tr("Could not switch: {0}", "Не удалось переключить: {0}"), r.why), !r.ok, now_ms());
				app.dirty = true;
			});
		});
	};
}

// ---------- quick settings ----------

/** A luna call to the TV's settings with the app's own access, and through the root shell when that gives nothing. */
std::string settings_call (const std::string &method, const std::string &payload, bool (*good) (const std::string &)) {
	std::string r = luna_call_sync("luna://com.webos.settingsservice/" + method, payload, 4000);
	if (good(r)) return r;
	std::string out = hbc_exec("luna-send -n 1 luna://com.webos.settingsservice/" + method + " '" + payload + "'", 8000);
	size_t b = out.find('{');
	return b == std::string::npos ? "" : out.substr(b);
}

std::string read_setting (const QuickSetting &s) {
	std::string payload = "{\"category\":\"" + s.category + "\",\"keys\":[\"" + s.key + "\"]}";
	return setting_from_reply(settings_call("getSystemSettings", payload, [] (const std::string &r) { return r.find("\"settings\"") != std::string::npos; }), s.key);
}

/** What the TV's settings are now, and the values each has (as its own menu shows them), for the tiles and their lists. */
void read_quick_values () {
	for (auto &s : quick_settings()) {
		if (s.category.empty()) continue;
		QuickSetting setting = s;
		run_background([setting] {
			std::string v = read_setting(setting);
			std::string payload = "{\"category\":\"" + setting.category + "\",\"keys\":[\"" + setting.key + "\"]}";
			auto values = values_from_desc(settings_call("getSystemSettingDesc", payload, [] (const std::string &r) { return r.find("\"results\"") != std::string::npos; }), setting.key);
			logf("quick %s: now %s, the TV has %d", setting.id.c_str(), v.c_str(), (int)values.size());
			post_main([setting, v, values] {
				app.quick.set_value(setting.id, v);
				if (!values.empty()) app.quick.set_options(setting.id, values);
				app.dirty = true;
			});
		});
	}
}

void setup_quick () {
	app.home.on_quick = [] { app.quick.open(now_ms()); read_quick_values(); app.dirty = true; };
	app.quick.on_close = [] { app.quick.close(now_ms()); app.dirty = true; };
	app.quick.on_activate = [] (const std::string &id) {
		double now = now_ms();
		if (id == "tvsettings") { app.quick.close(now); launch_system_settings(); return; }
		if (id == "screenoff") {
			// the picture off, the sound on: the TV's power service (energySaving "screen_off" is not on this model); the TV
			// itself turns it back on at a key
			app.quick.close(now);
			run_background([] {
				bool ok = false;
				std::string out = hbc_exec("luna-send -n 1 luna://com.webos.service.tvpower/power/turnOffScreen '{}'", 8000, &ok);
				logf("screen off: %s", out.substr(0, 160).c_str());
				bool off = ok && reply_ok(out.substr(out.find('{') == std::string::npos ? 0 : out.find('{')));
				if (!off) post_main([] { app.toast.show(tr("The TV did not switch the screen off", "ТВ не выключил экран"), true, now_ms()); app.dirty = true; });
			});
		}
	};
	app.quick.on_choose = [] (const std::string &id, const std::string &value) {
		const QuickSetting *s = find_quick(id);
		if (!s) return;
		QuickSetting setting = *s;
		run_background([setting, value] {
			bool ok = false;
			std::string out = hbc_exec(set_command(setting.category, setting.key, value), 8000, &ok);
			size_t b = out.find('{');
			bool taken = ok && reply_ok(b == std::string::npos ? out : out.substr(b));
			// the TV can say yes and still keep the old one (the sound's Personal here): what it is now is read back
			SDL_Delay(400);
			std::string now_is = read_setting(setting);
			logf("quick %s: %s -> %s, now %s", setting.id.c_str(), value.c_str(), taken ? "taken" : "refused", now_is.c_str());
			post_main([setting, value, now_is] {
				if (!now_is.empty()) app.quick.set_value(setting.id, now_is);
				if (now_is != value)
					app.toast.show(tr("The TV did not switch to it. It can be chosen in the TV's settings", "ТВ не переключился на этот режим. Его можно выбрать в настройках ТВ"), true, now_ms());
				app.dirty = true;
			});
		});
	};
}

void open_settings () {
	app.screen = App::Screen::settings;
	if (app.cfg.glass) app.wallpaper.prepare_frost();
	refresh_settings();
	app.settings.open(now_ms());
	read_mode();
	app.dirty = true;
}

/** Back to the home screen (Home pressed, or the launcher went to the background). */
void close_screens () {
	if (app.menu.open()) { app.menu.close(); app.dirty = true; }
	if (app.quick.is_open()) { app.quick.close(now_ms()); app.dirty = true; }
	if (app.screen == App::Screen::home) return;
	if (app.screen == App::Screen::gallery) app.gallery.release();
	if (app.picker.typing()) SDL_StopTextInput();
	app.screen = App::Screen::home;
	app.home.mark_board();
	app.dirty = true;
}

/** The launcher is on screen: the helper's file for it touched now and again 1 and 2 s later (the helper starts its check
 *  when the TV reports the launcher in front, which can come just after this; homehook.ts markAliveSoon). The file is made
 *  writable to the launcher by helper.sh; when it is not, the root shell touches it. */
void mark_alive () {
	run_background([] {
		std::string path = alive_file(APP_ID);
		for (int i = 0; i < 3; i++) {
			if (i) std::this_thread::sleep_for(std::chrono::seconds(1));
			// (before the launcher knows the helper is on: the file is there only when it is)
			if (utime(path.c_str(), nullptr) != 0 && app.helper_on) hbc_exec("touch " + shell_quote(path), 4000);
		}
	});
}

// ---------- an update installed while this copy runs (lib/update.ts) ----------
// The TV keeps the launcher loaded (in standby, and as Home behind the Home button), so after an update through the Homebrew
// Channel the old program would go on running until a full restart. The program on disk is noted at the start; when the
// launcher comes on screen again and that file is another one, it asks the TV (through the root shell, which outlives it) to
// open it again in a moment, and closes: one blink, and the new version is on screen. (Closing as it leaves the screen was
// tried: while it is not running nothing brings it back over LG Home after the last app, seen on webOS 10.3.2.)
struct ProgramFile { dev_t dev = 0; ino_t ino = 0; time_t mtime = 0; bool known = false; };
ProgramFile program_file () {
	ProgramFile f;
	struct stat st;
	if (stat(join_path(package_dir(), "simplelauncher").c_str(), &st) == 0) { f.dev = st.st_dev; f.ino = st.st_ino; f.mtime = st.st_mtime; f.known = true; }
	return f;
}
ProgramFile started_as;

bool updated_on_disk () {
	ProgramFile now = program_file();
	return started_as.known && now.known && (now.dev != started_as.dev || now.ino != started_as.ino || now.mtime != started_as.mtime);
}

void on_activate (const TileModel &t) {
	if (t.key == "settings") { open_settings(); return; }
	if (t.key == "system-settings") { launch_system_settings(); return; }
	if (t.key == "update") { open_settings(); app.settings.focus_option("home", "update", now_ms()); update_pressed(); return; }
	if (t.key == "lg-home") {
		app.lg_home_by_us = now_ms();
		// the helper would open the launcher over it at once: it stands back first, until LG Home is left
		if (app.helper_on) run_background([] { hbc_exec(stock_command(APP_ID), 6000); post_main([] { launch_app("com.webos.app.home"); }); });
		else launch_app("com.webos.app.home");
		return;
	}
	launch_app(t.key);
}

/** For screenshots and tests with no remote, from the launch's (or a relaunch's) parameters: {"open":"settings:3"} opens
 *  Settings in its fourth section, in the list; "settings:3:nav" leaves the focus on the sections; "city" (country, calendars,
 *  feeds) a sheet; "gallery" the wallpaper gallery; "update" what the Update tile does (Settings on the update row, and on to
 *  the download), "update:install" that and Install answered by itself; "keys:R,R,OK,BACK" presses keys of the remote on the screen
 *  that is open (L R U D OK BACK), as the remote would. */
void test_open (const std::string &what) {
	if (what.rfind("keys:", 0) == 0) {
		std::string list = what.substr(5) + ",";
		for (size_t at = 0, comma; (comma = list.find(',', at)) != std::string::npos; at = comma + 1) {
			std::string k = list.substr(at, comma - at);
			SDL_Scancode sc = k == "L" ? SDL_SCANCODE_LEFT : k == "R" ? SDL_SCANCODE_RIGHT : k == "U" ? SDL_SCANCODE_UP : k == "D" ? SDL_SCANCODE_DOWN
				: k == "OK" ? SDL_SCANCODE_RETURN : k == "BACK" ? SDL_SCANCODE_ESCAPE : SDL_SCANCODE_UNKNOWN;
			if (sc == SDL_SCANCODE_UNKNOWN) continue;
			for (Uint32 type : {SDL_KEYDOWN, SDL_KEYUP}) {
				SDL_Event e{};
				e.type = type;
				e.key.keysym.scancode = sc;
				e.key.state = type == SDL_KEYDOWN ? SDL_PRESSED : SDL_RELEASED;
				SDL_PushEvent(&e);
			}
		}
		return;
	}
	if (what.rfind("update", 0) == 0) {
		test_install = what == "update:install";
		open_settings();
		app.settings.focus_option("home", "update", now_ms());
		check_update(AfterCheck::go_on);
	} else
	if (what.rfind("settings", 0) == 0) {
		open_settings();
		int tab = what.size() > 9 ? atoi(what.c_str() + 9) : 0;
		double t = now_ms();
		for (int i = 0; i < tab; i++) app.settings.key_down(SDL_SCANCODE_DOWN, false, t);
		if (what.find(":nav") == std::string::npos) app.settings.key_down(SDL_SCANCODE_RIGHT, false, t);
	} else if (what == "gallery") {
		open_settings();
		change_option("wallpaper", Press::ok);
	} else {
		open_settings();
		open_picker(what == "country" ? Picker::Kind::country : what == "calendars" ? Picker::Kind::calendars : what == "feeds" ? Picker::Kind::feeds : Picker::Kind::city);
	}
}

void lifecycle (const std::string &msg) {
	json j = json::parse(msg, nullptr, false);
	// lifecycle v2 names the event in "event"; v1 (the registration as LG Home, register_as_home) in "message"
	std::string event = j.is_object() ? j.value("event", j.value("message", std::string())) : "";
	logf("lifecycle: %s", msg.substr(0, 160).c_str());
	// Running as the TV's Home, the TV relaunches it now and then with {"keepAlive": true} only to keep it loaded: that is not the
	// Home key, so whatever is open (Settings) stays open
	bool keep_alive = j.is_object() && j["parameters"].is_object() && j["parameters"].value("keepAlive", false) == true;
	if (event == "relaunch" && keep_alive) { logf("relaunch to keep it loaded"); return; }
	if (event == "relaunch") {
		// Opened again (its tile, or Home): the window comes to the front, as SDL itself does when it registers the app (we do
		// that ourselves, to get the parameters), and a reading that got old is fetched again
		SDL_ShowWindow(app.win);
		SDL_RaiseWindow(app.win);
		// keys for a test go to the screen that is open
		bool keys = j["parameters"].is_object() && j["parameters"].value("open", std::string()).rfind("keys:", 0) == 0;
		if (!keys) close_screens();
		app.home.wake();
		app.weather_next = std::min(app.weather_next, now_ms());
		app.cal_next = std::min(app.cal_next, now_ms());
		app.news_next = std::min(app.news_next, now_ms());
		app.dirty = true;
		if (j["parameters"].is_object() && j["parameters"].contains("open") && j["parameters"]["open"].is_string()) test_open(j["parameters"]["open"].get<std::string>());
	} else if (event == "close") app.running = false;
}

}  // namespace

namespace {
/** Something on the screen in front moves (the home screen under Settings stays still). */
bool screen_moving (double now) {
	if (app.menu.open() && app.menu.animating(now)) return true;      // over any screen (Restart the TV is asked over Settings)
	if (app.screen == App::Screen::settings) return app.settings.animating(now);
	if (app.screen == App::Screen::picker) return app.picker.animating(now);
	if (app.screen == App::Screen::gallery) return app.gallery.animating(now);
	if (app.screen == App::Screen::home_method) return app.home_method.animating(now);
	return app.home.animating(now) || (app.menu.open() && app.menu.animating(now)) || app.quick.animating(now);
}
}  // namespace

// As LG Home (the Swap) the app manager wants the registration of a native Home of lifecycle v1, and only from LG Home's id,
// which is not this program's on the bus (its own id, its own permissions: the executable is a link to the installed one). As
// root, luna-send may act for an app (-a), so a luna-send kept running holds the registration and hands on its messages (as
// ecklf/lg-luma does on webOS 10/11). Without it Home still opens the launcher, but the app manager starts it afresh at each
// Home press. The helper ends with this process (PR_SET_PDEATHSIG); losing the registration later ends the launcher too, so
// that the app manager starts Home again.
FILE *home_reg = nullptr;           // the registration's messages (register_as_home)
pid_t home_reg_pid = -1;
bool home_reg_seen = false;         // a message of it has come

/** Starts the registration (the luna-send that holds it); its messages are read from home_reg. */
bool start_home_registration () {
	int out[2];
	if (pipe(out) != 0) { logf("home registration: no pipe"); return false; }
	pid_t parent = getpid();
	pid_t pid = fork();
	if (pid < 0) { logf("home registration: no fork"); close(out[0]); close(out[1]); return false; }
	if (pid == 0) {
		prctl(PR_SET_PDEATHSIG, SIGKILL);
		if (getppid() != parent) _exit(1);
		dup2(out[1], 1);
		close(out[0]);
		close(out[1]);
		execl("/usr/bin/luna-send", "luna-send", "-a", "com.webos.app.home", "-i", "luna://com.webos.applicationManager/registerNativeApp", "{}", (char *)nullptr);
		_exit(127);
	}
	close(out[1]);
	logf("home registration: luna-send %d", (int)pid);
	home_reg = fdopen(out[0], "r");
	home_reg_pid = pid;
	if (!home_reg) { close(out[0]); kill(pid, SIGKILL); waitpid(pid, nullptr, 0); return false; }
	return true;
}

/** One message of the registration ("" when it ended or was refused). */
std::string home_reg_message () {
	char line[4096];
	while (home_reg && fgets(line, sizeof line, home_reg)) {
		std::string m(line);
		size_t b = m.find('{');
		if (b == std::string::npos) continue;
		m = m.substr(b);
		json j = json::parse(m, nullptr, false);
		if (!j.is_object()) continue;
		if (j.value("returnValue", true) == false) { logf("home registration refused: %s", m.substr(0, 160).c_str()); return ""; }
		home_reg_seen = true;
		return m;
	}
	return "";
}

/** Started in the background as Home (preload): registered, and nothing shown until Home is really asked for (a relaunch that
 *  is not only {"keepAlive": true}). False when the registration ended first (the app manager closed it). */
bool wait_as_preloaded_home () {
	if (!start_home_registration()) return false;
	for (std::string m; !(m = home_reg_message()).empty(); ) {
		logf("lifecycle (preloaded): %s", m.substr(0, 160).c_str());
		json j = json::parse(m, nullptr, false);
		std::string event = j.value("event", j.value("message", std::string()));
		bool keep_alive = j["parameters"].is_object() && j["parameters"].value("keepAlive", false) == true;
		if (event == "relaunch" && !keep_alive) return true;
		if (event == "close") break;
	}
	logf("home registration ended while preloaded");
	return false;
}

void register_as_home () {
	if (!home_reg && !start_home_registration()) return;
	std::thread([f = home_reg, pid = home_reg_pid] {
		// registered once a message has come (a preloaded Home has had its first ones already)
		bool registered = home_reg_seen;
		for (std::string m; !(m = home_reg_message()).empty(); ) {
			registered = true;
			post_main([m] { lifecycle(m); });
		}
		fclose(f);
		kill(pid, SIGKILL);
		waitpid(pid, nullptr, 0);
		logf("home registration ended (was registered: %s)", registered ? "yes" : "no");
		if (registered) post_main([] { app.running = false; });     // lost: the app manager starts Home again
	}).detach();
}

int main (int argc, char **argv) {
	// as LG Home it runs as root without the right to write the file of the ordinary start (another owner): a file of its own
	// (the native version in development beside the web one: logs of its own name)
	std::string log_name = std::string("/tmp/") + (std::string(APP_ID) == "tld.my.simplelauncher" ? "simplelauncher" : "simplelauncher-native");
	log_open((log_name + (argc > 1 && strstr(argv[1], "\":\"com.webos.app.home\"") ? "-home.log" : ".log")).c_str());
	logf("start, params %s", argc > 1 ? argv[1] : "-");
	// the package's folder: where the program really is (as LG Home it is started from / through a link to it), else the
	// folder it was started in
	char cwd[512] = ".";
	char exe[512];
	ssize_t n = readlink("/proc/self/exe", exe, sizeof exe - 1);
	if (n > 0) {
		exe[n] = 0;
		char *slash = strrchr(exe, '/');
		if (slash && slash != exe) { *slash = 0; snprintf(cwd, sizeof cwd, "%s", exe); }
	} else if (!getcwd(cwd, sizeof cwd)) snprintf(cwd, sizeof cwd, ".");
	set_package_dir(cwd);
	started_as = program_file();
	if (argc > 1) {
		// lifecycle v2 names the app in "appId"; v1 (the Swap's manifest) in "nid"
		json start = json::parse(argv[1], nullptr, false);
		for (const char *k : {"appId", "nid"})
			if (start.is_object() && start.contains(k) && start[k].is_string() && !start[k].get<std::string>().empty()) { RUN_ID = start[k].get<std::string>(); break; }
	}
	if (RUN_ID != APP_ID) logf("running as %s (installed as %s), from %s", RUN_ID.c_str(), APP_ID, cwd);
	AS_HOME = RUN_ID == "com.webos.app.home";
	// A start in the background ({"preload":"full"}): the TV's preload manager starts Home about 80 s into a full start, so
	// that LG Home is ready. LG Home stays out of sight; the launcher made a window and came in front of Live TV or an HDMI
	// input (the Swap, seen on webOS 10.3.2; issue 16). Nothing is shown: it ends here, and the Home key starts it as usual.
	// A start in the background ({"preload":"full"}) as Home: the TV's preload manager starts Home about 80 s into a full start,
	// so that LG Home is ready. LG Home stays out of sight; the launcher made its window and came in front of Live TV or an HDMI
	// input (the Swap, seen on webOS 10.3.2; issue 16). It registers as Home and waits, with no window, until Home is asked for.
	// (Ending at once instead made the preload manager start it again every 7 s.)
	if (AS_HOME && argc > 1 && strstr(argv[1], "\"preload\":")) {
		logf("started in the background (preload): no window until Home is asked for");
		if (!wait_as_preloaded_home()) return 0;
		logf("Home asked for: starting");
	}
	if (AS_HOME) {
		// SDL-webOS names its window from APPID (and WMCLASS): the compositor shows it as Home's. Before SDL and any thread.
		setenv("APPID", RUN_ID.c_str(), 1);
		setenv("SDL_VIDEO_WAYLAND_WMCLASS", RUN_ID.c_str(), 1);
		// the app manager may not relaunch a Home that is in front already: the Home key is ours then
		SDL_SetHint(SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_HOME, "true");
	}
	make_dirs(DATA_DIR);
	icons::set_cache_dir(join_path(DATA_DIR, "icons"));

	// Back is ours (it goes to the first tile); we register with the app manager ourselves, to get Home's relaunch with its params
	SDL_SetHint(SDL_HINT_WEBOS_ACCESS_POLICY_KEYS_BACK, "true");
	SDL_SetHint(SDL_HINT_WEBOS_REGISTER_APP, "false");
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) { logf("SDL_Init: %s", SDL_GetError()); return 1; }
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	app.win = SDL_CreateWindow("Simple launcher", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1920, 1080, SDL_WINDOW_FULLSCREEN | SDL_WINDOW_OPENGL);
	if (!app.win) { logf("window: %s", SDL_GetError()); return 1; }
	SDL_GLContext gl = SDL_GL_CreateContext(app.win);
	if (!gl) { logf("GL context: %s", SDL_GetError()); return 1; }
	SDL_GL_SetSwapInterval(1);
	int dw = 1920, dh = 1080, ww = 1920, wh = 1080;
	SDL_GL_GetDrawableSize(app.win, &dw, &dh);
	SDL_GetWindowSize(app.win, &ww, &wh);
	logf("window %dx%d, drawable %dx%d", ww, wh, dw, dh);
	if (!gfx::init(dw, dh)) return 1;
	SDL_StopTextInput();           // SDL starts with text input on, which would take the first OK
	text::init(join_path(cwd, "fonts"));
	// {"hinting":"light"} and the others: for comparing how the letters are fitted to the pixels on the TV
	if (argc > 1 && strstr(argv[1], "\"hinting\":\"")) {
		std::string mode(strstr(argv[1], "\"hinting\":\"") + 11);
		text::set_hinting(mode.substr(0, mode.find('"')));
	}

	apply_tv_zone();         // before any other thread starts (setenv is not thread-safe)
	unsigned wake = SDL_RegisterEvents(1);
	tasks_start(2, wake);
	if (AS_HOME) register_as_home();
	else luna_subscribe("luna://com.webos.applicationManager/registerApp", json({{"id", RUN_ID}}).dump(), lifecycle);

	http_init();
	// the cards are kept as a picture: new glass (Frost changed) draws them again, or they would show the step before
	app.wallpaper.on_glass = [] { app.home.mark_board(); app.dirty = true; };
	// the collection is set up before the settings are read: they say which of its pictures to download (what a restart left)
	app.collection.set_data_dir(DATA_DIR);
	// a picture of the collection came: the wallpaper takes it in (a slideshow grows as they come)
	app.collection.on_arrived = [] (const std::string &) { app.wallpaper.files_changed(); app.dirty = true; };
	app.collection.on_no_room = [] {
		app.toast.show(tr("Not enough room on the TV for the wallpaper collection", "На ТВ не хватает места для обоев из коллекции"), true, now_ms());
		app.dirty = true;
	};
	load_config();
	forget_splash();
	load_weather();
	load_calendar();
	load_news_and_quote();
	app.home.on_activate = on_activate;
	setup_settings();
	setup_gallery_and_menu();
	setup_home_method();
	setup_quick();
	{
		std::string raw;
		json info = read_file(join_path(cwd, "appinfo.json"), raw) ? json::parse(raw, nullptr, false) : json();
		app.version = info.is_object() ? info.value("version", std::string()) : "";
		// the first look for a new version a little after the start, when the network is up
		if (app.cfg.checkUpdates) app.update_next = now_ms() + 20000;
	}
	app.home.set_wallpaper(&app.wallpaper);
	app.apps = cached_apps(join_path(DATA_DIR, "apps.json"));
	rebuild_items();
	ask_tv();
	refresh_apps();
	// While the official way is on, its copy on the TV is brought up to this version's and LG's "start with Home" kept off
	// (tools/defaulthome.sh refresh): once at each start, a few seconds in, so an update needs nothing more
	run_background([] {
		std::this_thread::sleep_for(std::chrono::seconds(3));
		bool ok = false;
		// first what the web version (1.x) left on the TV, after the update from it (tools/migrate.sh; nothing once it is done)
		std::string moved = hbc_exec(migrate_command(APP_ID), 60000, &ok);
		while (!moved.empty() && isspace((unsigned char)moved.back())) moved.pop_back();
		if (ok && !moved.empty()) logf("from the web version: %s", moved.c_str());
		std::string out = hbc_exec(refresh_command(APP_ID), 15000, &ok);
		while (!out.empty() && isspace((unsigned char)out.back())) out.pop_back();
		logf("defaulthome refresh: %s", ok ? out.c_str() : "the Homebrew Channel did not answer");
		bool on = ok && out.size() >= 2 && out.compare(out.size() - 2, 2, "ok") == 0;
		post_main([on] { app.default_home = on; });
		// the Swap's copy too, while it is on (takeover.sh refresh)
		std::string t = hbc_exec(takeover_refresh_command(APP_ID), 15000, &ok);
		while (!t.empty() && isspace((unsigned char)t.back())) t.pop_back();
		logf("takeover refresh: %s", ok ? t.c_str() : "the Homebrew Channel did not answer");
		// and the helper's, while it is on (helper.sh refresh: "off" when it is not)
		std::string h = hbc_exec(helper_refresh_command(APP_ID), 20000, &ok);
		while (!h.empty() && isspace((unsigned char)h.back())) h.pop_back();
		logf("helper refresh: %s", ok ? h.c_str() : "the Homebrew Channel did not answer");
		bool helper = ok && h.size() >= 2 && h.compare(h.size() - 2, 2, "ok") == 0;
		post_main([helper] { app.helper_on = helper; if (helper) mark_alive(); if ((helper || AS_HOME) && app.front) app.name_at = now_ms(); });
	});
	// When the last app closes, the TV opens its "last input" through LG Home's own handler (last_input_handler), which opens
	// LG Home whatever the Home app is (seen on webOS 10.3.2 after closing YouTube). While the official way is on, LG Home
	// coming in front just after another app, and not opened from the launcher's tile, brings the launcher back in front.
	luna_subscribe("luna://com.webos.applicationManager/getForegroundAppInfo", "{\"subscribe\":true}", [] (const std::string &r) {
		json j = json::parse(r, nullptr, false);
		if (!j.is_object() || !j.contains("appId") || !j["appId"].is_string()) return;
		std::string id = j["appId"].get<std::string>();
		std::string before = app.front_app;
		if (!id.empty()) app.front_app = id;
		if (id != "com.webos.app.home" || !app.default_home) return;
		bool ours = app.lg_home_by_us >= 0 && now_ms() - app.lg_home_by_us < 10000;
		bool after_app = !before.empty() && before != RUN_ID && before != "com.webos.app.home";
		logf("front: LG Home after %s%s", before.empty() ? "nothing" : before.c_str(), ours ? " (opened from the tile)" : "");
		if (!ours && after_app) launch_app(RUN_ID);
	});

	if (argc > 1 && strstr(argv[1], "\"open\":\"")) {
		std::string what(strstr(argv[1], "\"open\":\"") + 8);
		test_open(what.substr(0, what.find('"')));
	}

	// For measuring, with no remote: {"demo":"row"} walks right along the row and back, a step every 150 ms; {"demo":"cal"} goes
	// up onto the weather, across to the calendar, up to its head and into the month, back to the day and the row, 900 ms a key.
	bool demo_cal = argc > 1 && strstr(argv[1], "\"demo\":\"cal\"");
	bool demo = demo_cal || (argc > 1 && strstr(argv[1], "\"demo\":\"row\""));
	std::vector<int> demo_keys;
	if (demo_cal) {
		demo_keys = {SDL_SCANCODE_UP};
		for (int i = 0; i < 7; i++) demo_keys.push_back(SDL_SCANCODE_RIGHT);
		for (int k : {SDL_SCANCODE_UP, SDL_SCANCODE_UP, SDL_SCANCODE_RIGHT, SDL_SCANCODE_LEFT, SDL_SCANCODE_DOWN, SDL_SCANCODE_DOWN, SDL_SCANCODE_WEBOS_BACK}) demo_keys.push_back(k);
	} else
		for (int i = 0; i < 30; i++) demo_keys.push_back(i < 15 ? SDL_SCANCODE_RIGHT : SDL_SCANCODE_LEFT);
	const double demo_gap = demo_cal ? 900 : 150;
	size_t demo_step = 0;
	double demo_next = now_ms() + 3000;
	double first_frame = -1;
	// the frame after a movement ends is drawn too, before any sleep: the last one drawn while it moved is a step short of its end
	// (Settings back from its peek kept the home screen under it until the next key)
	bool was_moving = false;
	while (app.running) {
		double now = now_ms();
		bool moving = screen_moving(now) | (app.wallpaper.update(now) && app.screen == App::Screen::home);
		std::time_t tt = std::time(nullptr);
		std::tm lt;
		localtime_r(&tt, &lt);
		if (lt.tm_min != app.last_minute) { app.last_minute = lt.tm_min; app.dirty = true; app.home.mark_board(); apply_dim(); app.collection.tick(); }
		weather_tick(now);
		calendar_tick(now);
		news_tick(now);
		quote_tick(now);
		if (app.home.tick(now)) app.dirty = true;
		if (app.screen == App::Screen::settings && app.settings.tick(now)) app.dirty = true;
		if (app.toast.tick(now)) app.dirty = true;
		if (app.update_next >= 0 && now >= app.update_next && !app.update_busy) check_update();
		if (app.name_at >= 0 && now >= app.name_at) {
			// the Swap or the Helper on, and the launcher in front: Home is the TV's last input now, so a full start opens Home, as it
			// opens an input that was the last thing on the screen (tools/lastinput.sh). Home and not the launcher's own id: started
			// as the last input at a full start, the native launcher never ran and the TV kept a black screen with it "in front",
			// every later launch waiting behind it (seen on webOS 10.3.2). Home is the launcher with the Swap, and LG Home that the
			// helper replaces at once with the Helper.
			app.name_at = -1;
			if (app.front && (AS_HOME || app.helper_on)) run_background([] { hbc_exec(name_last_input_command(APP_ID, "com.webos.app.home"), 10000); });
		}

		// sleep until an event, the next minute or the next slideshow step; while something moves, no sleep (vsync paces it)
		SDL_Event ev;
		int timeout = 0;
		if (!moving && !app.dirty && !was_moving) {
			double until = (60 - lt.tm_sec) * 1000.0;
			double wall = app.wallpaper.next_event(now);
			if (wall >= 0) until = std::min(until, wall + 1);
			if (app.cfg.weather.on && !app.weather_busy) until = std::min(until, std::max(1.0, app.weather_next - now));
			if (app.cfg.calendar.on && !app.cal_busy) until = std::min(until, std::max(1.0, app.cal_next - now));
			if (app.cfg.news.on && !app.news_busy) until = std::min(until, std::max(1.0, app.news_next - now));
			if (app.cfg.quote.on && !app.quote_busy) until = std::min(until, std::max(1.0, app.quote_next - now));
			if (app.update_next >= 0 && !app.update_busy) until = std::min(until, std::max(1.0, app.update_next - now));
			double page = app.home.next_event(now);
			if (page >= 0) until = std::min(until, page + 1);
			for (double t : {app.toast.next_event(now), app.screen == App::Screen::settings ? app.settings.next_event(now) : -1.0, app.name_at >= 0 ? std::max(0.0, app.name_at - now) : -1.0})
				if (t >= 0) until = std::min(until, t + 1);
			if (demo && demo_step < demo_keys.size()) until = std::min(until, std::max(1.0, demo_next - now));
			timeout = (int)std::max(1.0, std::min(until, 60000.0));
		}
		bool got = timeout > 0 ? SDL_WaitEventTimeout(&ev, timeout) : SDL_PollEvent(&ev);
		while (got) {
			now = now_ms();
			float sx = 1920.f / ww, sy = 1080.f / wh;
			switch (ev.type) {
				case SDL_QUIT: app.running = false; break;
				case SDL_KEYDOWN:
					if (AS_HOME && ev.key.keysym.scancode == SDL_SCANCODE_WEBOS_HOME) {
						// the Home key while the launcher is Home and in front: back to the home screen
						if (!ev.key.repeat) { app.menu.close(); app.quick.close(now); close_screens(); app.home.wake(); }
						app.dirty = true;
						break;
					}
					if (app.menu.open()) app.menu.key_down(ev.key.keysym.scancode, ev.key.repeat != 0, now);      // over any screen
					else if (app.screen == App::Screen::home_method) app.home_method.key_down(ev.key.keysym.scancode, now);
					else if (app.quick.is_open()) app.quick.key_down(ev.key.keysym.scancode, ev.key.repeat != 0, now);
					else if (app.screen == App::Screen::gallery) app.gallery.key_down(ev.key.keysym.scancode, now);
					else if (app.screen == App::Screen::settings) app.settings.key_down(ev.key.keysym.scancode, ev.key.repeat != 0, now);
					else if (app.screen == App::Screen::picker) app.picker.key_down(ev.key.keysym.scancode, ev.key.repeat != 0, now);
					else app.home.key(ev.key.keysym.scancode, ev.key.repeat != 0, now);
					app.dirty = true;
					break;
				case SDL_KEYUP:
					if (app.menu.open()) { if (app.menu.key_up(ev.key.keysym.scancode, now)) app.dirty = true; }
					else if (app.quick.is_open()) { if (app.quick.key_up(ev.key.keysym.scancode, now)) app.dirty = true; }
					else if (app.screen == App::Screen::settings) { if (app.settings.key_up(ev.key.keysym.scancode, now)) app.dirty = true; }
					else if (app.screen == App::Screen::home && app.home.key_up(ev.key.keysym.scancode, now)) app.dirty = true;
					break;
				case SDL_TEXTINPUT:
					if (app.screen == App::Screen::picker) { app.picker.text_input(ev.text.text); app.dirty = true; }
					break;
				case SDL_TEXTEDITING:
					logf("text editing: \"%s\"", ev.edit.text);
					break;
				case SDL_MOUSEMOTION: {
					float x = ev.motion.x * sx, y = ev.motion.y * sy;
					if (app.menu.open()) app.menu.pointer(x, y, now);
					else if (app.screen == App::Screen::settings) app.settings.pointer(x, y, now);
					else if (app.screen == App::Screen::home_method) app.home_method.pointer(x, y, now);
					else if (app.screen == App::Screen::home && !app.quick.is_open()) app.home.pointer(x, y, now);
					app.dirty = true;
					break;
				}
				case SDL_MOUSEBUTTONDOWN: {
					if (ev.button.button != SDL_BUTTON_LEFT) break;
					float x = ev.button.x * sx, y = ev.button.y * sy;
					if (app.menu.open()) app.menu.click(x, y, now);
					else if (app.screen == App::Screen::settings) app.settings.click(x, y, now);
					else if (app.screen == App::Screen::home_method) app.home_method.click(x, y, now);
					else if (app.screen == App::Screen::home && !app.quick.is_open()) app.home.click(x, y, now);
					app.dirty = true;
					break;
				}
				case SDL_MOUSEWHEEL:
					if (app.menu.open()) break;
					if (app.screen == App::Screen::settings) app.settings.wheel(ev.wheel.y, now);
					else if (app.screen == App::Screen::home) app.home.wheel(ev.wheel.y, now);
					app.dirty = true;
					break;
				case SDL_WINDOWEVENT:
					if (ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED || ev.window.event == SDL_WINDOWEVENT_SHOWN) {
						if (!app.front) { mark_alive(); if (AS_HOME || app.helper_on) app.name_at = now + 3000; }
						if (!app.front && updated_on_disk()) {
							logf("an update is installed: starting again as the new version");
							hbc_exec("( setsid nohup sh -c 'sleep 1; luna-send -n 1 luna://com.webos.applicationManager/launch \"{\\\"id\\\":\\\"" + RUN_ID + "\\\"}\"' </dev/null >/dev/null 2>&1 & ); echo ok", 5000);
							app.running = false;
						}
						if (!app.front) refresh_inputs();
						app.front = true;
						app.home.set_front(true);
						app.wallpaper.set_paused(false, now);
					} else if (ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST || ev.window.event == SDL_WINDOWEVENT_HIDDEN) {
						app.front = false;
						app.home.set_front(false);
						app.wallpaper.set_paused(true, now);
						app.home.pointer_left();
						if (ev.window.event == SDL_WINDOWEVENT_HIDDEN) close_screens();
					}
					app.dirty = true;
					break;
				default: break;
			}
			got = SDL_PollEvent(&ev);
		}
		if (demo && now_ms() >= demo_next && demo_step < demo_keys.size()) {
			app.home.key(demo_keys[demo_step++], false, now_ms());
			demo_next = now_ms() + demo_gap;
			app.dirty = true;
		}
		if (run_main_posted()) app.dirty = true;
		// an icon or glyph arrived: the board (kept as a picture) is drawn again too, or what is on the cards (the calendar's
		// arrows) would show only at its next change
		if (icons::upload_ready()) { app.dirty = true; app.home.mark_board(); }
		now = now_ms();
		moving = screen_moving(now) | (app.wallpaper.update(now) && app.screen == App::Screen::home);
		bool ended = was_moving && !moving;
		was_moving = moving;
		if (!app.dirty && !moving && !ended) continue;
		app.dirty = false;

		// The wallpaper and the board go into a picture of their own, made again only when something on them changed or moves;
		// a frame is then that picture, the app row and what is over them. When the wallpaper changes, the board is drawn once for
		// the new picture and the two pictures cross-fade (1.4 s, as the wallpaper's own fade), instead of drawing it all every frame.
		double t0 = now_ms();
		static std::unique_ptr<Target> board, old_board;
		static bool have_board = false;
		static double changed_at = -1e9, seen_shown = -1;
		const double FADE_MS = 1400;
		if (!board) { board = gfx::make_target(); old_board = gfx::make_target(); }
		bool targets = board && old_board;
		app.wallpaper.set_settled(targets);
		if (targets && app.wallpaper.shown_at() != seen_shown) {
			seen_shown = app.wallpaper.shown_at();
			if (have_board) { std::swap(board, old_board); changed_at = now; }
			have_board = false;
		}
		// Settings and the screens opened from it cover the whole screen, 90 % opaque (SCREEN_OPACITY), over the wallpaper alone;
		// the home screen is drawn only while Settings steps aside to show a change on it (its peek)
		float peek = app.screen == App::Screen::settings ? app.settings.peek(now) : 0;
		bool on_home = app.screen == App::Screen::home || peek > 0;
		if (on_home && targets && (!have_board || app.home.board_needed(now))) {
			gfx::begin_frame(theme_named(app.cfg.theme).bg, board.get());
			app.wallpaper.draw(now);
			app.home.draw_board(now, lt);
			have_board = true;
		}
		gfx::begin_frame(theme_named(app.cfg.theme).bg);
		double f = (now - changed_at) / FADE_MS;
		if (!on_home) app.wallpaper.draw(now);
		else {
			if (!targets) { app.wallpaper.draw(now); app.home.draw_board(now, lt); }
			else if (f < 1) {
				gfx::image(*old_board->tex, {0, 0, 1920, 1080});
				gfx::image(*board->tex, {0, 0, 1920, 1080}, (float)ease(Ease::ease_in_out, f));
			} else gfx::image(*board->tex, {0, 0, 1920, 1080});
			if (f < 1) app.dirty = true;
			app.home.draw_live(now, lt);
		}
		double t1 = now_ms();
		if (on_home) {
			app.home.draw_row(now);
			app.home.draw_overlay(now);
		}
		if (app.screen == App::Screen::settings) app.settings.draw(now);      // fades itself, all but the changed row, while it peeks
		else if (app.screen == App::Screen::picker) app.picker.draw(now);
		else if (app.screen == App::Screen::gallery) app.gallery.draw(now);
		else if (app.screen == App::Screen::home_method) app.home_method.draw(now);
		if (app.screen == App::Screen::home) app.quick.draw(app.cfg.theme, now);
		app.menu.draw(app.cfg.theme, now);
		app.toast.draw(app.cfg.theme);
		double t2 = now_ms();
		SDL_GL_SwapWindow(app.win);
		double t3 = now_ms();
		static double last_end = 0;
		if (demo && last_end > 0 && t3 - last_end > 40)
			logf("slow frame: %.0f ms since the last (before drawing %.0f, board+wallpaper %.1f, row %.1f, swap %.1f)", t3 - last_end, t0 - last_end, t1 - t0, t2 - t1, t3 - t2);
		last_end = t3;
		if (demo) {
			// how long a frame takes: the wallpaper, the home screen (CPU and GL calls), the swap (waiting for the GPU and vsync)
			static double sum_wall = 0, sum_home = 0, sum_swap = 0, max_home = 0;
			static int frames = 0;
			sum_wall += t1 - t0; sum_home += t2 - t1; sum_swap += t3 - t2; max_home = std::max(max_home, t2 - t1);
			if (++frames == 60) {
				logf("frame: board %.1f ms, row %.1f ms (max %.1f), swap %.1f ms", sum_wall / 60, sum_home / 60, max_home, sum_swap / 60);
				sum_wall = sum_home = sum_swap = max_home = 0;
				frames = 0;
			}
		}
		text::trim();
		if (first_frame < 0) { first_frame = now; logf("first frame at %.0f ms", now); }
	}
	logf("exit");
	SDL_GL_DeleteContext(gl);
	SDL_DestroyWindow(app.win);
	SDL_Quit();
	// out at once, without waiting for the jobs in the background: one may be waiting up to 90 s for a home-mode switch, whose
	// script restarts SAM, and SAM's restart waits for this process to end (it is in SAM's own cgroup): each held the other up
	_exit(0);
}
