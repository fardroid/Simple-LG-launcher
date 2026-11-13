#include "data/cities.h"

#include "i18n/i18n.h"

#include <algorithm>

namespace {

struct Preset { std::string country; City city; };

Preset city (const char *country, const char *en, const char *ru, const char *country_en, const char *country_ru, double lat, double lon) {
	City c;
	c.name = tr(en, ru);
	c.lat = lat;
	c.lon = lon;
	c.detail = tr(country_en, country_ru);
	return {country, c};
}

// the list as written: a country's largest cities, the capital or the largest first
std::vector<Preset> presets () {
	return {
		city("AZ", "Baku", "Баку", "Azerbaijan", "Азербайджан", 40.4093, 49.8671),
		city("AZ", "Ganja", "Гянджа", "Azerbaijan", "Азербайджан", 40.6828, 46.3606),
		city("AZ", "Sumgait", "Сумгаит", "Azerbaijan", "Азербайджан", 40.5855, 49.6317),
		city("RU", "Moscow", "Москва", "Russia", "Россия", 55.7558, 37.6173),
		city("RU", "Saint Petersburg", "Санкт-Петербург", "Russia", "Россия", 59.9343, 30.3351),
		city("RU", "Kazan", "Казань", "Russia", "Россия", 55.7961, 49.1064),
		city("RU", "Yekaterinburg", "Екатеринбург", "Russia", "Россия", 56.8389, 60.6057),
		city("RU", "Novosibirsk", "Новосибирск", "Russia", "Россия", 55.0084, 82.9357),
		city("RU", "Krasnodar", "Краснодар", "Russia", "Россия", 45.0355, 38.9753),
		city("RU", "Sochi", "Сочи", "Russia", "Россия", 43.5855, 39.7231),
		city("UA", "Kyiv", "Киев", "Ukraine", "Украина", 50.4501, 30.5234),
		city("UA", "Lviv", "Львов", "Ukraine", "Украина", 49.8397, 24.0297),
		city("UA", "Odesa", "Одесса", "Ukraine", "Украина", 46.4825, 30.7233),
		city("BY", "Minsk", "Минск", "Belarus", "Беларусь", 53.9006, 27.559),
		city("GE", "Tbilisi", "Тбилиси", "Georgia", "Грузия", 41.7151, 44.8271),
		city("AM", "Yerevan", "Ереван", "Armenia", "Армения", 40.1792, 44.4991),
		city("KZ", "Astana", "Астана", "Kazakhstan", "Казахстан", 51.1694, 71.4491),
		city("KZ", "Almaty", "Алматы", "Kazakhstan", "Казахстан", 43.2389, 76.8897),
		city("UZ", "Tashkent", "Ташкент", "Uzbekistan", "Узбекистан", 41.2995, 69.2401),
		city("KG", "Bishkek", "Бишкек", "Kyrgyzstan", "Киргизия", 42.8746, 74.5698),
		city("TR", "Istanbul", "Стамбул", "Turkey", "Турция", 41.0082, 28.9784),
		city("TR", "Ankara", "Анкара", "Turkey", "Турция", 39.9334, 32.8597),
		city("AE", "Dubai", "Дубай", "UAE", "ОАЭ", 25.2048, 55.2708),
		city("DE", "Berlin", "Берлин", "Germany", "Германия", 52.52, 13.405),
		city("DE", "Munich", "Мюнхен", "Germany", "Германия", 48.1351, 11.582),
		city("DE", "Hamburg", "Гамбург", "Germany", "Германия", 53.5511, 9.9937),
		city("FR", "Paris", "Париж", "France", "Франция", 48.8566, 2.3522),
		city("FR", "Lyon", "Лион", "France", "Франция", 45.764, 4.8357),
		city("FR", "Marseille", "Марсель", "France", "Франция", 43.2965, 5.3698),
		city("GB", "London", "Лондон", "United Kingdom", "Великобритания", 51.5074, -0.1278),
		city("GB", "Manchester", "Манчестер", "United Kingdom", "Великобритания", 53.4808, -2.2426),
		city("ES", "Madrid", "Мадрид", "Spain", "Испания", 40.4168, -3.7038),
		city("ES", "Barcelona", "Барселона", "Spain", "Испания", 41.3874, 2.1686),
		city("IT", "Rome", "Рим", "Italy", "Италия", 41.9028, 12.4964),
		city("IT", "Milan", "Милан", "Italy", "Италия", 45.4642, 9.19),
		city("PT", "Lisbon", "Лиссабон", "Portugal", "Португалия", 38.7223, -9.1393),
		city("PT", "Porto", "Порту", "Portugal", "Португалия", 41.1579, -8.6291),
		city("BR", "São Paulo", "Сан-Паулу", "Brazil", "Бразилия", -23.5505, -46.6333),
		city("BR", "Rio de Janeiro", "Рио-де-Жанейро", "Brazil", "Бразилия", -22.9068, -43.1729),
		city("PL", "Warsaw", "Варшава", "Poland", "Польша", 52.2297, 21.0122),
		city("PL", "Kraków", "Краков", "Poland", "Польша", 50.0647, 19.945),
		city("CZ", "Prague", "Прага", "Czechia", "Чехия", 50.0755, 14.4378),
		city("AT", "Vienna", "Вена", "Austria", "Австрия", 48.2082, 16.3738),
		city("US", "New York", "Нью-Йорк", "USA", "США", 40.7128, -74.006),
		city("US", "Los Angeles", "Лос-Анджелес", "USA", "США", 34.0522, -118.2437),
		city("US", "Chicago", "Чикаго", "USA", "США", 41.8781, -87.6298),
		city("CA", "Toronto", "Торонто", "Canada", "Канада", 43.6532, -79.3832),
		city("CA", "Vancouver", "Ванкувер", "Canada", "Канада", 49.2827, -123.1207),
		city("CN", "Beijing", "Пекин", "China", "Китай", 39.9042, 116.4074),
		city("CN", "Shanghai", "Шанхай", "China", "Китай", 31.2304, 121.4737),
		city("JP", "Tokyo", "Токио", "Japan", "Япония", 35.6762, 139.6503),
		city("JP", "Osaka", "Осака", "Japan", "Япония", 34.6937, 135.5023),
		city("KR", "Seoul", "Сеул", "South Korea", "Южная Корея", 37.5665, 126.978),
		city("KR", "Busan", "Пусан", "South Korea", "Южная Корея", 35.1796, 129.0756)
	};
}

}  // namespace

