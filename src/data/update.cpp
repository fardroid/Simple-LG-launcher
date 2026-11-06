#include "data/update.h"

#include <nlohmann/json.hpp>

#include <cstdlib>

using json = nlohmann::json;

const char *UPDATE_FEED = "https://raw.githubusercontent.com/fardroid/Simple-LG-launcher/main/repo/apps.json";

std::optional<UpdateInfo> parse_update_feed (const std::string &json_text, const std::string &app_id, bool local) {
	json j = json::parse(json_text, nullptr, false);
	if (!j.is_object() || !j.contains("packages") || !j["packages"].is_array()) return std::nullopt;
	for (const auto &p : j["packages"]) {
		if (!p.is_object() || !p.contains("manifest") || !p["manifest"].is_object()) continue;
		const json &m = p["manifest"];
		if (m.value("id", std::string()) != app_id) continue;
		UpdateInfo u;
		u.version = m.value("version", std::string());
		u.url = m.value("ipkUrl", std::string());
		if (m.contains("ipkHash") && m["ipkHash"].is_object()) u.sha256 = m["ipkHash"].value("sha256", std::string());
		if (m.contains("ipkSize") && m["ipkSize"].is_number()) u.size = m["ipkSize"].get<long>();
		// a package that cannot be checked is not offered
		bool secure = u.url.rfind("https://", 0) == 0 || (local && u.url.rfind("file:///", 0) == 0);
		if (u.version.empty() || !secure || u.sha256.size() != 64) return std::nullopt;
		return u;
	}
	return std::nullopt;
}

bool version_newer (const std::string &a, const std::string &b) {
	const char *x = a.c_str(), *y = b.c_str();
	while (*x || *y) {
		char *ex, *ey;
		long nx = strtol(x, &ex, 10), ny = strtol(y, &ey, 10);
		if (nx != ny) return nx > ny;
		x = *ex == '.' ? ex + 1 : ex;
		y = *ey == '.' ? ey + 1 : ey;
		// anything that is not a number ends the comparison (2.0.0 and 2.0.0-rc are the same here)
		if ((*x && (*x < '0' || *x > '9')) || (*y && (*y < '0' || *y > '9'))) break;
	}
	return false;
}

std::string update_file (const std::string &data_dir, const std::string &version) {
	std::string safe;
	for (char c : version) if (isalnum((unsigned char)c) || c == '.' || c == '-') safe += c;
	return data_dir + "/update-" + safe + ".ipk";
}
