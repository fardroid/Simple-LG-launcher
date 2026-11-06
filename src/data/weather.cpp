#include "data/weather.h"

#include "i18n/dates.h"
#include "i18n/i18n.h"
#include "sys/http.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cmath>
#include <ctime>

using json = nlohmann::json;

namespace {

std::optional<double> num (const json &v) {
	if (v.is_number() && std::isfinite(v.get<double>())) return v.get<double>();
	return std::nullopt;
}

std::optional<double> at (const json &list, size_t i) { return list.is_array() && i < list.size() ? num(list[i]) : std::nullopt; }

// plain numbers only, three decimals: nothing else can get into the address
std::string coord (double n) {
	char b[32];
	snprintf(b, sizeof b, "%.3f", std::round(n * 1000) / 1000);
	return b;
}

bool valid_coords (double lat, double lon) { return std::isfinite(lat) && std::isfinite(lon) && std::fabs(lat) <= 90 && std::fabs(lon) <= 180; }

double now_ms () {
	using namespace std::chrono;
	return (double)duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

// "2026-10-25" -> its parts
std::tm date_of (const std::string &d) {
	std::tm t{};
	if (d.size() >= 10) {
		t.tm_year = atoi(d.substr(0, 4).c_str()) - 1900;
		t.tm_mon = atoi(d.substr(5, 2).c_str()) - 1;
		t.tm_mday = atoi(d.substr(8, 2).c_str());
		t.tm_hour = 12;
		std::tm copy = t;
		timegm(&copy);                 // sets tm_wday (plain date arithmetic, no time zone)
		t.tm_wday = copy.tm_wday;
	}
	return t;
}

std::string wind_text (double ms, const std::string &unit) {
	if (lang() == "ru") return "ветер " + std::to_string((long)std::lround(ms)) + " м/с";
	std::string v = unit == "f" ? std::to_string((long)std::lround(ms * 2.237)) + " mph" : std::to_string((long)std::lround(ms * 3.6)) + " km/h";
	return fmt(tr("wind {0}", "ветер {0}"), v);
}

}  // namespace

std::string forecast_url (double lat, double lon) {
	return "https://api.open-meteo.com/v1/forecast?latitude=" + coord(lat) + "&longitude=" + coord(lon) +
		"&current=temperature_2m,apparent_temperature,weather_code,is_day,wind_speed_10m&hourly=temperature_2m,weather_code,is_day"
		"&daily=weather_code,temperature_2m_max,temperature_2m_min&forecast_days=7&wind_speed_unit=ms&timezone=auto";
}

std::string geocode_url (const std::string &query) {
	std::string q = query;
	while (!q.empty() && isspace((unsigned char)q.front())) q.erase(q.begin());
	while (!q.empty() && isspace((unsigned char)q.back())) q.pop_back();
	if (q.size() > 60) q = q.substr(0, 60);
	return "https://geocoding-api.open-meteo.com/v1/search?name=" + url_encode(q) + "&count=8&language=" + lang_tag() + "&format=json";
}

std::optional<WeatherData> parse_weather (const std::string &text, double lat, double lon, double now) {
	json data = json::parse(text, nullptr, false);
	if (!data.is_object() || !data["current"].is_object()) return std::nullopt;
	const json &cur = data["current"];
	auto temp = num(cur.value("temperature_2m", json())), code = num(cur.value("weather_code", json()));
	if (!temp || !code) return std::nullopt;
	WeatherData d;
	d.lat = lat;
	d.lon = lon;
	d.tempC = *temp;
	d.code = (int)*code;
	d.isDay = !(cur.value("is_day", json()).is_number() && cur["is_day"].get<double>() == 0);
	d.feelsC = num(cur.value("apparent_temperature", json()));
	d.windMs = num(cur.value("wind_speed_10m", json()));
	const json daily = data.value("daily", json::object());
	const json dates = daily.value("time", json::array());
	for (size_t i = 0; i < dates.size() && i < 7; i++) {
		auto dc = at(daily.value("weather_code", json()), i);
		if (dates[i].is_string() && dc) d.daily.push_back({dates[i].get<std::string>(), (int)*dc, at(daily.value("temperature_2m_max", json()), i), at(daily.value("temperature_2m_min", json()), i)});
	}
	d.highC = at(daily.value("temperature_2m_max", json()), 0);
	d.lowC = at(daily.value("temperature_2m_min", json()), 0);
	const json hours = data.value("hourly", json::object());
	const json times = hours.value("time", json::array());
	for (size_t i = 0; i < times.size(); i++) {
		if (!times[i].is_string()) continue;
		std::string t = times[i].get<std::string>();
		int day = -1;
		for (size_t k = 0; k < d.daily.size(); k++) if (t.compare(0, d.daily[k].date.size(), d.daily[k].date) == 0) { day = (int)k; break; }
		if (day == -1 || t.size() < 13) continue;
		int hour = atoi(t.substr(11, 2).c_str());
		auto ht = at(hours.value("temperature_2m", json()), i), hc = at(hours.value("weather_code", json()), i), hd = at(hours.value("is_day", json()), i);
		if (hour % 3 == 0 && ht && hc) d.hourly.push_back({day, hour, *ht, (int)*hc, !(hd && *hd == 0)});
	}
	d.at = now;
	return d;
}

std::vector<City> parse_cities (const std::string &text) {
	std::vector<City> out;
	json data = json::parse(text, nullptr, false);
	if (!data.is_object() || !data["results"].is_array()) return out;
	for (auto &r : data["results"]) {
		if (!r.is_object() || !r["name"].is_string()) continue;
		auto la = num(r.value("latitude", json())), lo = num(r.value("longitude", json()));
		if (!la || !lo || !valid_coords(*la, *lo)) continue;
		std::string detail;
		for (const char *k : {"admin1", "country"}) {
			if (r[k].is_string() && !r[k].get<std::string>().empty()) detail += (detail.empty() ? "" : ", ") + r[k].get<std::string>();
		}
		out.push_back({r["name"].get<std::string>(), *la, *lo, detail});
	}
	return out;
}

Described describe (int code, bool day) {
	if (code == 0) return {tr("Clear", "Ясно"), day ? "sun" : "moon"};
	if (code == 1) return {tr("Mainly clear", "Преимущественно ясно"), day ? "sun" : "moon"};
	if (code == 2) return {tr("Partly cloudy", "Переменная облачность"), day ? "partly" : "cmoon"};
	if (code == 3) return {tr("Overcast", "Пасмурно"), "cloud"};
	if (code == 45 || code == 48) return {tr("Fog", "Туман"), "fog"};
	if (code >= 51 && code <= 55) return {tr("Drizzle", "Морось"), "rain"};
	if (code == 56 || code == 57) return {tr("Freezing drizzle", "Ледяная морось"), "rain"};
	if (code == 61) return {tr("Light rain", "Небольшой дождь"), "rain"};
	if (code == 63) return {tr("Rain", "Дождь"), "rain"};
	if (code == 65) return {tr("Heavy rain", "Сильный дождь"), "rain"};
	if (code == 66 || code == 67) return {tr("Freezing rain", "Ледяной дождь"), "rain"};
	if (code == 71) return {tr("Light snow", "Небольшой снег"), "snow"};
	if (code == 73) return {tr("Snow", "Снег"), "snow"};
	if (code == 75) return {tr("Heavy snow", "Сильный снег"), "snow"};
	if (code == 77) return {tr("Snow grains", "Снежная крупа"), "snow"};
	if (code == 80) return {tr("Light showers", "Небольшие ливни"), "rain"};
	if (code == 81) return {tr("Showers", "Ливни"), "rain"};
	if (code == 82) return {tr("Heavy showers", "Сильные ливни"), "rain"};
	if (code == 85 || code == 86) return {tr("Snow showers", "Снегопад"), "snow"};
	if (code == 95) return {tr("Thunderstorm", "Гроза"), "storm"};
	if (code == 96 || code == 99) return {tr("Thunderstorm with hail", "Гроза с градом"), "storm"};
	return {tr("Cloudy", "Облачно"), "cloud"};
}

std::string format_temp (double c, const std::string &unit) {
	double v = unit == "f" ? c * 9 / 5 + 32 : c;
	long r = std::lround(v);
	// Math.round rounds -0.5 up to 0; lround rounds it away from zero: follow JavaScript
	if (v - std::floor(v) == 0.5) r = (long)std::floor(v) + 1;
	return std::to_string(r == 0 ? 0 : r) + "°";
}

WeatherView weather_view (const WeatherData &d, const std::string &unit, const std::string &city, int day) {
	WeatherView v;
	int picked = std::max(0, std::min(day, (int)d.daily.size() - 1));
	auto desc = describe(d.code, d.isDay);
	std::vector<std::string> detail;
	if (d.feelsC) detail.push_back(fmt(tr("Feels like {0}", "Ощущается как {0}"), format_temp(*d.feelsC, unit)));
	if (d.windMs) detail.push_back(wind_text(*d.windMs, unit));
	v.icon = desc.icon;
	v.temp = format_temp(d.tempC, unit);
	v.text = desc.text;
	for (size_t i = 0; i < detail.size(); i++) v.detail += (i ? " · " : "") + detail[i];
	v.city = city;
	v.day = picked;
	if (picked == 0 || picked >= (int)d.daily.size()) v.dayName = tr("Today", "Сегодня");
	else {
		std::tm t = date_of(d.daily[picked].date);
		v.dayName = day_name(lang(), t) + ", " + date_text(lang(), t);
	}
	for (auto &h : d.hourly) {
		if (h.day != picked) continue;
		char label[8];
		snprintf(label, sizeof label, "%02d:00", h.hour);
		v.hours.push_back({label, format_temp(h.tempC, unit), h.tempC, h.hour, describe(h.code, h.isDay).icon});
	}
	for (size_t i = 0; i < d.daily.size(); i++) {
		const DayPoint &p = d.daily[i];
		std::tm t = date_of(p.date);
		std::string label = i == 0 ? tr("Today", "Сегодня") : day_short(lang(), t.tm_wday) + " " + std::to_string(t.tm_mday);
		v.days.push_back({label, describe(p.code, true).icon, p.highC ? format_temp(*p.highC, unit) : "–", p.lowC ? format_temp(*p.lowC, unit) : "–", i == 0, (int)i == picked});
	}
	return v;
}

bool weather_fresh (const std::optional<WeatherData> &d, double lat, double lon, double now) {
	return d && d->lat == lat && d->lon == lon && now - d->at < WEATHER_FRESH_MS && now >= d->at;
}

std::optional<WeatherData> usable_weather (const std::optional<WeatherData> &d, double lat, double lon, double now) {
	if (d && d->lat == lat && d->lon == lon && now - d->at < WEATHER_SHOWN_MS && now >= d->at) return d;
	return std::nullopt;
}

std::string weather_json (const WeatherData &d) {
	auto opt = [] (const std::optional<double> &x) { return x ? json(*x) : json(nullptr); };
	json hourly = json::array(), daily = json::array();
	for (auto &h : d.hourly) hourly.push_back({{"day", h.day}, {"hour", h.hour}, {"tempC", h.tempC}, {"code", h.code}, {"isDay", h.isDay}});
	for (auto &p : d.daily) daily.push_back({{"date", p.date}, {"code", p.code}, {"highC", opt(p.highC)}, {"lowC", opt(p.lowC)}});
	return json({{"lat", d.lat}, {"lon", d.lon}, {"tempC", d.tempC}, {"highC", opt(d.highC)}, {"lowC", opt(d.lowC)}, {"code", d.code},
		{"isDay", d.isDay}, {"feelsC", opt(d.feelsC)}, {"windMs", opt(d.windMs)}, {"hourly", hourly}, {"daily", daily}, {"at", d.at}}).dump();
}

std::optional<WeatherData> weather_from_json (const std::string &raw) {
	json j = json::parse(raw, nullptr, false);
	if (!j.is_object()) return std::nullopt;
	auto lat = num(j.value("lat", json())), lon = num(j.value("lon", json())), temp = num(j.value("tempC", json())), code = num(j.value("code", json())), when = num(j.value("at", json()));
	if (!lat || !lon || !valid_coords(*lat, *lon) || !temp || !code || !when || !j["daily"].is_array() || !j["hourly"].is_array()) return std::nullopt;
	WeatherData d;
	d.lat = *lat; d.lon = *lon; d.tempC = *temp; d.code = (int)*code; d.at = *when;
	d.highC = num(j.value("highC", json())); d.lowC = num(j.value("lowC", json()));
	d.feelsC = num(j.value("feelsC", json())); d.windMs = num(j.value("windMs", json()));
	d.isDay = j.value("isDay", true);
	for (auto &h : j["hourly"]) {
		if (!h.is_object()) continue;
		d.hourly.push_back({h.value("day", 0), h.value("hour", 0), h.value("tempC", 0.0), h.value("code", 0), h.value("isDay", true)});
	}
	for (auto &p : j["daily"]) {
		if (!p.is_object()) continue;
		d.daily.push_back({p.value("date", std::string()), p.value("code", 0), num(p.value("highC", json())), num(p.value("lowC", json()))});
	}
	return d;
}

std::optional<WeatherData> fetch_weather (const City &city, std::string *why) {
	if (!valid_coords(city.lat, city.lon)) { if (why) *why = "bad place"; return std::nullopt; }
	HttpResult r = http_get(forecast_url(city.lat, city.lon), 15);
	if (!r.ok) { if (why) *why = r.error; return std::nullopt; }
	auto d = parse_weather(r.body, city.lat, city.lon, now_ms());
	if (!d && why) *why = "unreadable reply";
	return d;
}

std::vector<City> search_cities (const std::string &query, bool *ok) {
	std::string q = query;
	if (ok) *ok = true;
	if (q.size() < 2) return {};
	HttpResult r = http_get(geocode_url(q), 15);
	if (ok) *ok = r.ok;
	return r.ok ? parse_cities(r.body) : std::vector<City>();
}
