// The quote of the day from Wikiquote, in the launcher's language (lib/quote.ts, lib/quoteload.ts): the editors' quote where the
// language's Wikiquote has one, else one picked by the date from the page of one of about 400 well-known authors (a Wikidata
// query, kept for a month). The whole day shows the same quote.
#pragma once

#include <optional>
#include <string>
#include <vector>

struct Quote { std::string text, author, lang, day, kind; int rules = 0; };
constexpr int QUOTE_RULES = 2;
constexpr double AUTHORS_FRESH_MS = 30 * 86400000.0;

struct QuoteFound { std::string text, author; };

/** Whether the language has an editors' quote of the day (and so is not always picked by the date). */
bool has_daily (const std::string &lang);
std::string parse_url (const std::string &lang, const std::string &page);
std::string feed_url (const std::string &lang);
std::optional<std::string> parse_reply (const std::string &json);
std::string day_key (long long now);
long long quote_day_number (long long now);
std::string clean_quote (const std::string &raw, const std::string &lang);
std::string drop_source (const std::string &text);
std::string author_name (const std::string &title);
bool skipped_section (const std::string &section, const std::string &author = "");
std::optional<QuoteFound> daily_from_page (const std::string &html, const std::string &lang);
std::optional<QuoteFound> daily_from_feed (const std::string &xml, const std::string &lang);
std::vector<std::string> quotes_from_page (const std::string &html, const std::string &lang, const std::string &author = "");
std::string authors_query (const std::string &lang);
std::string authors_url (const std::string &lang);
std::vector<std::string> parse_authors (const std::string &json);
std::optional<std::string> pick_author (const std::vector<std::string> &authors, long long now, int attempt = 0);
std::optional<std::string> pick_quote (const std::vector<std::string> &quotes, long long now);

std::string quote_json (const Quote &q);
std::optional<Quote> quote_from_json (const std::string &raw);

/** Today's quote (worker thread). `authors_file`: where the list of authors is kept for a month. */
std::optional<Quote> fetch_quote (const std::string &lang, long long now, const std::string &authors_file, std::string *why);
