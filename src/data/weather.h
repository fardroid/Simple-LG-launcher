// The weather card's data (lib/weather.ts, lib/weathernet.ts): now, every 3 hours of each day and the next days, from Open-Meteo
// (free, no key); reading the reply, describing it, formatting, the saved copy.
#pragma once

#include "data/config.h"

#include <optional>
#include <string>
#include <vector>

struct HourPoint { int day, hour; double tempC; int code; bool isDay; };
struct DayPoint { std::string date; int code; std::optional<double> highC, lowC; };

struct WeatherData {
	double lat = 0, lon = 0;
	double tempC = 0;
	std::optional<double> highC, lowC, feelsC, windMs;
	int code = 0;
	bool isDay = true;
	std::vector<HourPoint> hourly;
	std::vector<DayPoint> daily;
	double at = 0;             // when it was read, ms since 1970
};

constexpr double WEATHER_FRESH_MS = 30 * 60 * 1000.0;
constexpr double WEATHER_SHOWN_MS = 6 * 60 * 60 * 1000.0;

extern const std::vector<std::string> FORECAST_HOSTS;
std::string forecast_url (double lat, double lon, int host = 0);
/** The hosts' numbers in the order to try them: `first` (the one that answered last), then the rest. */
std::vector<int> host_order (int first);
/** The host that answered last (kept by the caller between starts). */
int weather_host ();
void set_weather_host (int i);
std::string geocode_url (const std::string &query);
std::optional<WeatherData> parse_weather (const std::string &json, double lat, double lon, double now);
std::vector<City> parse_cities (const std::string &json);

struct Described { std::string text, icon; };
/** Words and a picture for a WMO weather code; `day` false swaps the sun for the moon. */
Described describe (int code, bool day);
/** "29°" in whole degrees of the unit ("c" or "f"), never "-0°". */
std::string format_temp (double celsius, const std::string &unit);

struct HourView { std::string label, temp; double tempC; int hour; std::string icon; };
struct DayView { std::string label, icon, high, low; bool today, selected; };
struct WeatherView {
	std::string icon, temp, text, detail, city, dayName;
	int day = 0;
	std::vector<HourView> hours;
	std::vector<DayView> days;
};

WeatherView weather_view (const WeatherData &d, const std::string &unit, const std::string &city, int day);
bool weather_fresh (const std::optional<WeatherData> &d, double lat, double lon, double now);
std::optional<WeatherData> usable_weather (const std::optional<WeatherData> &d, double lat, double lon, double now);
std::string weather_json (const WeatherData &d);
std::optional<WeatherData> weather_from_json (const std::string &raw);

/** Fetches the forecast (worker thread). */
std::optional<WeatherData> fetch_weather (const City &city, std::string *why = nullptr);
/** Places by name (worker thread); `ok` false when the service could not be reached. */
std::vector<City> search_cities (const std::string &query, bool *ok = nullptr);
