// Files and folders (the app reads the TV's storage directly: /media/developer, the app folders, /usr/share/fonts).
#pragma once

#include <string>
#include <vector>

bool read_file (const std::string &path, std::string &out, size_t max_bytes = 64u << 20);
/** Writes through a temporary file and a rename, so a crash never leaves half a file. */
bool write_file (const std::string &path, const std::string &data);
bool file_exists (const std::string &path);
long file_size (const std::string &path);
bool make_dirs (const std::string &path);

struct DirEntry { std::string name; bool dir; };
/** The entries of a folder, without hidden ones ("." first). */
std::vector<DirEntry> list_dir (const std::string &path);

std::string join_path (const std::string &base, const std::string &name);
std::string base_name (const std::string &path);
std::string parent_path (const std::string &path);
/** A picture the launcher can show: .jpg .jpeg .png (.webp and .bmp are listed by the web version too, but not decoded here). */
bool is_picture (const std::string &name);
