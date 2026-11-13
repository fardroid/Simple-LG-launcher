// When the TV's storage is full (a write fails with "no space left"): the files the launcher can make again are removed, so that
// the settings can still be kept. The settings themselves, the calendars' links in them and the pictures are never touched.
#pragma once

#include <string>
#include <vector>

/** Free bytes on the disk that holds `dir`; -1 when the TV does not say. */
long long free_bytes (const std::string &dir);
/** The files in the launcher's data folder that it makes again by itself: the downloaded update (unless `keep_update`: one is being
 *  downloaded or waits to be installed), the copies of the apps' icons, and the saved weather, calendar, news, quote and app list.
 *  The largest kinds first. */
std::vector<std::string> rebuildable_files (const std::string &data_dir, bool keep_update);
/** Removes them; the bytes freed. */
long long clear_rebuildable (const std::string &data_dir, bool keep_update);
