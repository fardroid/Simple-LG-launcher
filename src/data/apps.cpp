#include "data/apps.h"

#include "core/log.h"
#include "sys/fs.h"
#include "sys/luna.h"

#include <nlohmann/json.hpp>

#include <algorithm>

using json = nlohmann::json;

namespace {

struct CatalogApp { const char *id, *title, *icon, *color; };
#include "data/catalog.inc"

// apps LG lists that make no sense on the launcher (the launcher itself, promotions)
const char *HIDDEN_IDS[] = {"tld.my.simplelauncher", "tld.my.simplelauncher.native", "google.assistant.promo", "com.webos.app.discovery"};
const char *SETTINGS_IDS[] = {"com.palm.app.settings", "com.webos.app.settings"};
const char *LIVE_TV = "com.webos.app.livetv";

std::string pkg_dir = ".";

const CatalogApp *known (const std::string &id) {
	for (const auto &c : CATALOG) if (id == c.id) return &c;
	return nullptr;
}

bool hidden (const std::string &id) {
	for (const char *h : HIDDEN_IDS) if (id == h) return true;
	return false;
}

void app_exists (const std::string &id, std::function<void(bool)> done) {
	luna_call("luna://com.webos.applicationManager/getAppLoadStatus", json({{"appId", id}}).dump(), [done] (const std::string &r) {
		json j = json::parse(r, nullptr, false);
		done(j.is_object() && j.value("exist", false));
	}, 4000);
}

}  // namespace

void set_package_dir (const std::string &dir) { pkg_dir = dir; }
const std::string &package_dir () { return pkg_dir; }

void list_apps (std::function<void(std::vector<AppEntry>)> done) {
	luna_call("luna://com.webos.applicationManager/listLaunchPoints", "{}", [done] (const std::string &r) {
		std::vector<AppEntry> out;
		json j = json::parse(r, nullptr, false);
		if (j.is_object() && j["launchPoints"].is_array()) {
			for (auto &p : j["launchPoints"]) {
				std::string id = p.value("id", std::string()), title = p.value("title", std::string());
				if (id.empty() || title.empty() || hidden(id)) continue;
				AppEntry a;
				a.id = id;
				const CatalogApp *c = known(id);
				// the catalogue's titles are shorter than LG's ("IPTV Player" vs "webOS IPTV Player")
				a.title = c ? c->title : title;
				std::string large = p.value("largeIcon", std::string()), icon = p.value("icon", std::string());
				a.iconPath = !large.empty() ? large : icon;
				if (a.iconPath.empty() && c && *c->icon) a.iconPath = join_path(pkg_dir, std::string("icons/apps/") + c->icon);
				if (c) a.color = c->color;
				a.removable = p.contains("removable") && p["removable"].is_boolean() ? (p["removable"].get<bool>() ? 1 : 0) : -1;
				out.push_back(a);
			}
		} else logf("listLaunchPoints: %s", r.substr(0, 200).c_str());
		done(out);
	}, 10000);
}

void list_inputs (const std::string &live_tv_label, const std::string &launcher_label, std::function<void(std::vector<InputSource>)> done) {
	luna_call("luna://com.webos.service.eim/getAllInputStatus", "{}", [=] (const std::string &r) {
		std::vector<InputSource> external;
		json j = json::parse(r, nullptr, false);
		if (j.is_object() && j["devices"].is_array()) {
			for (auto &d : j["devices"]) {
				std::string app = d.value("appId", std::string()), label = d.value("label", std::string());
				bool self = app == "tld.my.simplelauncher" || app == "tld.my.simplelauncher.native";
				if (app.empty() || (label.empty() && !self)) continue;
				InputSource s;
				s.id = d.value("id", std::string());
				s.appId = app;
				s.label = label.empty() ? launcher_label : label;
				s.port = d.value("port", 0);
				s.connected = d.value("connected", true);
				s.icon = d.value("icon", std::string("hdmigeneric.png"));
				if (s.icon.empty()) s.icon = "hdmigeneric.png";
				external.push_back(s);
			}
			std::stable_sort(external.begin(), external.end(), [] (const InputSource &a, const InputSource &b) { return a.port < b.port; });
		}
		app_exists(LIVE_TV, [=] (bool tuner) {
			std::vector<InputSource> out;
			if (tuner) out.push_back({"LIVE_TV", LIVE_TV, live_tv_label, 0, true, "tv.png"});
			out.insert(out.end(), external.begin(), external.end());
			done(out);
		});
	});
}

std::vector<AppEntry> cached_apps (const std::string &file) {
	std::vector<AppEntry> out;
	std::string raw;
	if (!read_file(file, raw)) return out;
	json j = json::parse(raw, nullptr, false);
	if (!j.is_array()) return out;
	for (auto &a : j) {
		if (!a.is_object()) continue;
		out.push_back({a.value("id", std::string()), a.value("title", std::string()), a.value("icon", std::string()),
			a.value("color", std::string()), a.contains("removable") && a["removable"].is_number_integer() ? a["removable"].get<int>() : -1});
	}
	return out;
}

void cache_apps (const std::string &file, const std::vector<AppEntry> &apps) {
	json j = json::array();
	for (auto &a : apps) j.push_back({{"id", a.id}, {"title", a.title}, {"icon", a.iconPath}, {"color", a.color}, {"removable", a.removable}});
	write_file(file, j.dump());
}

void launch_app (const std::string &id, std::function<void(bool)> done) {
	logf("launch %s", id.c_str());
	luna_call("luna://com.webos.applicationManager/launch", json({{"id", id}, {"params", json::object()}}).dump(), [done, id] (const std::string &r) {
		json j = json::parse(r, nullptr, false);
		bool ok = j.is_object() && j.value("returnValue", false);
		if (!ok) logf("launch %s: %s", id.c_str(), r.substr(0, 200).c_str());
		if (done) done(ok);
	});
}

// the first of the Settings app's ids that exists on this TV; when none can be confirmed, the most likely one anyway
static void try_settings (size_t i, std::function<void(bool)> done) {
	if (i >= sizeof SETTINGS_IDS / sizeof *SETTINGS_IDS) { launch_app(SETTINGS_IDS[0], done); return; }
	app_exists(SETTINGS_IDS[i], [i, done] (bool found) {
		if (found) launch_app(SETTINGS_IDS[i], done);
		else try_settings(i + 1, done);
	});
}

void launch_system_settings (std::function<void(bool)> done) { try_settings(0, done); }
