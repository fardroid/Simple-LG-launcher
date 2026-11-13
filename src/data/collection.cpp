#include "data/collection.h"

#include "sys/fs.h"

#include <nlohmann/json.hpp>

#include <algorithm>

using json = nlohmann::json;

const char *COLLECTION_FEED = "https://raw.githubusercontent.com/fardroid/Simple-LG-launcher/main/repo/wallpapers.json";

const std::vector<std::string> &collection_groups () {
	static const std::vector<std::string> groups = {"mountains", "nature", "space", "abstract"};
	return groups;
}

namespace {

bool plain_id (const std::string &id) {
	if (id.empty() || id.size() > 40) return false;
	for (char c : id)
		if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
	return true;
}

bool secure (const std::string &url, bool local) {
	// a test list may also name files on the TV, or a server on the TV itself
	return url.rfind("https://", 0) == 0 || (local && (url.rfind("file:///", 0) == 0 || url.rfind("http://127.0.0.1:", 0) == 0));
}

std::string text (const json &o, const char *key) {
	auto it = o.find(key);
	return it != o.end() && it->is_string() ? it->get<std::string>() : std::string();
}

long number (const json &o, const char *key) {
	auto it = o.find(key);
	return it != o.end() && it->is_number_integer() ? it->get<long>() : 0;
}

}  // namespace

std::vector<CollectionPhoto> parse_collection (const std::string &json_text, bool local) {
	std::vector<CollectionPhoto> out;
	json j = json::parse(json_text, nullptr, false);
	if (!j.is_object() || !j.contains("photos") || !j["photos"].is_array()) return out;
	const auto &groups = collection_groups();
	std::set<std::string> seen;
	for (const auto &p : j["photos"]) {
		if (!p.is_object()) continue;
		CollectionPhoto c;
		c.id = text(p, "id");
		c.group = text(p, "group");
		c.title = text(p, "title");
		c.author = text(p, "author");
		c.licence = text(p, "licence");
		c.source = text(p, "source");
		c.url = text(p, "url");
		c.sha256 = text(p, "sha256");
		c.thumb = text(p, "thumb");
		c.thumbSha256 = text(p, "thumbSha256");
		c.size = number(p, "size");
		c.thumbSize = number(p, "thumbSize");
		if (!plain_id(c.id) || seen.count(c.id)) continue;
		if (std::find(groups.begin(), groups.end(), c.group) == groups.end()) continue;
		if (!secure(c.url, local) || !secure(c.thumb, local) || c.sha256.size() != 64 || c.thumbSha256.size() != 64) continue;
		if (c.title.empty()) c.title = c.id;
		seen.insert(c.id);
		out.push_back(c);
	}
	return out;
}

std::string collection_dir (const std::string &data_dir) { return join_path(data_dir, "collection"); }
std::string collection_thumbs_dir (const std::string &data_dir) { return join_path(data_dir, "collection-thumbs"); }
std::string collection_file (const std::string &data_dir, const std::string &id) { return join_path(collection_dir(data_dir), id + ".jpg"); }
std::string collection_thumb (const std::string &data_dir, const std::string &id) { return join_path(collection_thumbs_dir(data_dir), id + ".jpg"); }

std::vector<std::string> unused_collection_files (const std::string &data_dir, const std::set<std::string> &keep) {
	std::vector<std::string> out;
	std::string dir = collection_dir(data_dir);
	for (auto &e : list_dir(dir)) {
		if (e.dir) continue;
		std::string path = join_path(dir, e.name);
		if (!keep.count(path)) out.push_back(path);
	}
	return out;
}
