// Time zones by name ("Europe/Rome"), read from the system's zoneinfo files (TZif), for calendar events that name their zone.
// The web version asks Intl; native code has none. Thread-safe; each zone is read once.
#pragma once

#include <optional>
#include <string>

/** How far the zone's wall clock is ahead of UTC at the moment `utc_ms`, in ms (empty when the zone is unknown). */
std::optional<long long> zone_offset (const std::string &tz, long long utc_ms);
/** Where the zoneinfo files are (default /usr/share/zoneinfo). */
void set_zoneinfo_dir (const std::string &dir);
