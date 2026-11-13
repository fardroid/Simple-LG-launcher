// Day or night, for dimming the wallpaper at night: the sun's height over a place, worked out on the TV (no request), and how far
// into the night it is.
#pragma once

#include <ctime>
#include <optional>

/** The sun's height over the horizon, in degrees, at `unix_s` (seconds since 1970, UTC) at this latitude and longitude (the
 *  Astronomical Almanac's low-precision formulas: right to about a hundredth of a degree). */
double sun_elevation (double unix_s, double lat, double lon);
/** 0 by day, 1 at night, and in between while it gets dark or light (the sun from the horizon down to 6° under it: civil
 *  twilight, about half an hour). */
double night_level (double elevation);
/** The same without a place: night from 19:00 to 7:00 by the TV's clock. */
double night_level_by_clock (const std::tm &local);
