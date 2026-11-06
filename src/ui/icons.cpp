#include "ui/icons.h"

#include "core/log.h"
#include "core/tasks.h"
#include "data/apps.h"
#include "gfx/image.h"
#include "sys/fs.h"
#include "sys/luna.h"

#include <algorithm>
#include <deque>
#include <mutex>
#include <unordered_map>

namespace {

struct Entry { TexturePtr tex; bool asked = false, failed = false; };
std::unordered_map<std::string, Entry> entries;
std::mutex ready_lock;
std::deque<std::pair<std::string, std::shared_ptr<Pixels>>> ready;
const int UPLOADS_PER_FRAME = 4;
std::string cache_dir;

// Icon paths go into a root shell: plain absolute paths only (as the web version's SAFE_PATH).
bool safe_path (const std::string &p) {
	if (p.empty() || p[0] != '/') return false;
	for (unsigned char c : p) if (!(isalnum(c) || c == '.' || c == '_' || c == '/' || c == '+' || c == '@' || c == '-')) return false;
	return true;
}

std::string cache_file (const std::string &path) {
	unsigned long long h = 1469598103934665603ULL;          // FNV-1a
	for (unsigned char c : path) { h ^= c; h *= 1099511628211ULL; }
	char name[32];
	snprintf(name, sizeof name, "%016llx.img", h);
	return join_path(cache_dir, name);
}

std::string from_base64 (const std::string &in) {
	static int table[256];
	static bool ready = false;
	if (!ready) {
		for (int &t : table) t = -1;
		const char *a = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		for (int i = 0; i < 64; i++) table[(unsigned char)a[i]] = i;
		ready = true;
	}
	std::string out;
	int bits = 0, value = 0;
	for (unsigned char c : in) {
		if (table[c] < 0) continue;
		value = (value << 6) | table[c];
		bits += 6;
		if (bits >= 8) { bits -= 8; out += (char)((value >> bits) & 0xFF); }
	}
	return out;
}

// The icon file's bytes: read directly; else the copy kept from an earlier read; else through the Homebrew Channel (root), as
// most apps' icons are in folders this app may not read (/media/cryptofs, /mnt/otncabi), and kept for next time.
bool icon_bytes (const std::string &path, std::string &data) {
	if (read_file(path, data, 4u << 20)) return true;
	if (cache_dir.empty() || !safe_path(path)) return false;
	std::string kept = cache_file(path);
	if (read_file(kept, data, 4u << 20)) return true;
	bool ok = false;
	std::string b64 = hbc_exec("base64 " + shell_quote(path) + " 2>/dev/null | tr -d '\\n'", 15000, &ok);
	if (!ok || b64.empty()) return false;
	data = from_base64(b64);
	if (data.empty()) return false;
	make_dirs(cache_dir);
	write_file(kept, data);
	return true;
}

}  // namespace

namespace icons {

void set_cache_dir (const std::string &dir) { cache_dir = dir; }

TexturePtr get (const std::string &path, int size) {
	if (path.empty()) return nullptr;
	Entry &e = entries[path];
	if (e.tex || e.failed) return e.tex;
	if (!e.asked) {
		e.asked = true;
		// twice the size it is shown at, at most: sharp when it grows, and small in memory
		int limit = size * 2;
		// the package's own symbols (a few KB each) at once, on this thread: in the background they wait behind the apps'
		// icons, which the Homebrew Channel reads for seconds, and Settings and the cards showed empty places meanwhile
		std::string data;
		if (path.rfind(package_dir() + "/glyphs/", 0) == 0 && read_file(path, data, 64u << 10)) {
			std::string why;
			std::shared_ptr<Pixels> px = decode_memory(data, 0, 0, &why);
			if (px && (px->w > limit || px->h > limit)) {
				double k = (double)limit / std::max(px->w, px->h);
				px = resize(*px, std::max(1, (int)(px->w * k)), std::max(1, (int)(px->h * k)));
			}
			if (px) e.tex = make_texture(*px);
			else { logf("icon %s: %s", path.c_str(), why.c_str()); e.failed = true; }
			return e.tex;
		}
		run_background([path, limit] {
			std::string why, data;
			std::shared_ptr<Pixels> px;
			if (icon_bytes(path, data)) px = decode_memory(data, 0, 0, &why);
			else why = "cannot read";
			if (px && (px->w > limit || px->h > limit)) {
				double k = (double)limit / std::max(px->w, px->h);
				px = resize(*px, std::max(1, (int)(px->w * k)), std::max(1, (int)(px->h * k)));
			}
			if (!px) logf("icon %s: %s", path.c_str(), why.c_str());
			{
				std::lock_guard<std::mutex> l(ready_lock);
				ready.emplace_back(path, px);
			}
			post_main([] {});      // wakes the main loop, which uploads it and draws again
		});
	}
	return nullptr;
}

bool upload_ready () {
	bool any = false;
	for (int i = 0; i < UPLOADS_PER_FRAME; i++) {
		std::pair<std::string, std::shared_ptr<Pixels>> item;
		{
			std::lock_guard<std::mutex> l(ready_lock);
			if (ready.empty()) break;
			item = ready.front();
			ready.pop_front();
		}
		Entry &e = entries[item.first];
		if (item.second) e.tex = make_texture(*item.second);
		else e.failed = true;
		any = true;
	}
	return any;
}

}  // namespace icons
