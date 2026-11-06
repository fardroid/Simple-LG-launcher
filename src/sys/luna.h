// Luna (the TV's system bus), through the TV's libhelpers: the library SDL-webOS itself uses, so the app has one registration,
// under its own id. Answers arrive on libhelpers' thread and are handed to the main thread.
#pragma once

#include <functional>
#include <string>

/** One call; `done` runs on the main thread with the answer's JSON text (or {"returnValue":false,"errorText":...} on failure
 *  or after `timeout_ms`). */
void luna_call (const std::string &uri, const std::string &payload, std::function<void(const std::string &)> done, int timeout_ms = 8000);

/** A subscription: `each` runs on the main thread for every message. */
void luna_subscribe (const std::string &uri, const std::string &payload, std::function<void(const std::string &)> each);

/** A call from a worker thread that waits for its answer. */
std::string luna_call_sync (const std::string &uri, const std::string &payload, int timeout_ms = 8000);

/** A shell command as root through the Homebrew Channel's service (worker threads only); its standard output, or "" on failure. */
std::string hbc_exec (const std::string &command, int timeout_ms = 15000, bool *ok = nullptr, std::string *why = nullptr);

/** A value quoted for a POSIX shell. */
std::string shell_quote (const std::string &value);