std::string region_of (const std::string &tag) {
	// "ru-RU", "en_GB", "zh-Hans-CN": the two-letter part after the language
	size_t from = 0;
	bool first = true;
	while (from <= tag.size()) {
		size_t to = tag.find_first_of("-_", from);
		std::string part = tag.substr(from, to == std::string::npos ? std::string::npos : to - from);
		if (!first && part.size() == 2 && isalpha((unsigned char)part[0]) && isalpha((unsigned char)part[1]))
			return {(char)toupper((unsigned char)part[0]), (char)toupper((unsigned char)part[1])};
		first = false;
		if (to == std::string::npos) break;
		from = to + 1;
	}
	return "";
}

std::vector<std::string> language_countries (const std::string &lang) {
	if (lang == "en") return {"US", "GB", "CA"};
	if (lang == "ru") return {"RU"};
	if (lang == "es") return {"ES"};
	if (lang == "fr") return {"FR"};
	if (lang == "de") return {"DE", "AT"};
	if (lang == "it") return {"IT"};
	if (lang == "pt") return {"PT", "BR"};
	if (lang == "tr") return {"TR"};
	if (lang == "uk") return {"UA"};
	if (lang == "pl") return {"PL"};
	if (lang == "zh") return {"CN"};
	if (lang == "ja") return {"JP"};
	if (lang == "ko") return {"KR"};
	return {};
}

std::vector<City> preset_cities (const std::string &region, const std::string &lang) {
	std::vector<Preset> all = presets();
	std::vector<std::string> own = language_countries(lang);
	// 0: the TV's country; 1, 2…: the countries of the launcher's language, in their order; then the others by the country's name
	auto group = [&] (const Preset &p) {
		if (!region.empty() && p.country == region) return 0;
		for (size_t i = 0; i < own.size(); i++) if (p.country == own[i]) return 1 + (int)i;
		return 1 + (int)own.size();
	};
	std::stable_sort(all.begin(), all.end(), [&] (const Preset &a, const Preset &b) {
		int ga = group(a), gb = group(b);
		if (ga != gb) return ga < gb;
		return ga == 1 + (int)own.size() && a.city.detail < b.city.detail;
	});
	std::vector<City> out;
	for (auto &p : all) out.push_back(p.city);
	return out;
}
