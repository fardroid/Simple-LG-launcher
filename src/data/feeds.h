// The news card's data (lib/feeds.ts, lib/feedsload.ts, lib/webfetch.ts): headlines from Google News for the chosen country (in the
// launcher's language when the country has an edition in it) and from the user's own feeds. RSS 2.0, RSS 1.0 and Atom are read;
// the feeds are joined into one list, newest first, without repeats.
#pragma once

#include <optional>
#include <string>
#include <vector>

struct FeedItem {
	std::string title, link, source, summary;
	long long at = 0;            // when it was published, ms (0: not given)
	int feed = 0;
};

struct NewsData { std::string key; double at = 0; std::vector<FeedItem> items; };

constexpr long long NEWS_MAX_AGE_MS = 48LL * 3600000;
constexpr int NEWS_MAX_ITEMS = 50;
constexpr double NEWS_FRESH_MS = 25 * 60000.0;

/** The country in use: the chosen one; else the TV's region when it has an edition in the launcher's language, else the
 *  language's own country. `tag` is the TV's language tag ("ru-RU"). */
std::string country_of (const std::string &chosen, const std::string &tag, const std::string &lang);
std::string country_name (const std::string &code, const std::string &lang);
/** The language a country's headlines come in (the launcher's when the country has an edition in it), named in `lang`. */
std::string edition_language (const std::string &code, const std::string &lang);
std::string google_news_url (const std::string &country, const std::string &lang);
std::vector<std::string> news_countries ();
/** The same, in the alphabetical order of their names in `lang`. */
std::vector<std::string> news_countries_by_name (const std::string &lang);

std::vector<FeedItem> parse_feed (const std::string &xml, int feed);
std::vector<FeedItem> join_feeds (const std::vector<std::vector<FeedItem>> &lists, long long now);
/** How many headlines fit on a page of a card `height` px tall at text size `k`. */
int headlines_per_page (double height, double k);
/** "12 min ago": the number and its unit ("min", "h", "d"); unit "" when the date is not known. */
std::pair<long, std::string> age_of (long long at, long long now);
/** A date as feeds write it (RFC 822 "Tue, 03 Nov 2026 08:00:00 GMT", or ISO 8601), in ms; 0 when unreadable. */
long long parse_date (const std::string &s);
/** Bytes in the encoding they say they are in (Content-Type, else the XML declaration, else UTF-8), as UTF-8. */
std::string decode_text (const std::string &bytes, const std::string &content_type);

std::string news_json (const NewsData &d);
std::optional<NewsData> news_from_json (const std::string &raw);
bool news_fresh (const std::optional<NewsData> &d, const std::string &key, double now);

/** Fetches the feeds one after another (worker thread); a feed that cannot be read is left out. */
std::optional<NewsData> fetch_news (const std::vector<std::string> &urls, const std::string &key, std::string *why);
