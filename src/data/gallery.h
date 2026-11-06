// Where wallpaper pictures can be (lib/wallpaper.ts): the USB drives and the TV's developer storage, and the folders in them.
// The native app reads them directly; what it may not read is listed through the Homebrew Channel's shell (ls -1p).
#pragma once

#include "sys/fs.h"

#include <string>
#include <vector>

struct StorageRoot { std::string label, path; };

extern const char *INTERNAL_ROOT_PATH;
std::string internal_root_label ();
/** Control characters can never be part of a path given to a shell. */
bool safe_path (const std::string &path);
/** /proc/mounts: the mounted USB drives (numbered when there are several). */
std::vector<StorageRoot> parse_mounts (const std::string &text);
/** `ls -1p`: folders first, then pictures, each A-Z; hidden files and other files left out. */
std::vector<DirEntry> parse_listing (const std::string &text);
/** The USB drives, then the TV storage (worker thread). `ok` false when the mounts could not be read. */
std::vector<StorageRoot> list_roots (bool *ok);
/** The folders and pictures in a folder (worker thread). `ok` false when it could not be read. */
std::vector<DirEntry> list_pictures (const std::string &path, bool *ok);
