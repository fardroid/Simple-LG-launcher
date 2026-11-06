#include "data/gallery.h"

#include "i18n/i18n.h"
#include "sys/luna.h"

#include <dirent.h>
#include <strings.h>

#include <algorithm>

const char *INTERNAL_ROOT_PATH = "/media/developer";

std::string internal_root_label () { return tr("TV storage", "Память ТВ"); }

bool safe_path (const std::string &path) {
	if (path.empty() || path[0] != '/') return false;
	for (unsigned char c : path) if (c < 0x20 || c == 0x7f) return false;
	return true;
}

std::vector<StorageRoot> parse_mounts (const std::string &text) {
	std::vector<StorageRoot> roots;
	size_t at = 0;
	while (at < text.size()) {
		size_t nl = text.find('\n', at);
		std::string line = text.substr(at, nl == std::string::npos ? std::string::npos : nl - at);
		at = nl == std::string::npos ? text.size() : nl + 1;
		size_t s1 = line.find(' ');
		if (s1 == std::string::npos) continue;
		size_t s2 = line.find(' ', s1 + 1);
		if (s2 == std::string::npos) continue;
		std::string dev = line.substr(0, s1), raw = line.substr(s1 + 1, s2 - s1 - 1), mount;
		for (size_t i = 0; i < raw.size(); i++) {
			if (raw.compare(i, 4, "\\040") == 0) { mount += ' '; i += 3; }
			else mount += raw[i];
		}
		std::string low = mount;
		for (auto &c : low) c = (char)tolower((unsigned char)c);
		bool usb = (dev.compare(0, 7, "/dev/sd") == 0 && dev.size() > 7 && islower((unsigned char)dev[7])) || low.find("/usb/") != std::string::npos;
		if (!usb || !safe_path(mount)) continue;
		bool seen = false;
		for (auto &r : roots) seen = seen || r.path == mount;
		if (!seen) roots.push_back({tr("USB drive", "USB-накопитель"), mount});
	}
	if (roots.size() > 1)
		for (size_t i = 0; i < roots.size(); i++) roots[i].label = fmt(tr("USB drive {0}", "USB-накопитель {0}"), std::to_string(i + 1));
	return roots;
}

namespace {
void sort_entries (std::vector<DirEntry> &list) {
	std::sort(list.begin(), list.end(), [] (const DirEntry &a, const DirEntry &b) {
		if (a.dir != b.dir) return a.dir;
		return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
	});
}
}  // namespace

std::vector<DirEntry> parse_listing (const std::string &text) {
	std::vector<DirEntry> out;
	size_t at = 0;
	while (at < text.size()) {
		size_t nl = text.find('\n', at);
		std::string name = text.substr(at, nl == std::string::npos ? std::string::npos : nl - at);
		at = nl == std::string::npos ? text.size() : nl + 1;
		if (!name.empty() && name.back() == '\r') name.pop_back();
		if (name.empty() || name[0] == '.') continue;
		if (name.back() == '/') out.push_back({name.substr(0, name.size() - 1), true});
		else if (is_picture(name)) out.push_back({name, false});
	}
	sort_entries(out);
	return out;
}

std::vector<StorageRoot> list_roots (bool *ok) {
	std::string text;
	*ok = read_file("/proc/mounts", text);
	if (!*ok) text = hbc_exec("cat /proc/mounts", 8000, ok);
	auto roots = *ok ? parse_mounts(text) : std::vector<StorageRoot>();
	roots.push_back({internal_root_label(), INTERNAL_ROOT_PATH});
	return roots;
}

std::vector<DirEntry> list_pictures (const std::string &path, bool *ok) {
	*ok = false;
	if (!safe_path(path)) return {};
	if (DIR *d = opendir(path.c_str())) {
		closedir(d);
		*ok = true;
		std::vector<DirEntry> out;
		for (auto &e : list_dir(path)) if (e.dir || is_picture(e.name)) out.push_back(e);
		return out;
	}
	std::string text = hbc_exec("ls -1p -- " + shell_quote(path) + " 2>/dev/null", 15000, ok);
	return *ok ? parse_listing(text) : std::vector<DirEntry>();
}
