#include "i18n/dates.h"

#include <cctype>
#include <cstdio>

namespace {

struct DateWords {
	const char *lang;
	const char *day[7];
	const char *shortDay[7];
	const char *month[12];
	const char *withYear;          // the parts in order: \001d \001m \001y, the rest literal
	const char *monthInYear[12];
	const char *noYear;
	const char *monthNoYear[12];
};

#include "i18n/dates.inc"

const DateWords &words (const std::string &lang) {
	for (const auto &w : DATE_WORDS) if (lang == w.lang) return w;
	return DATE_WORDS[0];
}

std::string fill (const char *pattern, const char *month, const std::tm &t) {
	std::string out;
	for (const char *p = pattern; *p; p++) {
		if (*p != '\001') { out += *p; continue; }
		char part = *++p;
		if (!part) break;
		if (part == 'd') out += std::to_string(t.tm_mday);
		else if (part == 'm') out += month;
		else if (part == 'y') out += std::to_string(t.tm_year + 1900);
	}
	return out;
}

// the first letter a capital, as the web version does with Intl's text (ASCII only: the other scripts have no case that matters here)
std::string capital (std::string s) {
	if (!s.empty() && (unsigned char)s[0] < 0x80) s[0] = (char)toupper((unsigned char)s[0]);
	return s;
}

}  // namespace

bool known_lang (const std::string &code) {
	for (const auto &w : DATE_WORDS) if (code == w.lang) return true;
	return false;
}

std::string lang_from_tag (const std::string &tag) {
	std::string code;
	for (char c : tag) {
		if (c == '-' || c == '_') break;
		code += (char)tolower((unsigned char)c);
	}
	return known_lang(code) ? code : "en";
}

std::string day_name (const std::string &lang, const std::tm &t) { return words(lang).day[t.tm_wday]; }
std::string day_short (const std::string &lang, int wday) { return words(lang).shortDay[wday % 7]; }
std::string month_name (const std::string &lang, int month) { return words(lang).month[month % 12]; }

std::string date_text (const std::string &lang, const std::tm &t) {
	const auto &w = words(lang);
	return capital(fill(w.noYear, w.monthNoYear[t.tm_mon], t));
}

std::string date_year (const std::string &lang, const std::tm &t) {
	const auto &w = words(lang);
	return capital(fill(w.withYear, w.monthInYear[t.tm_mon], t));
}

TimeParts time_parts (const std::tm &t, bool h24) {
	char buf[16];
	if (h24) {
		snprintf(buf, sizeof buf, "%02d:%02d", t.tm_hour, t.tm_min);
		return {buf, ""};
	}
	int h = t.tm_hour % 12;
	snprintf(buf, sizeof buf, "%d:%02d", h ? h : 12, t.tm_min);
	return {buf, t.tm_hour < 12 ? "AM" : "PM"};
}
