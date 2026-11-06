// The settings screens as data (lib/prefs.ts): every option of every section (Appearance, Widgets, Wallpaper, Home screen), how
// it reads the config, and how OK / ← → change it. The Settings lists are built from this table and a change is made with apply_pref.
#pragma once

#include "data/config.h"
#include "data/update.h"

#include <functional>
#include <string>
#include <vector>

/** Facts about the TV (not user settings) that some rows display. */
struct SystemInfo {
	std::string home_mode;       // how the launcher replaces LG Home (default mount helper none); empty while unknown
	std::string version;         // the launcher's version (appinfo.json)
	std::string tv_tag;          // the TV's language tag ("ru-RU"), for the news country
	UpdateState update;          // a new version of the launcher (data/update.h)
	int root = 0;                // can the TV run commands as root (Root in data/homemode.h: 0 unknown, 1 yes, 2 no)
	std::string root_detail;     // what it said when it could not
};

/** How a key press changes a setting: OK steps and wraps round; ← → nudge and stop at the ends. */
enum class Press { ok, left, right };

struct Pref {
	std::string id, tab, label, description;
	std::string kind;            // toggle choice action info
	std::string group;           // the heading of its group (rows of one group follow each other)
	bool opens = false;          // an action that opens another screen
	std::string icon;            // a glyph at the row's end, in the colour of its text (Restart the TV: "restart")
	std::function<bool(const UserConfig &)> when;        // shown only when this says so
	std::function<bool(const UserConfig &, const SystemInfo &)> shown;     // the same, from the TV's side
	std::function<std::string(const SystemInfo &)> label_of;     // a label that changes (the update row)
	std::function<float(const SystemInfo &)> progress_of;        // a bar under the label, 0-1 (-1: none)
	std::function<bool(const UserConfig &)> on;          // toggle: is it on
	std::function<std::string(const UserConfig &, const SystemInfo &)> text;    // choice / action: the value as text
	std::function<UserConfig(const UserConfig &, Press)> change;
};

/** A row of a Settings list (components/OptionList.tsx OptionItem). */
struct OptionItem {
	std::string id, label, description, group, kind, icon;
	bool opens = false;
	int value = -1;              // toggles: 0 / 1; -1 for the others
	std::string valueText;
	bool stepper = false;        // ← → change it
	float progress = -1;         // a bar under the label, 0-1 (-1: none): a download
	std::string disabled;        // greyed out: why
};

template <class T>
T step_value (const std::vector<T> &values, const T &current, int delta, bool wrap) {
	int n = (int)values.size(), from = 0;
	for (int i = 0; i < n; i++) if (values[i] == current) { from = i; break; }
	int next = wrap ? ((from + delta) % n + n) % n : std::min(n - 1, std::max(0, from + delta));
	return values[next];
}

extern const std::vector<int> INTERVAL_VALUES;
std::string interval_text (int seconds);
std::string font_label (const std::string &id);
std::string color_label (const std::string &id);
extern const std::vector<std::string> CLOCK_FONT_IDS, CLOCK_COLOR_IDS;

/** The look, margins and clock back to their defaults (apps, sources and wallpaper are left alone). */
UserConfig reset_look (const UserConfig &c);
/** Every row, worded in the current language. */
const std::vector<Pref> &prefs ();
const Pref *find_pref (const std::string &id);
/** The Settings list for one section. */
std::vector<OptionItem> options_for (const std::string &tab, const UserConfig &c, const SystemInfo &info);
/** The config after pressing OK / ← / → on option `id`; `changed` false when nothing changes. */
UserConfig apply_pref (const UserConfig &c, const std::string &id, Press press, bool *changed = nullptr);
/** Two configs the same, as saved. */
bool same_config (const UserConfig &a, const UserConfig &b);
