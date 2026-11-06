// Quick settings (Down from the row of apps): the TV's picture mode, sound mode and energy saving changed in one press, the screen
// switched off, and the TV's own menus. The values are the TV's (com.webos.settingsservice): which of them a model has is not
// the same on every model: the TV lists its own (getSystemSettingDesc, those shown in its menu), and a change still goes on past one
// the TV refuses.
#pragma once

#include <optional>
#include <string>
#include <vector>

struct QuickSetting {
	std::string id;          // picture sound energy screenoff tvsettings
	std::string category;    // the TV's settings category ("picture", "sound"); empty: an action
	std::string key;         // its key ("pictureMode")
	std::vector<std::string> values;     // the values tried, in order, when the TV does not list its own
};

const std::vector<QuickSetting> &quick_settings ();
const QuickSetting *find_quick (const std::string &id);
/** The name of a setting's tile, and of one of its values ("cinema" → "Кино"); an unknown value as the TV calls it. */
std::string quick_label (const std::string &id);
std::string quick_value_name (const std::string &id, const std::string &value);
/** The values to try after `current`, in order, wrapping round (all but the current one). */
std::vector<std::string> next_values (const QuickSetting &s, const std::string &current);
std::vector<std::string> next_values (const std::vector<std::string> &values, const std::string &current);
/** The values of `key` the TV's menu shows (visible and active) in a getSystemSettingDesc reply, in its order; none when not there. */
std::vector<std::string> values_from_desc (const std::string &reply, const std::string &key);
/** The value of `key` in a getSystemSettings reply (empty when it is not there). */
std::string setting_from_reply (const std::string &reply, const std::string &key);
/** The luna-send command that sets one value (run through the Homebrew Channel's root shell). */
std::string set_command (const std::string &category, const std::string &key, const std::string &value);
/** Did the TV take it (its reply says "returnValue": true)? */
bool reply_ok (const std::string &reply);
