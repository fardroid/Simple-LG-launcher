// The apps and inputs on the TV (lib/apps.ts, lib/inputs.ts, lib/launch.ts). A native app may ask the app manager for the list
// itself (the web version needed the Homebrew Channel for that), and read the icon files straight from the TV.
#pragma once

#include <functional>
#include <string>
#include <vector>

struct AppEntry {
	std::string id, title;
	std::string iconPath;       // a file on the TV (the app's own icon) or in the package (icons/apps/); empty: a monogram
	std::string color;          // the monogram's background
	int removable = -1;         // LG's "can be removed" flag: 1, 0, or -1 when the TV did not say
};

struct InputSource {
	std::string id;          // HDMI_1
	std::string appId;       // com.webos.app.hdmi1: launching it switches to the input
	std::string label;
	int port = 0;
	bool connected = true;   // something is plugged in (the TV's own flag; true when it does not say)
	std::string device;      // the name the device plugged in announces over HDMI, when the TV could be asked (root)
	std::string icon;        // file name in icons/inputs/
};

/** The live list from the app manager; `done` on the main thread (an empty list when the TV did not answer). */
void list_apps (std::function<void(std::vector<AppEntry>)> done);
/** Live TV (when the TV has a tuner) and the external inputs, by port. */
void list_inputs (const std::string &live_tv_label, const std::string &launcher_label, std::function<void(std::vector<InputSource>)> done);
/** The list as last seen (for an instant first paint), and keeping it. */
std::vector<AppEntry> cached_apps (const std::string &file);
void cache_apps (const std::string &file, const std::vector<AppEntry> &apps);

/** Opens an app or an input; `done(ok)` on the main thread. */
void launch_app (const std::string &id, std::function<void(bool)> done = nullptr);
/** The TV's own Settings app: the first of its ids that exists. */
void launch_system_settings (std::function<void(bool)> done = nullptr);

/** Where the package's own files are (icons/apps, icons/inputs...). */
void set_package_dir (const std::string &dir);
const std::string &package_dir ();
