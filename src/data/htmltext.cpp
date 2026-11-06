#include "data/htmltext.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <regex>

namespace {

const std::vector<std::string> DROP = {"style", "script", "sup", "table", "figure", "noscript", "math", "svg"};
const std::vector<std::string> DROP_KEEP_TABLES = {"style", "script", "sup", "figure", "noscript", "math", "svg"};
const std::vector<std::string> BLOCK = {"p", "div", "td", "th", "tr", "table", "h1", "h2", "h3", "h4", "h5", "h6", "br", "li", "ul", "ol", "dl",
	"dd", "dt", "center", "blockquote", "section", "tbody"};
const std::vector<std::string> VOID = {"br", "img", "hr", "input", "meta", "link", "wbr", "source", "area", "col", "embed"};

bool in (const std::vector<std::string> &list, const std::string &s) { return std::find(list.begin(), list.end(), s) != list.end(); }

bool drop_class (const std::string &cls) {
	static const std::regex re(R"(\b(mw-editsection|navbox|metadata|noprint|reference|mw-empty-elt|toc|thumb|mw-references-wrap|hatnote|ambox|q-original|q-comment)\b)");
	return !cls.empty() && std::regex_search(cls, re);
}

std::string utf8 (unsigned cp) {
	std::string out;
	if (cp < 0x80) out += (char)cp;
	else if (cp < 0x800) { out += (char)(0xC0 | (cp >> 6)); out += (char)(0x80 | (cp & 0x3F)); }
	else if (cp < 0x10000) { out += (char)(0xE0 | (cp >> 12)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
	else { out += (char)(0xF0 | (cp >> 18)); out += (char)(0x80 | ((cp >> 12) & 0x3F)); out += (char)(0x80 | ((cp >> 6) & 0x3F)); out += (char)(0x80 | (cp & 0x3F)); }
	return out;
}

struct Tag { std::string name; bool close; std::string attrs; size_t start, end; };

std::string lower (std::string s) { for (auto &c : s) c = (char)tolower((unsigned char)c); return s; }

// <(\/?)([a-zA-Z][a-zA-Z0-9]*)([^>]*)>  and comments, skipped
std::vector<Tag> tags (const std::string &h) {
	std::vector<Tag> out;
	size_t i = 0, n = h.size();
	while ((i = h.find('<', i)) != std::string::npos) {
		if (h.compare(i, 4, "<!--") == 0) {
			size_t e = h.find("-->", i + 4);
			if (e == std::string::npos) break;
			i = e + 3;
			continue;
		}
		size_t j = i + 1;
		bool close = false;
		if (j < n && h[j] == '/') { close = true; j++; }
		if (j >= n || !isalpha((unsigned char)h[j])) { i++; continue; }
		size_t ns = j;
		while (j < n && isalnum((unsigned char)h[j])) j++;
		std::string name = lower(h.substr(ns, j - ns));
		size_t gt = h.find('>', j);
		if (gt == std::string::npos) break;
		out.push_back({name, close, h.substr(j, gt - j), i, gt + 1});
		i = gt + 1;
	}
	return out;
}

std::string class_of (const std::string &attrs) {
	static const std::regex re(R"re(\bclass\s*=\s*("([^"]*)"|'([^']*)'))re", std::regex::icase);
	std::smatch m;
	if (!std::regex_search(attrs, m, re)) return "";
	return m[2].matched ? m[2].str() : m[3].str();
}

bool self_closing (const Tag &t) {
	if (in(VOID, t.name)) return true;
	size_t e = t.attrs.find_last_not_of(" \t\r\n");
	return e != std::string::npos && t.attrs[e] == '/';
}

// One pass over the HTML: onText gets every piece of text outside dropped elements, onTag every tag that is not dropped.
void walk (const std::string &html, const std::function<void(const Tag &, const std::string &)> &on_tag, const std::function<void(const std::string &)> &on_text,
		const std::vector<std::string> &drop) {
	auto list = tags(html);
	size_t at = 0;
	int dropping = 0;
	std::vector<std::string> stack;
	for (auto &tag : list) {
		if (!dropping && tag.start > at) on_text(html.substr(at, tag.start - at));
		at = tag.end;
		bool sc = self_closing(tag);
		if (dropping) {
			if (sc) continue;
			if (tag.close && tag.name == stack.back()) { stack.pop_back(); dropping--; }
			else if (!tag.close && tag.name == stack.back()) { stack.push_back(tag.name); dropping++; }
			continue;
		}
		std::string cls = tag.close ? "" : class_of(tag.attrs);
		if (!tag.close && !sc && (in(drop, tag.name) || drop_class(cls))) {
			dropping = 1;
			stack.clear();
			stack.push_back(tag.name);
			continue;
		}
		on_tag(tag, cls);
	}
	if (!dropping && at < html.size()) on_text(html.substr(at));
}

// JavaScript's \s: ASCII white space, no-break space, and the other Unicode spaces used in pages
size_t space_at (const std::string &s, size_t i) {
	unsigned char c = (unsigned char)s[i];
	if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') return 1;
	if (c == 0xC2 && i + 1 < s.size() && (unsigned char)s[i + 1] == 0xA0) return 2;            // U+00A0
	if (c == 0xE2 && i + 2 < s.size()) {
		unsigned char b = (unsigned char)s[i + 1], d = (unsigned char)s[i + 2];
		if (b == 0x80 && ((d >= 0x80 && d <= 0x8A) || d == 0xA8 || d == 0xA9 || d == 0xAF)) return 3;   // U+2000-200A, 2028, 2029, 202F
		if (b == 0x81 && d == 0x9F) return 3;                                                        // U+205F
	}
	if (c == 0xE3 && i + 2 < s.size() && (unsigned char)s[i + 1] == 0x80 && (unsigned char)s[i + 2] == 0x80) return 3;   // U+3000
	if (c == 0xEF && i + 2 < s.size() && (unsigned char)s[i + 1] == 0xBB && (unsigned char)s[i + 2] == 0xBF) return 3;   // U+FEFF
	return 0;
}

// runs of white space (but not new lines when `keep_lines`) to one space
std::string collapse (const std::string &s, bool keep_lines) {
	std::string out;
	bool space = false;
	for (size_t i = 0; i < s.size();) {
		size_t n = space_at(s, i);
		if (n && !(keep_lines && s[i] == '\n')) { space = true; i += n; continue; }
		if (space) out += ' ';
		space = false;
		out += s[i++];
	}
	if (space) out += ' ';
	return out;
}

std::string trim_spaces (const std::string &s) {
	size_t a = 0, b = s.size();
	while (a < b) { size_t n = space_at(s, a); if (!n) break; a += n; }
	while (b > a) {
		size_t k = b - 1;
		while (k > a && ((unsigned char)s[k] & 0xC0) == 0x80) k--;
		if (!space_at(s, k)) break;
		b = k;
	}
	return s.substr(a, b - a);
}

std::string tidy (const std::string &text) {
	std::string s = collapse(decode_entities(text), true);
	// " *\n *" → "\n"
	std::string out;
	for (size_t i = 0; i < s.size(); i++) {
		if (s[i] == '\n') {
			while (!out.empty() && out.back() == ' ') out.pop_back();
			out += '\n';
			while (i + 1 < s.size() && s[i + 1] == ' ') i++;
		} else out += s[i];
	}
	return trim_spaces(out);
}

// the headings an item is under, outermost first: a heading replaces those at its level and below
struct Sections {
	std::vector<std::string> levels;
	std::string set (const std::string &tag, const std::string &text) {
		int depth = std::max(0, atoi(tag.c_str() + 1) - 2);
		levels.resize(depth + 1);
		levels[depth] = text;
		std::string out;
		for (auto &l : levels) if (!l.empty()) out += (out.empty() ? "" : " / ") + l;
		return out;
	}
};

bool heading_tag (const std::string &n) { return n.size() == 2 && n[0] == 'h' && n[1] >= '1' && n[1] <= '6'; }

}  // namespace

std::string squeeze_spaces (const std::string &s) { return collapse(s, false); }

std::string decode_entities (const std::string &text) {
	static const std::pair<const char *, const char *> ENTITIES[] = {
		{"amp", "&"}, {"lt", "<"}, {"gt", ">"}, {"quot", "\""}, {"apos", "'"}, {"nbsp", "\xC2\xA0"}, {"ndash", "–"}, {"mdash", "—"},
		{"laquo", "«"}, {"raquo", "»"}, {"hellip", "…"}, {"rsquo", "’"}, {"lsquo", "‘"}, {"rdquo", "”"}, {"ldquo", "“"}, {"bdquo", "„"},
		{"middot", "·"}, {"shy", ""}};
	std::string out;
	for (size_t i = 0; i < text.size(); i++) {
		if (text[i] != '&') { out += text[i]; continue; }
		size_t semi = text.find(';', i + 1);
		if (semi == std::string::npos || semi - i > 12) { out += '&'; continue; }
		std::string name = text.substr(i + 1, semi - i - 1);
		bool ok = false;
		if (name.size() > 1 && name[0] == '#') {
			bool hex = name[1] == 'x' || name[1] == 'X';
			std::string digits = name.substr(hex ? 2 : 1);
			bool valid = !digits.empty() && std::all_of(digits.begin(), digits.end(), [hex] (char c) { return hex ? isxdigit((unsigned char)c) : isdigit((unsigned char)c); });
			if (valid) {
				unsigned long code = strtoul(digits.c_str(), nullptr, hex ? 16 : 10);
				if (code > 0 && code < 0x110000) { out += utf8((unsigned)code); ok = true; }
			}
		} else if (!name.empty() && std::all_of(name.begin(), name.end(), [] (char c) { return isalpha((unsigned char)c); })) {
			std::string l = lower(name);
			for (auto &e : ENTITIES) if (l == e.first) { out += e.second; ok = true; break; }
		}
		if (ok) i = semi;
		else out += '&';
	}
	return out;
}

std::vector<std::string> html_lines (const std::string &html, bool tables) {
	std::string out;
	walk(html, [&] (const Tag &t, const std::string &) { if (in(BLOCK, t.name)) out += '\n'; },
		[&] (const std::string &text) { out += collapse(text, false); }, tables ? DROP_KEEP_TABLES : DROP);
	std::string decoded = decode_entities(out);
	std::vector<std::string> lines;
	size_t i = 0;
	while (i <= decoded.size()) {
		size_t nl = decoded.find('\n', i);
		std::string line = decoded.substr(i, nl == std::string::npos ? std::string::npos : nl - i);
		line = trim_spaces(collapse(line, false));
		if (!line.empty()) lines.push_back(line);
		if (nl == std::string::npos) break;
		i = nl + 1;
	}
	return lines;
}

std::string html_to_text (const std::string &html) {
	std::string out;
	for (auto &l : html_lines(html)) out += (out.empty() ? "" : "\n") + l;
	return out;
}

std::vector<ListItem> top_level_items (const std::string &html) {
	std::vector<ListItem> items;
	int depth = 0;
	bool has_current = false, has_heading = false;
	std::string current, heading, level = "h2", section;
	Sections sections;
	walk(html, [&] (const Tag &t, const std::string &) {
		const std::string &n = t.name;
		if (heading_tag(n)) {
			if (!t.close) { has_heading = true; heading.clear(); level = n; }
			else if (has_heading) { section = sections.set(level, tidy(heading)); has_heading = false; }
			return;
		}
		if (n == "ul" || n == "ol" || n == "dl") { depth += t.close ? -1 : 1; if (depth < 0) depth = 0; return; }
		if (n == "li" && depth == 1) {
			if (!t.close) { has_current = true; current.clear(); }
			else if (has_current) { items.push_back({tidy(current), section}); has_current = false; }
			return;
		}
		if (n == "br" && has_current && depth == 1) current += '\n';
	}, [&] (const std::string &text) {
		if (has_heading) heading += text;
		else if (has_current && depth == 1) current += collapse(text, false);
	}, DROP);
	items.erase(std::remove_if(items.begin(), items.end(), [] (const ListItem &i) { return i.text.empty(); }), items.end());
	return items;
}

std::vector<ListItem> text_of_class (const std::string &html, const std::string &name, bool tables) {
	std::vector<ListItem> out;
	std::regex re("(^|\\s)" + name + "(\\s|$)");
	std::vector<std::string> stack;
	bool has_current = false, has_heading = false;
	std::string current, heading, level = "h2", section;
	Sections sections;
	walk(html, [&] (const Tag &t, const std::string &cls) {
		if (heading_tag(t.name) && !has_current) {
			if (!t.close) { has_heading = true; heading.clear(); level = t.name; }
			else if (has_heading) { section = sections.set(level, tidy(heading)); has_heading = false; }
			return;
		}
		if (in(VOID, t.name)) { if (t.name == "br" && has_current) current += '\n'; return; }
		if (!has_current) {
			if (!t.close && std::regex_search(cls, re)) { has_current = true; current.clear(); stack.clear(); stack.push_back(t.name); }
			return;
		}
		if (!t.close) { stack.push_back(t.name); if (in(BLOCK, t.name)) current += '\n'; return; }
		if (!stack.empty() && t.name == stack.back()) stack.pop_back();
		if (stack.empty()) { out.push_back({tidy(current), section}); has_current = false; }
	}, [&] (const std::string &text) {
		if (has_heading) heading += text;
		else if (has_current) current += collapse(text, false);
	}, tables ? DROP_KEEP_TABLES : DROP);
	out.erase(std::remove_if(out.begin(), out.end(), [] (const ListItem &i) { return i.text.empty(); }), out.end());
	return out;
}
