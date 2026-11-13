#include "data/inputs.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

const char *CEC_LIST_URI = "luna://com.webos.service.tv.externaldevice/getExternalInputStatusList";

std::map<int, std::string> device_names (const std::string &reply) {
	std::map<int, std::string> out;
	// luna-send prints the reply; anything before its first brace is not part of it
	size_t brace = reply.find('{');
	if (brace == std::string::npos) return out;
	json j = json::parse(reply.substr(brace), nullptr, false);
	if (!j.is_object() || !j.contains("cec") || !j["cec"].is_object()) return out;
	const json &cec = j["cec"];
	if (!cec.contains("cecDeviceList") || !cec["cecDeviceList"].is_array()) return out;
	for (auto &d : cec["cecDeviceList"]) {
		if (!d.is_object() || !d.contains("osdName") || !d["osdName"].is_string()) continue;
		std::string name = d["osdName"].get<std::string>();
		while (!name.empty() && isspace((unsigned char)name.front())) name.erase(name.begin());
		while (!name.empty() && isspace((unsigned char)name.back())) name.pop_back();
		int port = 0;
		if (d.contains("portId") && d["portId"].is_number_integer() && d["portId"].get<int>() > 0) port = d["portId"].get<int>();
		else if (d.contains("physicalAddr") && d["physicalAddr"].is_number_integer() && d["physicalAddr"].get<int>() > 0) port = (d["physicalAddr"].get<int>() >> 12) & 0xf;
		if (name.empty() || port <= 0 || out.count(port)) continue;
		out[port] = name;
	}
	return out;
}
