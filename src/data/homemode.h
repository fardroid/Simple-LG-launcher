// How the launcher replaces LG Home (lib/homemode.ts), chosen in Settings → Home screen → "How the launcher opens". The switching
// is done on the TV by tools/homemode.sh (packaged in the app's tools/ folder, see scripts/gen-tools.sh), one way at a time.
#pragma once

#include <optional>
#include <string>
#include <vector>

extern const std::vector<std::string> HOME_MODES;     // default mount helper none

struct HomeModeInfo {
	std::string mode;            // none helper default mount
	bool can_default = false;    // the TV lets an app other than LG Home be chosen as Home
	std::string release;         // webOS release, such as 10.3.2 (webOS 25); empty when unknown
};

struct SwitchResult { bool ok; std::string mode, why; };

std::optional<HomeModeInfo> parse_mode_status (const std::string &out);
/** The result line of a switch, or none while it is still running. */
std::optional<SwitchResult> parse_result (const std::string &out);
int release_major (const std::string &release);

struct ModeText { std::string name, how; std::vector<std::string> pros, cons; };
ModeText mode_text (const std::string &mode);
struct ModeNote { bool blocked; std::string text; };
/** Why a way cannot be chosen here, or a caution about it. */
ModeNote mode_note (const std::string &mode, const std::optional<HomeModeInfo> &info);

/** The TV-side commands, for the app installed under `app_id`. */
std::string tools_dir (const std::string &app_id);
std::string status_command (const std::string &app_id);
std::string run_command (const std::string &app_id, const std::string &mode);
std::string refresh_command (const std::string &app_id);
std::string result_file (const std::string &app_id);
/** The move from the web version (tools/migrate.sh): what it put on the TV turned into this version's. */
std::string migrate_command (const std::string &app_id);
/** The Swap and the Helper: the launcher (as `id`) named the TV's last input, which a full start opens (tools/lastinput.sh). */
std::string name_last_input_command (const std::string &app_id, const std::string &id);
/** What was the last input before put back (those ways switched off). */
std::string restore_last_input_command (const std::string &app_id);
/** The Swap (tools/takeover.sh): the copy its boot link runs brought up to the package's, while it is on. */
std::string takeover_refresh_command (const std::string &app_id);
/** The helper (tools/helper.sh): its files brought up to the package's and it started, while it is on. */
std::string helper_refresh_command (const std::string &app_id);
/** The helper stands back while LG's own Home is opened from its tile. */
std::string stock_command (const std::string &app_id);
std::string hook_dir (const std::string &app_id);
/** Touched by the launcher whenever it comes on screen: the helper checks for it after opening the launcher. */
std::string alive_file (const std::string &app_id);

// Run on worker threads, through the Homebrew Channel's shell.
/** `root`, when given: whether the Homebrew Channel ran the command (the TV can run commands as root); `why`: what it said if not. */
std::optional<HomeModeInfo> read_home_mode (const std::string &app_id, bool *root = nullptr, std::string *why = nullptr);

// Whether this TV can run commands through the Homebrew Channel (rooted), told from the answer to the first command Settings sends
// (lib/prefs.ts disabledReason): the rows that run shell commands are greyed out without it, with why.
enum class Root { unknown, yes, no };
/** The rows that run commands as root. */
extern const std::vector<std::string> ROOT_OPTIONS;
/** Why a row needing root cannot be used (`detail`: what the TV said); empty while it can, or while it is not known. */
std::string root_reason (Root root, const std::string &detail);
/** Starts a switch (it runs on its own on the TV: the launcher may be closed by it) and waits for its result. */
SwitchResult switch_home_mode (const std::string &app_id, const std::string &mode);
