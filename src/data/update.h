// New versions of the launcher (Settings → Home screen → About → Check for new versions): the repository the Homebrew Channel
// reads (repo/apps.json on GitHub) names the latest version, its package and that package's SHA-256. The launcher downloads it
// only when asked, checks the sum, and installs it through the root shell as the Homebrew Channel would.
#pragma once

#include <optional>
#include <string>

extern const char *UPDATE_FEED;     // repo/apps.json on GitHub
/** How often the launcher looks, while it is on (a look that could not reach GitHub waits as long: the next day will do). */
constexpr double UPDATE_EVERY_MS = 24 * 3600 * 1000.0;

struct UpdateInfo {
	std::string version, url, sha256;
	long size = 0;               // bytes; 0 when the repository does not say
};

/** The package of `app_id` in a Homebrew Channel repository's apps.json, or none. Its address must be https; `local` lets a
 *  file:// one through too (a test repository, named in the data folder's update-feed file). */
std::optional<UpdateInfo> parse_update_feed (const std::string &json_text, const std::string &app_id, bool local = false);
/** a is a later version than b (dotted numbers: 2.0.10 after 2.0.9; a missing part counts as 0). */
bool version_newer (const std::string &a, const std::string &b);
/** Where a downloaded version is kept until it is installed (in the launcher's data folder). */
std::string update_file (const std::string &data_dir, const std::string &version);

/** The state shown in Settings, on the home screen's notice and on the row's tile. */
struct UpdateState {
	enum class Step { none, available, downloading, ready, failed } step = Step::none;
	UpdateInfo info;
	double progress = 0;         // 0-1 while downloading
	std::string why;             // failed: what went wrong
	// the look itself (the row under the setting checks again by hand)
	bool checking = false;
	bool latest = false;         // looked, and this is the latest version
	bool unreachable = false;    // the last look could not reach GitHub
};
