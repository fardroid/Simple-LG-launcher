// Dates in the launcher's languages (lib/clock.ts): the words come from src/i18n/dates.inc (scripts/gen-dates.mjs).
#pragma once

#include <ctime>
#include <string>

/** The language codes the launcher has (lib/i18n.ts LANG_INFO). */
bool known_lang (const std::string &code);
/** The launcher's language for a tag such as "pt-BR" or "zh-Hans-CN" (English when it has none). */
std::string lang_from_tag (const std::string &tag);

/** "Sunday" */
std::string day_name (const std::string &lang, const std::tm &t);
/** "Sun", Sunday = 0 */
std::string day_short (const std::string &lang, int wday);
/** "October" (a heading) */
std::string month_name (const std::string &lang, int month);
/** "11 October" */
std::string date_text (const std::string &lang, const std::tm &t);
/** "11 October 2026" */
std::string date_year (const std::string &lang, const std::tm &t);

struct TimeParts { std::string time, period; };
/** "15:42" (24h) or "3:42" and "PM" (12h) */
TimeParts time_parts (const std::tm &t, bool h24);
