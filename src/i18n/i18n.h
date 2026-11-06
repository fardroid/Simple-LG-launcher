// The language of the screens (lib/i18n.ts). English and Russian are written where they are used, as a pair: tr("Today", "Сегодня").
// Every other language is a dictionary keyed by the English text (src/i18n/locales.inc, from the web version's locales).
#pragma once

#include <string>
#include <vector>

void set_lang (const std::string &lang);
const std::string &lang ();
/** The BCP 47 tag of the current language ("zh-CN" for Chinese). */
std::string lang_tag ();

/** The text in the current language; `key` tells apart the same English words that mean different things. */
std::string tr (const char *en, const char *ru, const char *key = nullptr);
/** Fills {0}, {1}… in a text. */
std::string fmt (const std::string &text, const std::vector<std::string> &values);
inline std::string fmt (const std::string &text, const std::string &a) { return fmt(text, std::vector<std::string>{a}); }
/** The right Russian form of a counted noun: plural(2, {"день", "дня", "дней"}) → "дня". */
std::string plural (long n, const char *one, const char *few, const char *many);
/** A number with its noun: count(3, {"day", "days"}, {"день", "дня", "дней"}) → "3 days" / "3 дня". */
std::string count (long n, const char *en1, const char *en2, const char *ru1, const char *ru2, const char *ru5);
/** The name of a language setting in that language ("Same as the TV" for auto). */
std::string lang_name (const std::string &setting);
