// Uninstalling an app from the launcher (lib/uninstall.ts; hold OK on a tile). It runs through the Homebrew Channel's root shell,
// so it is careful about what it removes: only apps that look user-installed, never the launcher itself, LG Home, the Homebrew
// Channel or system apps. The shell command is built only from an app ID that has passed a strict check.
#pragma once

#include <string>

extern const char *UNINSTALL_LOG;

struct Verdict { bool ok; std::string reason; };
/** May this app be offered for uninstalling? Needs positive evidence that it is user-installed. `removable`: 1, 0, -1 unknown. */
Verdict uninstall_verdict (const std::string &id, const std::string &icon_path, int removable);
/** The shell command (ask the developer-mode remover, then the store remover, checking after each); empty for a bad ID. */
std::string uninstall_command (const std::string &id);
/** "removed", "still-installed" or "failed", from the command's last line. */
std::string parse_uninstall (const std::string &output);
/** Runs it (worker thread). */
std::string uninstall_app (const std::string &id);
