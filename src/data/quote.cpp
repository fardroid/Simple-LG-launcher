#include "data/quote.h"

#include "core/unicode.h"
#include "data/htmltext.h"
#include "sys/fs.h"
#include "sys/http.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <regex>

using json = nlohmann::json;

namespace {

struct Daily { const char *lang, *page, *marker, *empty; bool feed; };
// where each language's quote of the day is; the others always pick by the date
const Daily DAILY[] = {
	{"en", "Main Page", "Quote of the day", nullptr, false}, {"es", "Portada", "Cita del día", nullptr, false},
	{"pl", "Strona główna", "Cytat dnia", nullptr, false}, {"zh", "Wikiquote:首页", "每日名言", nullptr, false},
	{"ko", "위키인용집:대문", "오늘의 명언", nullptr, false}, {"tr", "Ana Sayfa", "Günün Sözü", nullptr, false},
	{"pt", "Página principal", "Citação do Dia", nullptr, false}, {"uk", "Головна сторінка", "Цитата дня", "ще не обрано", false},
	{"it", nullptr, nullptr, nullptr, true}};

const Daily *daily_of (const std::string &lang) {
	for (auto &d : DAILY) if (lang == d.lang) return &d;
	return nullptr;
}

bool dense (const std::string &lang) { return lang == "zh" || lang == "ja"; }

using W = std::wstring;
using WR = std::wregex;

bool match (const W &s, const WR &re) { return std::regex_search(s, re); }
W replace (const W &s, const WR &re, const W &with) { return std::regex_replace(s, re, with); }

W trim (const W &s) {
	size_t a = 0, b = s.size();
	while (a < b && iswspace(s[a])) a++;
	while (b > a && iswspace(s[b - 1])) b--;
	return s.substr(a, b - a);
}

// the part names of the page that hold quotes about the author, doubtful ones, links or sources (SKIP_SECTION, in lower case)
const WR &skip_section_re () {
	static const WR re(L"^(" L"quotes? about|about |misattributed|disputed|attributed|see also|external links|sources|references|"
		L"о нём|о ней|об |высказывания о|цитаты о|сомнительн|ошибочно|приписыва|см\\. также|ссылки|источники|литература|"
		L"общие источники|статьи о|"
		L"zitate über|über |zitate mit bezug|fälschlich|zugeschrieben|unbelegt|siehe auch|weblinks|quellen|literatur|"
		L"citations le concernant|citations sur|à propos|attribu|apocryph|voir aussi|liens|références|bibliographie|"
		L".*について|.*に関する|.*誤って|関連|外部リンク|出典|参考|"
		L"про |цитати про|сумнівн|приписуван|див\\. також|посилання|джерела|загальні джерела|статті про|"
		L"sobre |citas sobre|atribuid|véase|enlaces|fuentes|veja também|ligações|citações sobre|"
		L".*hakkında|ayrıca|kaynak|o nim|o niej|przypisywane|zobacz też|linki|źródła|"
		L".*에 대한|관련|외부 링크|.*的名言|关于|出处|相关|外部链接|"
		L"su di|citazioni su|attribuit|voci correlate|collegamenti|fonti" L")");
	return re;
}

bool about_author (const W &part, const std::string &author) {
	static const WR about(L"^(?:о|об|обо|про|about|über|sur|sobre|su|o)\\s+(.+)$");
	std::wsmatch m;
	W low = lower(part);
	if (author.empty() || !std::regex_match(low, m, about)) return false;
	static const WR sep(L"[\\s,.\\-]+");
	auto words = [] (const W &s) {
		std::vector<W> out;
		std::wsregex_token_iterator it(s.begin(), s.end(), sep, -1), end;
		for (; it != end; ++it) out.push_back(*it);
		return out;
	};
	std::vector<W> stems;
	for (auto &w : words(lower(widen(author)))) if (w.size() >= 5) stems.push_back(w.substr(0, 5));
	for (auto &w : words(m[1].str())) if (std::find(stems.begin(), stems.end(), w.substr(0, 5)) != stems.end()) return true;
	return false;
}

bool fits (const W &text, const std::string &lang) {
	size_t n = text.size();
	if (dense(lang) ? n < 6 || n > 90 : n < 25 || n > 220) return false;
	static const WR cross(L"^(?:см\\.|смотри|див\\.|дивись|see |cf\\.|vgl\\.|siehe |voir |véase |vedi |zob\\.|категория:|категорія:|category:|kategorie:|catégorie:)");
	static const WR cut(L"[,:;]$");
	W low = lower(text);
	if (match(low, cross) || match(text, cut)) return false;
	static const WR refs(L"https?:|isbn|\\bp\\.\\s?\\d|\\bs\\.\\s?\\d|\\bed\\.|éd\\.|trad\\.|\\(\\d{4}\\)|^\\d{4}\\b");
	return !match(low, refs);
}

std::optional<QuoteFound> daily_from_lines (const std::vector<std::string> &lines, const Daily &d, const std::string &lang) {
	size_t i = 0;
	if (d.marker) {
		W marker = lower(widen(d.marker));
		size_t found = lines.size();
		for (size_t k = 0; k < lines.size(); k++) {
			W l = lower(widen(lines[k]));
			if (l == marker || (l.compare(0, marker.size(), marker) == 0 && l.size() <= marker.size() + 3)) { found = k; break; }
		}
		if (found == lines.size()) return std::nullopt;
		i = found + 1;
	}
	static const WR marks(L"^[\\s“”\"„«»‘’‚‹›'「」『』〝〞⹂]*$");
	auto next = [&] () -> W {
		while (i < lines.size() && (match(widen(lines[i]), marks) || lines[i].compare(0, 17, ".mw-parser-output") == 0)) i++;
		return i < lines.size() ? widen(lines[i++]) : W();
	};
	W text = next();
	if (text.empty() || (d.empty && text.find(widen(d.empty)) != W::npos)) return std::nullopt;
	W author;
	static const WR dash_tail(L"\\s*(?:——|—|–|~|\\s-)\\s*([^—–~\\n]{2,80})$");
	std::wsmatch m;
	if (std::regex_search(text, m, dash_tail) && text.size() - m[0].length() > 3) {
		author = trim(m[1].str());
		text = text.substr(0, text.size() - m[0].length());
	} else {
		W line = next();
		static const WR lead(L"^[\\s—–~\\-]+");
		author = trim(replace(line, lead, L""));
	}
	W cleaned = widen(clean_quote(narrow(text), lang));
	if (cleaned.size() < 3 || cleaned.size() > 600 || author.size() > 120 || author.empty()) return std::nullopt;
	return QuoteFound{narrow(cleaned), narrow(author)};
}

std::string ci_lower (std::string s) { for (auto &c : s) c = (char)tolower((unsigned char)c); return s; }

// The table cells of the original-language text (grey italic) taken out of a page: <td … font-style: italic …> … </td>
std::string without_originals (const std::string &html) {
	std::string low = ci_lower(html), out;
	size_t at = 0;
	for (size_t i = low.find("<td", 0); i != std::string::npos; i = low.find("<td", i + 1)) {
		if (i < at) continue;
		size_t gt = low.find('>', i);
		if (gt == std::string::npos) break;
		std::string attrs = low.substr(i, gt - i);
		size_t fs = attrs.find("font-style:");
		if (fs == std::string::npos) continue;
		size_t j = fs + 11;
		while (j < attrs.size() && isspace((unsigned char)attrs[j])) j++;
		if (attrs.compare(j, 6, "italic") != 0) continue;
		size_t close = low.find("</td>", gt);
		if (close == std::string::npos) break;
		out += html.substr(at, i - at);
		at = close + 5;
		i = at - 1;
	}
	return out + html.substr(at);
}

long long now_ms () {
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

}  // namespace

bool has_daily (const std::string &lang) { return daily_of(lang) != nullptr; }

std::string parse_url (const std::string &lang, const std::string &page) {
	return "https://" + lang + ".wikiquote.org/w/api.php?action=parse&format=json&formatversion=2&prop=text&redirects=1&origin=*&page=" + url_encode(page);
}

std::string feed_url (const std::string &lang) {
	return "https://" + lang + ".wikiquote.org/w/api.php?action=featuredfeed&feed=qotd&feedformat=atom&origin=*";
}

std::optional<std::string> parse_reply (const std::string &text) {
	json j = json::parse(text, nullptr, false);
	if (j.is_object() && j["parse"].is_object() && j["parse"]["text"].is_string()) return j["parse"]["text"].get<std::string>();
	return std::nullopt;
}

std::string day_key (long long now) {
	std::time_t t = (std::time_t)(now / 1000);
	std::tm d;
	localtime_r(&t, &d);
	char b[48];
	snprintf(b, sizeof b, "%04d-%02d-%02d", d.tm_year + 1900, d.tm_mon + 1, d.tm_mday);
	return b;
}

long long quote_day_number (long long now) {
	std::time_t t = (std::time_t)(now / 1000);
	std::tm d;
	localtime_r(&t, &d);
	std::tm u{};
	u.tm_year = d.tm_year; u.tm_mon = d.tm_mon; u.tm_mday = d.tm_mday;
	return (long long)timegm(&u) / 86400;
}

std::string clean_quote (const std::string &raw, const std::string &lang) {
	W text = widen(raw);
	text = replace(text, WR(L"\\s*//\\s*"), L"\n");
	text = replace(text, WR(L"\\[\\d+\\]"), L"");
	text = trim(replace(text, WR(L"[ \\t]+"), L" "));
	if (lang == "ja") text = replace(text, WR(L"\\s*[（(]\\s*[ぁ-ゖー\\s]+\\s*[）)]"), L"");
	text = replace(text, WR(L"\\s*(?:--|——)[^\\n]{0,40}$"), L"");
	std::wsmatch m;
	static const WR quoted(L"^[„\"“«‚‹「『]([\\s\\S]+?)[“\"”»‘›„」』]\\s*(?:[–—\\-]\\s*[^\\n]{0,80})?$");
	if (std::regex_match(text, m, quoted)) text = trim(m[1].str());
	text = trim(replace(text, WR(L"^[„\"“«「『]+|[“\"”»„」』]+$"), L""));
	// a mark taken off the end that closed a quote inside the text ("… спрашиваю «почему нет?»") goes back
	auto count = [&] (wchar_t c) { return std::count(text.begin(), text.end(), c); };
	if (count(L'«') > count(L'»')) text += L'»';
	if (count(L'「') > count(L'」')) text += L'」';
	return narrow(text);
}

std::string drop_source (const std::string &raw) {
	W text = widen(raw);
	static const WR bracket(L"([.!?…»])\\s+(\\((?:[^()\\n]|\\([^()\\n]*\\))*\\))$");
	std::wsmatch m;
	if (std::regex_search(text, m, bracket) && match(m[2].str(), WR(L"\\d"))) text = text.substr(0, m.position(0) + 1);
	text = replace(text, WR(L"([.!?…»])\\s+—\\s+[^\\n]+$"), L"$1");
	text = replace(text, WR(L"^—\\s+"), L"");
	return narrow(text);
}

std::string author_name (const std::string &title) {
	return narrow(trim(replace(widen(title), WR(L"\\s*\\([^)]*\\)\\s*$"), L"")));
}

bool skipped_section (const std::string &section, const std::string &author) {
	static const WR person(L"^(?:О|Об|Про) [А-ЯЁІЇЄҐ]\\S* [А-ЯЁІЇЄҐ]");
	W s = widen(section);
	size_t i = 0;
	while (i <= s.size()) {
		size_t sep = s.find(L" / ", i);
		W part = trim(s.substr(i, sep == W::npos ? W::npos : sep - i));
		if (match(lower(part), skip_section_re()) || match(part, person) || about_author(part, author)) return true;
		if (sep == W::npos) break;
		i = sep + 3;
	}
	return false;
}

std::optional<QuoteFound> daily_from_page (const std::string &html, const std::string &lang) {
	const Daily *d = daily_of(lang);
	if (!d || d->feed) return std::nullopt;
	return daily_from_lines(html_lines(html, true), *d, lang);
}

std::optional<QuoteFound> daily_from_feed (const std::string &xml, const std::string &lang) {
	std::string low = ci_lower(xml);
	size_t last = low.rfind("<entry>");
	if (last == std::string::npos) return std::nullopt;
	std::string piece = xml.substr(last), lp = low.substr(last);
	std::string body;
	for (const char *name : {"summary", "content"}) {
		size_t o = lp.find(std::string("<") + name);
		if (o == std::string::npos) continue;
		size_t gt = lp.find('>', o), close = lp.find(std::string("</") + name + ">", o);
		if (gt == std::string::npos || close == std::string::npos) continue;
		body = piece.substr(gt + 1, close - gt - 1);
		break;
	}
	if (body.empty()) return std::nullopt;
	return daily_from_lines(html_lines(decode_entities(body), true), Daily{lang.c_str(), nullptr, nullptr, nullptr, true}, lang);
}

std::vector<std::string> quotes_from_page (const std::string &html, const std::string &lang, const std::string &author) {
	// Russian and Ukrainian pages keep many quotes in boxes of the "Q" template (table rows), not in lists
	bool boxes = lang == "ru" || lang == "uk";
	std::vector<ListItem> items;
	if (lang == "fr") items = text_of_class(html, "citation");
	else if (boxes) {
		items = top_level_items(html);
		auto q = text_of_class(html, "q-text", true), poems = text_of_class(without_originals(html), "poem", true);
		items.insert(items.end(), q.begin(), q.end());
		items.insert(items.end(), poems.begin(), poems.end());
	} else items = top_level_items(html);
	std::vector<std::string> out;
	for (auto &item : items) {
		if (!item.section.empty() && skipped_section(item.section, author)) continue;
		std::string text = drop_source(clean_quote(item.text, lang));
		if (fits(widen(text), lang) && std::find(out.begin(), out.end(), text) == out.end()) out.push_back(text);
	}
	return out;
}

std::string authors_query (const std::string &lang) {
	const char *jobs = "wd:Q36180 wd:Q49757 wd:Q4964182 wd:Q6625963 wd:Q214917 wd:Q901 wd:Q169470 wd:Q1028181 wd:Q36834";
	return "SELECT DISTINCT ?title ?n WHERE { ?a schema:isPartOf <https://" + lang + ".wikiquote.org/>; schema:about ?p; schema:name ?title. "
		"?p wdt:P31 wd:Q5; wikibase:sitelinks ?n; wdt:P569 ?born; wdt:P106 ?job. VALUES ?job { " + jobs + " } FILTER(YEAR(?born) < 1920) } ORDER BY DESC(?n) LIMIT 400";
}

std::string authors_url (const std::string &lang) { return "https://query.wikidata.org/sparql?format=json&query=" + url_encode(authors_query(lang)); }

std::vector<std::string> parse_authors (const std::string &text) {
	std::vector<std::string> out;
	json j = json::parse(text, nullptr, false);
	if (!j.is_object() || !j["results"].is_object() || !j["results"]["bindings"].is_array()) return out;
	for (auto &r : j["results"]["bindings"]) {
		if (!r.is_object() || !r["title"].is_object() || !r["title"]["value"].is_string()) continue;
		std::string t = r["title"]["value"];
		if (!t.empty() && std::find(out.begin(), out.end(), t) == out.end()) out.push_back(t);
	}
	return out;
}

std::optional<std::string> pick_author (const std::vector<std::string> &authors, long long now, int attempt) {
	if (authors.empty()) return std::nullopt;
	return authors[(size_t)((quote_day_number(now) * 7919 + attempt * 104729LL) % (long long)authors.size())];
}

std::optional<std::string> pick_quote (const std::vector<std::string> &quotes, long long now) {
	if (quotes.empty()) return std::nullopt;
	return quotes[(size_t)((quote_day_number(now) * 2671) % (long long)quotes.size())];
}

std::string quote_json (const Quote &q) {
	return json({{"text", q.text}, {"author", q.author}, {"lang", q.lang}, {"day", q.day}, {"kind", q.kind}, {"rules", q.rules}}).dump();
}

std::optional<Quote> quote_from_json (const std::string &raw) {
	json j = json::parse(raw, nullptr, false);
	if (!j.is_object() || !j["text"].is_string() || !j["author"].is_string() || !j["day"].is_string() || j.value("rules", 0) != QUOTE_RULES) return std::nullopt;
	return Quote{j["text"], j["author"], j.value("lang", std::string()), j["day"], j.value("kind", std::string()), QUOTE_RULES};
}

std::optional<Quote> fetch_quote (const std::string &lang, long long now, const std::string &authors_file, std::string *why) {
	std::string day = day_key(now);
	auto get = [] (const std::string &url) { return http_get(url, 20, 3u << 20); };
	// the editors' quote, where there is one
	if (const Daily *d = daily_of(lang)) {
		std::optional<QuoteFound> found;
		if (d->feed) { auto r = get(feed_url(lang)); if (r.ok) found = daily_from_feed(r.body, lang); }
		else { auto r = get(parse_url(lang, d->page)); auto html = r.ok ? parse_reply(r.body) : std::nullopt; if (html) found = daily_from_page(*html, lang); }
		if (found) return Quote{found->text, found->author, lang, day, "daily", QUOTE_RULES};
	}
	// else one picked by the date: the authors (kept for a month), then a quote from the day's author's page
	std::vector<std::string> authors;
	std::string saved;
	if (read_file(authors_file, saved)) {
		json j = json::parse(saved, nullptr, false);
		if (j.is_object() && j.value("lang", std::string()) == lang && now - j.value("at", 0.0) < AUTHORS_FRESH_MS && j["titles"].is_array())
			for (auto &t : j["titles"]) if (t.is_string()) authors.push_back(t);
	}
	if (authors.empty()) {
		auto r = http_get(authors_url(lang), 60, 3u << 20);
		if (r.ok) authors = parse_authors(r.body);
		if (authors.empty()) { if (why) *why = "no authors"; return std::nullopt; }
		write_file(authors_file, json({{"lang", lang}, {"at", (double)now}, {"titles", authors}}).dump());
	}
	for (int attempt = 0; attempt < 3; attempt++) {
		auto title = pick_author(authors, now, attempt);
		if (!title) break;
		auto r = get(parse_url(lang, *title));
		auto html = r.ok ? parse_reply(r.body) : std::nullopt;
		if (!html) continue;
		auto q = pick_quote(quotes_from_page(*html, lang, author_name(*title)), now);
		if (q) return Quote{*q, author_name(*title), lang, day, "picked", QUOTE_RULES};
	}
	if (why) *why = "nothing to quote";
	(void)now_ms;
	return std::nullopt;
}
