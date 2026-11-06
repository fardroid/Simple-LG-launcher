#include "data/feeds.h"

#include "core/log.h"
#include "data/htmltext.h"
#include "sys/http.h"

#include <nlohmann/json.hpp>

#include <iconv.h>

#include <algorithm>
#include <chrono>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <ctime>
#include <set>

using json = nlohmann::json;

namespace {

struct Edition { const char *country, *lang, *hl, *ceid; };
struct CountryName { const char *code; const char *names[13]; };
#include "data/editions.inc"

struct Home { const char *lang, *country; };
const Home HOME_COUNTRY[] = {{"en", "US"}, {"ru", "RU"}, {"es", "ES"}, {"fr", "FR"}, {"de", "DE"}, {"it", "IT"}, {"pt", "PT"}, {"tr", "TR"},
	{"uk", "UA"}, {"zh", "CN"}, {"ja", "JP"}, {"ko", "KR"}, {"pl", "PL"}};

bool has_country (const std::string &c) {
	for (auto &e : EDITIONS) if (c == e.country) return true;
	return false;
}

const Edition &edition_for (const std::string &country, const std::string &lang) {
	std::string code = has_country(country) ? country : "US";
	const Edition *first = nullptr;
	for (auto &e : EDITIONS) {
		if (code != e.country) continue;
		if (lang == e.lang) return e;
		if (!first) first = &e;
	}
	return *first;
}

std::string lower (std::string s) { for (auto &c : s) c = (char)tolower((unsigned char)c); return s; }

// where a tag named `name` (any case) opens at or after `from`: "<name" followed by white space, ">" or "/"
size_t open_tag (const std::string &low, const std::string &name, size_t from) {
	std::string pat = "<" + name;
	for (size_t i = low.find(pat, from); i != std::string::npos; i = low.find(pat, i + 1)) {
		char c = i + pat.size() < low.size() ? low[i + pat.size()] : 0;
		if (c == '>' || c == '/' || isspace((unsigned char)c)) return i;
	}
	return std::string::npos;
}

// inner(block, names): the content of the first element of the first of these names that the block has
bool inner (const std::string &block, std::initializer_list<const char *> names, std::string &out) {
	std::string low = lower(block);
	for (const char *n : names) {
		size_t o = open_tag(low, n, 0);
		while (o != std::string::npos) {
			size_t gt = low.find('>', o);
			if (gt == std::string::npos) break;
			if (low[gt - 1] == '/') { o = open_tag(low, n, gt); continue; }         // <link .../> has no content
			size_t close = low.find(std::string("</") + n + ">", gt);
			if (close == std::string::npos) break;
			out = block.substr(gt + 1, close - gt - 1);
			return true;
		}
	}
	return false;
}

std::string unwrap_cdata (const std::string &s) {
	std::string out;
	size_t i = 0;
	while (true) {
		size_t a = s.find("<![CDATA[", i);
		if (a == std::string::npos) { out += s.substr(i); break; }
		size_t b = s.find("]]>", a + 9);
		if (b == std::string::npos) { out += s.substr(i); break; }
		out += s.substr(i, a - i) + s.substr(a + 9, b - a - 9);
		i = b + 3;
	}
	return out;
}

bool has_tag_start (const std::string &s) {
	for (size_t i = 0; i + 1 < s.size(); i++) if (s[i] == '<' && isalpha((unsigned char)s[i + 1])) return true;
	return false;
}

bool has_escaped_entity (const std::string &s) {
	for (const char *e : {"&amp;", "&lt;", "&gt;", "&quot;"}) if (s.find(e) != std::string::npos) return true;
	for (size_t i = s.find("&#"); i != std::string::npos; i = s.find("&#", i + 1)) {
		size_t j = i + 2;
		while (j < s.size() && isdigit((unsigned char)s[j])) j++;
		if (j > i + 2 && j < s.size() && s[j] == ';') return true;
	}
	return false;
}

// textOf: CDATA unwrapped, escaped HTML (a description) turned to plain text, on one line
std::string text_of (const std::string &raw) {
	if (raw.empty()) return "";
	std::string s = unwrap_cdata(raw);
	if ((s.find("&lt;") != std::string::npos || s.find("&#60;") != std::string::npos) && !has_tag_start(s)) s = decode_entities(s);
	std::string text = html_to_text(s);
	if (has_escaped_entity(text)) text = decode_entities(text);
	std::string flat = squeeze_spaces(text);
	size_t a = flat.find_first_not_of(' '), b = flat.find_last_not_of(' ');
	return a == std::string::npos ? "" : flat.substr(a, b - a + 1);
}

std::string attr (const std::string &tag, const char *name) {
	std::string low = lower(tag);
	std::string pat = name;
	for (size_t i = low.find(pat); i != std::string::npos; i = low.find(pat, i + 1)) {
		if (i > 0 && isalnum((unsigned char)low[i - 1])) continue;
		size_t j = i + pat.size();
		while (j < low.size() && isspace((unsigned char)low[j])) j++;
		if (j >= low.size() || low[j] != '=') continue;
		j++;
		while (j < low.size() && isspace((unsigned char)low[j])) j++;
		if (j >= low.size() || (low[j] != '"' && low[j] != '\'')) continue;
		char q = low[j];
		size_t e = tag.find(q, j + 1);
		if (e == std::string::npos) return "";
		return tag.substr(j + 1, e - j - 1);
	}
	return "";
}

std::string trim (const std::string &s) {
	size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
	return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

std::string link_of (const std::string &block) {
	std::string rss;
	if (inner(block, {"link"}, rss) && !trim(rss).empty() && trim(rss)[0] != '<') return decode_entities(trim(unwrap_cdata(rss)));
	// Atom: <link rel="alternate" href="…"/> (or the first link without a rel)
	std::string low = lower(block), best;
	for (size_t o = open_tag(low, "link", 0); o != std::string::npos; o = open_tag(low, "link", o + 1)) {
		size_t gt = block.find('>', o);
		if (gt == std::string::npos) break;
		std::string tag = block.substr(o, gt - o + 1);
		std::string href = attr(tag, "href");
		if (href.empty()) continue;
		std::string rel = attr(tag, "rel");
		if (rel.empty() || rel == "alternate") return decode_entities(href);
		if (best.empty()) best = decode_entities(href);
	}
	std::string guid;
	if (best.empty() && inner(block, {"guid"}, guid) && trim(guid).compare(0, 4, "http") == 0) return trim(guid);
	return best;
}

long long now_ms () {
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

// sameStory: the title in lower case without punctuation, for spotting the same story twice
std::string same_story (const std::string &title) {
	static const char *punct[] = {"“", "”", "‘", "’", "«", "»", "–", "—", "…"};
	std::string s = lower(title), out;
	for (size_t i = 0; i < s.size();) {
		bool p = false;
		for (const char *q : punct) { size_t n = strlen(q); if (s.compare(i, n, q) == 0) { p = true; i += n; break; } }
		if (!p) {
			char c = s[i];
			if (strchr(" \t\r\n.,:;!?'\"()[]-|", c)) { p = true; i++; }
		}
		if (p) { if (!out.empty() && out.back() != ' ') out += ' '; continue; }
		out += s[i++];
	}
	while (!out.empty() && out.back() == ' ') out.pop_back();
	return out;
}

}  // namespace

std::vector<std::string> news_countries () {
	std::vector<std::string> out;
	for (auto &e : EDITIONS) if (out.empty() || out.back() != e.country) out.push_back(e.country);
	return out;
}

std::vector<std::string> news_countries_by_name (const std::string &lang) {
	int li = 0;
	for (int i = 0; i < 13; i++) if (lang == NAME_LANGS[i]) li = i;
	return std::vector<std::string>(std::begin(COUNTRY_ORDER[li]), std::end(COUNTRY_ORDER[li]));
}

std::string country_of (const std::string &chosen, const std::string &tag, const std::string &lang) {
	if (has_country(chosen)) return chosen;
	std::string region;
	size_t dash = tag.find_first_of("-_");
	if (dash != std::string::npos && tag.size() >= dash + 3) {
		std::string r = tag.substr(dash + 1, 2);
		for (auto &c : r) c = (char)toupper((unsigned char)c);
		if (has_country(r)) region = r;
	}
	if (!region.empty())
		for (auto &e : EDITIONS) if (region == e.country && lang == e.lang) return region;
	for (auto &h : HOME_COUNTRY) if (lang == h.lang) return h.country;
	return region.empty() ? "US" : region;
}

std::string country_name (const std::string &code, const std::string &lang) {
	int li = 0;
	for (int i = 0; i < 13; i++) if (lang == NAME_LANGS[i]) li = i;
	for (auto &c : COUNTRY_NAMES) if (code == c.code) return c.names[li];
	return code;
}

std::string edition_language (const std::string &code, const std::string &lang) {
	std::string edition = edition_for(code, lang).lang;
	int li = 0;
	for (int i = 0; i < 13; i++) if (lang == NAME_LANGS[i]) li = i;
	for (auto &l : LANGUAGE_NAMES) if (edition == l.code) return l.names[li];
	return edition;
}

std::string google_news_url (const std::string &country, const std::string &lang) {
	std::string code = has_country(country) ? country : "US";
	const Edition &e = edition_for(code, lang);
	std::string ceid = e.ceid;
	size_t colon = ceid.find(':');
	if (colon != std::string::npos) ceid.replace(colon, 1, "%3A");
	return "https://news.google.com/rss?hl=" + std::string(e.hl) + "&gl=" + code + "&ceid=" + ceid;
}

std::vector<FeedItem> parse_feed (const std::string &xml, int feed) {
	std::vector<FeedItem> out;
	std::string low = lower(xml);
	size_t first_item = std::min(open_tag(low, "item", 0), open_tag(low, "entry", 0));
	std::string head = first_item == std::string::npos ? xml : xml.substr(0, first_item);
	std::string channel_raw;
	std::string channel = inner(head, {"title"}, channel_raw) ? text_of(channel_raw) : "";
	size_t at = 0;
	while (true) {
		size_t oi = open_tag(low, "item", at), oe = open_tag(low, "entry", at);
		size_t o = std::min(oi, oe);
		if (o == std::string::npos) break;
		const char *name = o == oi ? "item" : "entry";
		size_t close = low.find(std::string("</") + name + ">", o);
		if (close == std::string::npos) break;
		size_t end = close + strlen(name) + 3;
		std::string block = xml.substr(o, end - o);
		at = end;
		std::string raw;
		std::string title = inner(block, {"title"}, raw) ? text_of(raw) : "";
		if (title.empty()) continue;
		std::string source = inner(block, {"source"}, raw) ? text_of(raw) : "";
		// Google News writes the outlet after the headline too: "Headline - Outlet"
		std::string tail = " - " + source;
		if (!source.empty() && title.size() > tail.size() && title.compare(title.size() - tail.size(), tail.size(), tail) == 0) title = title.substr(0, title.size() - tail.size());
		if (source.empty()) source = channel;
		std::string summary;
		for (const char *n : {"description", "summary", "content:encoded", "content"}) if (inner(block, {n}, raw)) { summary = text_of(raw); break; }
		long long date = 0;
		for (const char *n : {"pubdate", "dc:date", "published", "updated", "a10:updated"}) if (inner(block, {n}, raw)) { date = parse_date(trim(raw)); break; }
		FeedItem item;
		item.title = title;
		item.link = link_of(block);
		item.at = date;
		item.source = source;
		if (summary != title) {
			// slice(0, 600) counts characters; cut on a character boundary
			size_t cut = 0, chars = 0;
			while (cut < summary.size() && chars < 600) { unsigned char c = (unsigned char)summary[cut]; cut += c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4; chars++; }
			item.summary = summary.substr(0, std::min(cut, summary.size()));
		}
		item.feed = feed;
		out.push_back(item);
	}
	return out;
}

std::vector<FeedItem> join_feeds (const std::vector<std::vector<FeedItem>> &lists, long long now) {
	std::set<std::string> seen;
	std::vector<FeedItem> all;
	for (auto &list : lists)
		for (auto &item : list) {
			if (item.at && (now - item.at > NEWS_MAX_AGE_MS || item.at - now > 3600000)) continue;
			std::vector<std::string> keys;
			if (!item.link.empty()) keys.push_back(item.link);
			std::string story = same_story(item.title);
			if (!story.empty()) keys.push_back(story);
			if (std::any_of(keys.begin(), keys.end(), [&] (const std::string &k) { return seen.count(k); })) continue;
			for (auto &k : keys) seen.insert(k);
			all.push_back(item);
		}
	std::stable_sort(all.begin(), all.end(), [] (const FeedItem &a, const FeedItem &b) { return a.at > b.at; });
	if ((int)all.size() > NEWS_MAX_ITEMS) all.resize(NEWS_MAX_ITEMS);
	return all;
}

int headlines_per_page (double height, double k) {
	double head = 44 * k;
	double item = 16 + 15 * k * 1.35 + 6 + 2 * 22 * k * 1.3 + 14;
	return std::max(1, (int)std::floor((height - 44 - head - 10) / item));
}

std::pair<long, std::string> age_of (long long at, long long now) {
	if (!at) return {0, ""};
	long minutes = std::max(1L, (long)std::lround((now - at) / 60000.0));
	if (minutes < 60) return {minutes, "min"};
	if (minutes < 48 * 60) return {(long)std::lround(minutes / 60.0), "h"};
	return {(long)std::lround(minutes / 1440.0), "d"};
}

long long parse_date (const std::string &s) {
	static const char *MONTHS[] = {"jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"};
	std::string t = trim(s);
	if (t.empty()) return 0;
	std::tm tm{};
	long long offset = 0;
	// ISO 8601: 2026-11-03T08:00:00Z, +01:00, .123
	if (t.size() >= 10 && isdigit((unsigned char)t[0]) && t[4] == '-') {
		int y, mo, d, h = 0, mi = 0;
		double sec = 0;
		if (sscanf(t.c_str(), "%d-%d-%d", &y, &mo, &d) != 3) return 0;
		size_t T = t.find_first_of("Tt ");
		if (T != std::string::npos) sscanf(t.c_str() + T + 1, "%d:%d:%lf", &h, &mi, &sec);
		tm.tm_year = y - 1900; tm.tm_mon = mo - 1; tm.tm_mday = d; tm.tm_hour = h; tm.tm_min = mi; tm.tm_sec = (int)sec;
		size_t z = T == std::string::npos ? std::string::npos : t.find_first_of("Zz+-", T + 1);
		if (z != std::string::npos && t[z] != 'Z' && t[z] != 'z') {
			int oh = 0, om = 0;
			sscanf(t.c_str() + z + 1, "%2d:%2d", &oh, &om);
			offset = (t[z] == '-' ? -1 : 1) * (oh * 3600LL + om * 60) * 1000;
		} else if (z == std::string::npos && T != std::string::npos) {
			// no zone with a time: local time, as Date.parse
			tm.tm_isdst = -1;
			return (long long)mktime(&tm) * 1000;
		}
		return (long long)timegm(&tm) * 1000 - offset;
	}
	// RFC 822: [Tue, ]03 Nov 2026 08:00[:00] GMT|+0100|EST
	std::string u = t;
	size_t comma = u.find(',');
	if (comma != std::string::npos) u = trim(u.substr(comma + 1));
	char mon[8] = {0}, zone[16] = {0};
	int d, y, h = 0, mi = 0, sec = 0;
	int n = sscanf(u.c_str(), "%d %7s %d %d:%d:%d %15s", &d, mon, &y, &h, &mi, &sec, zone);
	if (n < 6) {
		n = sscanf(u.c_str(), "%d %7s %d %d:%d %15s", &d, mon, &y, &h, &mi, zone);
		if (n < 5) return 0;
		sec = 0;
	}
	int m = -1;
	for (int i = 0; i < 12; i++) if (strncasecmp(mon, MONTHS[i], 3) == 0) m = i;
	if (m < 0) return 0;
	if (y < 100) y += y < 50 ? 2000 : 1900;
	tm.tm_year = y - 1900; tm.tm_mon = m; tm.tm_mday = d; tm.tm_hour = h; tm.tm_min = mi; tm.tm_sec = sec;
	std::string z = zone;
	if (!z.empty() && (z[0] == '+' || z[0] == '-') && z.size() >= 5) {
		int v = atoi(z.c_str() + 1);
		offset = (z[0] == '-' ? -1 : 1) * ((v / 100) * 3600LL + (v % 100) * 60) * 1000;
	} else {
		static const std::pair<const char *, int> NAMED[] = {{"EST", -5}, {"EDT", -4}, {"CST", -6}, {"CDT", -5}, {"MST", -7}, {"MDT", -6}, {"PST", -8}, {"PDT", -7}};
		for (auto &p : NAMED) if (z == p.first) offset = p.second * 3600000LL;
	}
	return (long long)timegm(&tm) * 1000 - offset;
}

std::string decode_text (const std::string &bytes, const std::string &type) {
	std::string charset;
	std::string lt = lower(type);
	size_t c = lt.find("charset=");
	if (c != std::string::npos) {
		size_t i = c + 8;
		while (i < lt.size() && (lt[i] == '"' || lt[i] == '\'')) i++;
		size_t j = i;
		while (j < lt.size() && (isalnum((unsigned char)lt[j]) || strchr("._-", lt[j]))) j++;
		charset = lt.substr(i, j - i);
	}
	if (charset.empty()) {
		std::string head = lower(bytes.substr(0, 200));
		if (head.compare(0, 3, "\xef\xbb\xbf") == 0) head = head.substr(3);
		size_t x = head.find("<?xml");
		size_t e = x == std::string::npos ? std::string::npos : head.find("encoding=", x);
		size_t end = x == std::string::npos ? std::string::npos : head.find("?>", x);
		if (e != std::string::npos && e < end) {
			size_t i = e + 10, j = i;
			while (j < head.size() && (isalnum((unsigned char)head[j]) || strchr("._-", head[j]))) j++;
			charset = head.substr(i, j - i);
		}
	}
	if (charset.empty() || charset == "utf-8" || charset == "utf8") return bytes;
	iconv_t cd = iconv_open("UTF-8", charset.c_str());
	if (cd == (iconv_t)-1) return bytes;             // an encoding the TV does not know: as UTF-8, as the web version
	std::string out(bytes.size() * 4 + 16, '\0');
	char *in = const_cast<char *>(bytes.data()), *o = &out[0];
	size_t inleft = bytes.size(), outleft = out.size();
	while (inleft > 0) {
		if (iconv(cd, &in, &inleft, &o, &outleft) == (size_t)-1) {
			if (errno == EILSEQ || errno == EINVAL) { in++; inleft--; continue; }      // a broken byte is skipped
			break;
		}
	}
	iconv_close(cd);
	out.resize(out.size() - outleft);
	return out;
}

std::string news_json (const NewsData &d) {
	json items = json::array();
	for (auto &i : d.items) items.push_back({{"title", i.title}, {"link", i.link}, {"at", i.at}, {"source", i.source}, {"summary", i.summary}, {"feed", i.feed}});
	return json({{"key", d.key}, {"at", d.at}, {"items", items}}).dump();
}

std::optional<NewsData> news_from_json (const std::string &raw) {
	json j = json::parse(raw, nullptr, false);
	if (!j.is_object() || !j["key"].is_string() || !j["items"].is_array()) return std::nullopt;
	NewsData d;
	d.key = j["key"];
	d.at = j.value("at", 0.0);
	for (auto &i : j["items"])
		if (i.is_object() && i["title"].is_string())
			d.items.push_back({i["title"], i.value("link", std::string()), i.value("source", std::string()), i.value("summary", std::string()), i.value("at", 0LL), i.value("feed", 0)});
	return d;
}

bool news_fresh (const std::optional<NewsData> &d, const std::string &key, double now) {
	return d && d->key == key && now - d->at < NEWS_FRESH_MS;
}

std::optional<NewsData> fetch_news (const std::vector<std::string> &urls, const std::string &key, std::string *why) {
	long long now = now_ms();
	std::vector<std::vector<FeedItem>> lists;
	int failed = 0;
	for (size_t i = 0; i < urls.size(); i++) {
		HttpResult r = http_get(urls[i], 15, 2u << 20);
		if (!r.ok) { failed++; continue; }
		auto items = parse_feed(decode_text(r.body, r.content_type), (int)i);
		if (items.empty()) failed++;
		else lists.push_back(items);
	}
	if (lists.empty()) { if (why) *why = failed ? "nothing could be read" : "no feeds"; return std::nullopt; }
	NewsData d;
	d.key = key;
	d.at = (double)now;
	d.items = join_feeds(lists, now);
	return d;
}
