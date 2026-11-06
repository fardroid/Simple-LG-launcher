#include "data/quick.h"

#include "i18n/i18n.h"

#include <nlohmann/json.hpp>

#include <algorithm>

using json = nlohmann::json;

const std::vector<QuickSetting> &quick_settings () {
	static const std::vector<QuickSetting> list = {
		{"picture", "picture", "pictureMode", {"personalized", "vivid", "normal", "eco", "cinema", "sports", "game", "filmMaker", "expert1", "expert2"}},
		{"sound", "sound", "soundMode", {"personalized", "aiSoundPlus", "standard", "movie", "news", "sports", "music", "game"}},
		{"energy", "picture", "energySaving", {"auto", "off", "min", "med", "max"}},
		{"screenoff", "", "", {}},
		{"tvsettings", "", "", {}},
	};
	return list;
}

const QuickSetting *find_quick (const std::string &id) {
	for (auto &s : quick_settings()) if (s.id == id) return &s;
	return nullptr;
}

std::string quick_label (const std::string &id) {
	if (id == "picture") return tr("Picture mode", "Режим изображения");
	if (id == "sound") return tr("Sound mode", "Режим звука");
	if (id == "energy") return tr("Energy saving", "Энергосбережение");
	if (id == "screenoff") return tr("Screen off", "Выключить экран");
	return tr("All TV settings", "Все настройки ТВ");
}

std::string quick_value_name (const std::string &id, const std::string &v) {
	if (v == "personalized") return tr("Personal", "Личный");
	if (id == "picture") {
		if (v == "vivid") return tr("Vivid", "Яркий");
		if (v == "normal") return tr("Standard", "Стандартный");
		if (v == "eco") return tr("Eco", "Эко");
		if (v == "cinema") return tr("Cinema", "Кино");
		if (v == "sports") return tr("Sports", "Спорт");
		if (v == "game") return tr("Game", "Игры");
		if (v == "filmMaker") return "Filmmaker";
		if (v == "expert1") return tr("Expert (bright room)", "Эксперт (светлая комната)");
		if (v == "expert2") return tr("Expert (dark room)", "Эксперт (тёмная комната)");
	} else if (id == "sound") {
		if (v == "aiSoundPlus") return "AI Sound Pro";
		if (v == "standard") return tr("Standard", "Стандартный");
		if (v == "movie") return tr("Cinema", "Кино");
		if (v == "news") return tr("News", "Новости");
		if (v == "sports") return tr("Sports", "Спорт");
		if (v == "music") return tr("Music", "Музыка");
		if (v == "game") return tr("Game", "Игры");
	} else if (id == "energy") {
		if (v == "auto") return tr("Auto", "Авто");
		if (v == "off") return tr("Off", "Выкл.");
		if (v == "min") return tr("Minimum", "Минимум");
		if (v == "med") return tr("Medium", "Средний");
		if (v == "max") return tr("Maximum", "Максимум");
		if (v == "screen_off") return tr("Screen off", "Экран выключен");
	}
	return v;
}

std::vector<std::string> next_values (const QuickSetting &s, const std::string &current) {
	return next_values(s.values, current);
}

std::vector<std::string> next_values (const std::vector<std::string> &values, const std::string &current) {
	std::vector<std::string> out;
	int n = (int)values.size();
	auto it = std::find(values.begin(), values.end(), current);
	int from = it == values.end() ? -1 : (int)(it - values.begin());
	for (int k = 1; k <= n; k++) {
		int i = ((from + k) % n + n) % n;
		if (i != from) out.push_back(values[i]);
	}
	return out;
}

std::vector<std::string> values_from_desc (const std::string &reply, const std::string &key) {
	std::vector<std::string> out;
	size_t b = reply.find('{');
	json j = json::parse(b == std::string::npos ? std::string() : reply.substr(b), nullptr, false);
	if (!j.is_object() || !j["results"].is_array()) return out;
	for (auto &r : j["results"]) {
		if (!r.is_object() || r.value("key", "") != key || !r["values"].is_object() || !r["values"]["arrayExt"].is_array()) continue;
		for (auto &v : r["values"]["arrayExt"])
			if (v.is_object() && v.value("visible", false) && v.value("active", false) && v["value"].is_string()) out.push_back(v["value"].get<std::string>());
	}
	return out;
}

std::string setting_from_reply (const std::string &reply, const std::string &key) {
	json j = json::parse(reply, nullptr, false);
	if (!j.is_object() || !j["settings"].is_object()) return "";
	const json &v = j["settings"][key];
	return v.is_string() ? v.get<std::string>() : "";
}

std::string set_command (const std::string &category, const std::string &key, const std::string &value) {
	// the names are ours (plain words), so they go into the shell's quotes as they are
	json body = {{"category", category}, {"settings", {{key, value}}}};
	return "luna-send -n 1 luna://com.webos.settingsservice/setSystemSettings '" + body.dump() + "'";
}

bool reply_ok (const std::string &reply) {
	json j = json::parse(reply.substr(reply.find('{') == std::string::npos ? 0 : reply.find('{')), nullptr, false);
	return j.is_object() && j.value("returnValue", false) == true;
}
