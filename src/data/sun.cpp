#include "data/sun.h"

#include <algorithm>
#include <cmath>

namespace {
const double RAD = 3.14159265358979323846 / 180.0;
}

double sun_elevation (double unix_s, double lat, double lon) {
	double d = unix_s / 86400.0 + 2440587.5 - 2451545.0;          // days since 1 January 2000, 12:00 UTC
	double g = (357.529 + 0.98560028 * d) * RAD;                   // the sun's mean anomaly
	double q = 280.459 + 0.98564736 * d;                           // its mean longitude
	double l = (q + 1.915 * std::sin(g) + 0.020 * std::sin(2 * g)) * RAD;
	double e = (23.439 - 0.00000036 * d) * RAD;                    // the tilt of the Earth's axis
	double ra = std::atan2(std::cos(e) * std::sin(l), std::cos(l));
	double decl = std::asin(std::sin(e) * std::sin(l));
	double gmst = 18.697374558 + 24.06570982441908 * d;            // sidereal time at Greenwich, hours
	double hour_angle = (gmst * 15 + lon) * RAD - ra;
	double h = std::sin(lat * RAD) * std::sin(decl) + std::cos(lat * RAD) * std::cos(decl) * std::cos(hour_angle);
	return std::asin(std::max(-1.0, std::min(1.0, h))) / RAD;
}

double night_level (double elevation) { return std::max(0.0, std::min(1.0, -elevation / 6.0)); }

double night_level_by_clock (const std::tm &local) { return local.tm_hour >= 19 || local.tm_hour < 7 ? 1 : 0; }
