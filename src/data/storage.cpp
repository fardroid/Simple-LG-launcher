#include "data/storage.h"

#include "sys/fs.h"

#include <sys/statvfs.h>
#include <unistd.h>

long long free_bytes (const std::string &dir) {
	struct statvfs st;
	if (statvfs(dir.c_str(), &st) != 0) return -1;
	return (long long)st.f_bavail * (long long)st.f_frsize;
}

std::vector<std::string> rebuildable_files (const std::string &data_dir, bool keep_update) {
	std::vector<std::string> out;
	std::vector<DirEntry> entries = list_dir(data_dir);
	if (!keep_update)
		for (auto &e : entries)
			if (!e.dir && e.name.rfind("update-", 0) == 0 && e.name.find(".ipk") != std::string::npos) out.push_back(join_path(data_dir, e.name));
	// a write that was cut short
	for (auto &e : entries)
		if (!e.dir && e.name.size() > 4 && e.name.compare(e.name.size() - 4, 4, ".new") == 0) out.push_back(join_path(data_dir, e.name));
	// the copies of the apps' icons and the previews of the wallpaper collection
	for (const char *folder : {"icons", "collection-thumbs"}) {
		std::string dir = join_path(data_dir, folder);
		for (auto &e : list_dir(dir))
			if (!e.dir) out.push_back(join_path(dir, e.name));
	}
	for (const char *name : {"news.json", "quote-authors.json", "calendar.json", "apps.json", "weather.json", "quote.json"})
		if (file_exists(join_path(data_dir, name))) out.push_back(join_path(data_dir, name));
	return out;
}

long long clear_rebuildable (const std::string &data_dir, bool keep_update) {
	long long freed = 0;
	for (auto &path : rebuildable_files(data_dir, keep_update)) {
		long size = file_size(path);
		if (unlink(path.c_str()) == 0 && size > 0) freed += size;
	}
	return freed;
}
