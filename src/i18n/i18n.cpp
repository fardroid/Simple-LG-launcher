#include "i18n/i18n.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

namespace {

struct Entry { const char *key, *text; };
struct Locale { const char *lang; const Entry *entries; };
#include "i18n/locales.inc"

// lib/i18n.ts LANG_INFO
struct Info { const char *lang, *name, *tag, *rule; };
const Info INFO[] = {
	{"en", "English", "en", "en"}, {"ru", "Русский", "ru", "slavic"}, {"es", "Español", "es", "en"}, {"fr", "Français", "fr", "fr"},
	{"de", "Deutsch", "de", "en"}, {"it", "Italiano", "it", "en"}, {"pt", "Português", "pt", "fr"}, {"tr", "Türkçe", "tr", "en"},
	{"uk", "Українська", "uk", "slavic"}, {"zh", "简体中文", "zh-CN", "none"}, {"ja", "日本語", "ja", "none"}, {"ko", "한국어", "ko", "none"},
	{"pl", "Polski", "pl", "pl"}
};

std::string current = "en";
std::unordered_map<std::string, const char *> dictionary;

const Info &info () {
	for (const auto &i : INFO) if (current == i.lang) return i;
	return INFO[0];
}

int form_index (const char *rule, long n) {
	long x = std::labs(n);
	if (!strcmp(rule, "none")) return 0;
	if (!strcmp(rule, "slavic")) {
		long a = x % 100, b = a % 10;
		return a > 10 && a < 20 ? 2 : b > 1 && b < 5 ? 1 : b == 1 ? 0 : 2;
	}
	if (!strcmp(rule, "pl")) {
		long a = x % 100, b = a % 10;
		return x == 1 ? 0 : b > 1 && b < 5 && (a < 12 || a > 14) ? 1 : 2;
	}
	if (!strcmp(rule, "fr")) return x < 2 ? 0 : 1;
	return x == 1 ? 0 : 1;
}

}  // namespace

void set_lang (const std::string &l) {
	current = "en";
	for (const auto &i : INFO) if (l == i.lang) current = l;
	dictionary.clear();
	for (const auto &loc : LOCALES)
		if (current == loc.lang)
			for (const Entry *e = loc.entries; e->key; e++) dictionary[e->key] = e->text;
}

const std::string &lang () { return current; }
std::string lang_tag () { return info().tag; }

std::string tr (const char *en, const char *ru, const char *key) {
	if (current == "en") return en;
	if (current == "ru") return ru;
	auto it = dictionary.find(key ? key : en);
	return it == dictionary.end() ? en : it->second;
}

std::string fmt (const std::string &text, const std::vector<std::string> &values) {
	std::string out;
	for (size_t i = 0; i < text.size(); i++) {
		if (text[i] == '{') {
			size_t j = i + 1;
			while (j < text.size() && isdigit((unsigned char)text[j])) j++;
			if (j > i + 1 && j < text.size() && text[j] == '}') {
				size_t n = (size_t)atoi(text.c_str() + i + 1);
				if (n < values.size()) { out += values[n]; i = j; continue; }
			}
		}
		out += text[i];
	}
	return out;
}

std::string plural (long n, const char *one, const char *few, const char *many) {
	long a = std::labs(n) % 100, b = a % 10;
	if (a > 10 && a < 20) return many;
	if (b > 1 && b < 5) return few;
	return b == 1 ? one : many;
}

std::string count (long n, const char *en1, const char *en2, const char *ru1, const char *ru2, const char *ru5) {
	std::string num = std::to_string(n);
	if (current == "ru") return num + " " + plural(n, ru1, ru2, ru5);
	if (current == "en") return num + " " + (std::labs(n) == 1 ? en1 : en2);
	auto it = dictionary.find(en2);
	std::string forms = it == dictionary.end() ? en2 : it->second;
	std::vector<std::string> list;
	size_t start = 0;
	for (;;) {
		size_t bar = forms.find('|', start);
		list.push_back(forms.substr(start, bar == std::string::npos ? std::string::npos : bar - start));
		if (bar == std::string::npos) break;
		start = bar + 1;
	}
	const char *rule = info().rule;
	size_t i = std::min((size_t)form_index(rule, n), list.size() - 1);
	return num + (strcmp(rule, "none") ? " " : "") + list[i];
}

std::string lang_name (const std::string &setting) {
	if (setting == "auto") return tr("Same as the TV", "Как на ТВ");
	for (const auto &i : INFO) if (setting == i.lang) return i.name;
	return setting;
}
