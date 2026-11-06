#include "sys/fs.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <strings.h>

bool read_file (const std::string &path, std::string &out, size_t max_bytes) {
	FILE *f = fopen(path.c_str(), "rb");
	if (!f) return false;
	out.clear();
	char buf[65536];
	size_t n;
	while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
		out.append(buf, n);
		if (out.size() > max_bytes) { fclose(f); out.clear(); return false; }
	}
	fclose(f);
	return true;
}

bool write_file (const std::string &path, const std::string &data) {
	std::string tmp = path + ".new";
	FILE *f = fopen(tmp.c_str(), "wb");
	if (!f) return false;
	bool ok = fwrite(data.data(), 1, data.size(), f) == data.size();
	ok = fflush(f) == 0 && ok;
	fsync(fileno(f));
	fclose(f);
	if (!ok) { unlink(tmp.c_str()); return false; }
	return rename(tmp.c_str(), path.c_str()) == 0;
}

bool file_exists (const std::string &path) {
	struct stat st;
	return stat(path.c_str(), &st) == 0;
}

long file_size (const std::string &path) {
	struct stat st;
	return stat(path.c_str(), &st) == 0 ? (long)st.st_size : -1;
}

bool make_dirs (const std::string &path) {
	if (path.empty() || file_exists(path)) return true;
	make_dirs(parent_path(path));
	return mkdir(path.c_str(), 0755) == 0 || file_exists(path);
}

std::vector<DirEntry> list_dir (const std::string &path) {
	std::vector<DirEntry> out;
	DIR *d = opendir(path.c_str());
	if (!d) return out;
	while (dirent *e = readdir(d)) {
		if (e->d_name[0] == '.') continue;
		struct stat st;
		std::string full = join_path(path, e->d_name);
		if (stat(full.c_str(), &st) != 0) continue;
		out.push_back({e->d_name, S_ISDIR(st.st_mode)});
	}
	closedir(d);
	// folders first, then the rest, each A-Z ignoring case (as the web version's parseListing)
	std::sort(out.begin(), out.end(), [] (const DirEntry &a, const DirEntry &b) {
		if (a.dir != b.dir) return a.dir;
		return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
	});
	return out;
}

std::string join_path (const std::string &base, const std::string &name) {
	std::string b = base;
	while (b.size() > 1 && b.back() == '/') b.pop_back();
	return b == "/" ? "/" + name : b + "/" + name;
}

std::string base_name (const std::string &path) {
	std::string p = path;
	while (p.size() > 1 && p.back() == '/') p.pop_back();
	size_t i = p.rfind('/');
	return i == std::string::npos ? p : p.substr(i + 1);
}

std::string parent_path (const std::string &path) {
	std::string p = path;
	while (p.size() > 1 && p.back() == '/') p.pop_back();
	size_t i = p.rfind('/');
	if (i == std::string::npos) return "";
	return i == 0 ? "/" : p.substr(0, i);
}

bool is_picture (const std::string &name) {
	if (name.empty() || name[0] == '.') return false;
	size_t dot = name.rfind('.');
	if (dot == std::string::npos) return false;
	std::string ext = name.substr(dot + 1);
	for (auto &ch : ext) ch = (char)tolower((unsigned char)ch);
	return ext == "jpg" || ext == "jpeg" || ext == "png";
}
