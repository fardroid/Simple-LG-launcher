#include "core/unicode.h"

std::wstring widen (const std::string &s) {
	std::wstring out;
	for (size_t i = 0; i < s.size();) {
		unsigned char c = (unsigned char)s[i];
		unsigned cp;
		int n;
		if (c < 0x80) { cp = c; n = 1; }
		else if ((c >> 5) == 6) { cp = c & 0x1F; n = 2; }
		else if ((c >> 4) == 14) { cp = c & 0x0F; n = 3; }
		else if ((c >> 3) == 30) { cp = c & 0x07; n = 4; }
		else { i++; continue; }
		for (int k = 1; k < n && i + k < s.size(); k++) cp = (cp << 6) | ((unsigned char)s[i + k] & 0x3F);
		out += (wchar_t)cp;
		i += n;
	}
	return out;
}

std::string narrow (const std::wstring &w) {
	std::string out;
	for (wchar_t wc : w) {
		unsigned c = (unsigned)wc;
		if (c < 0x80) out += (char)c;
		else if (c < 0x800) { out += (char)(0xC0 | (c >> 6)); out += (char)(0x80 | (c & 0x3F)); }
		else if (c < 0x10000) { out += (char)(0xE0 | (c >> 12)); out += (char)(0x80 | ((c >> 6) & 0x3F)); out += (char)(0x80 | (c & 0x3F)); }
		else { out += (char)(0xF0 | (c >> 18)); out += (char)(0x80 | ((c >> 12) & 0x3F)); out += (char)(0x80 | ((c >> 6) & 0x3F)); out += (char)(0x80 | (c & 0x3F)); }
	}
	return out;
}

wchar_t lower_char (wchar_t c) {
	if (c >= 'A' && c <= 'Z') return c + 32;
	if (c >= 0xC0 && c <= 0xDE && c != 0xD7) return c + 32;                 // Latin-1
	if (c >= 0x410 && c <= 0x42F) return c + 32;                            // Cyrillic А-Я
	if (c >= 0x400 && c <= 0x40F) return c + 80;                            // Ѐ-Џ (Ё, Є, І, Ї...)
	if (c >= 0x391 && c <= 0x3A9 && c != 0x3A2) return c + 32;              // Greek
	// pairs of a capital and its small letter: the capital first (even) or second (odd), by range
	if (((c >= 0x100 && c <= 0x137) || (c >= 0x14A && c <= 0x177) || (c >= 0x460 && c <= 0x481) || (c >= 0x48A && c <= 0x4BF) || (c >= 0x4D0 && c <= 0x4FF)) && !(c & 1)) return c + 1;
	if (((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E) || (c >= 0x4C1 && c <= 0x4CE)) && (c & 1)) return c + 1;
	return c;
}

wchar_t upper_char (wchar_t c) {
	if (c >= 'a' && c <= 'z') return c - 32;
	if (c >= 0xE0 && c <= 0xFE && c != 0xF7) return c - 32;
	if (c >= 0x430 && c <= 0x44F) return c - 32;
	if (c >= 0x450 && c <= 0x45F) return c - 80;
	if (c >= 0x3B1 && c <= 0x3C9 && c != 0x3C2) return c - 32;
	if (((c >= 0x100 && c <= 0x137) || (c >= 0x14A && c <= 0x177) || (c >= 0x460 && c <= 0x481) || (c >= 0x48A && c <= 0x4BF) || (c >= 0x4D0 && c <= 0x4FF)) && (c & 1) && c != 0x131) return c - 1;
	if (((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E) || (c >= 0x4C1 && c <= 0x4CE)) && !(c & 1)) return c - 1;
	return c;
}

std::wstring lower (const std::wstring &s) {
	std::wstring out = s;
	for (auto &c : out) c = lower_char(c);
	return out;
}

std::string lower_utf8 (const std::string &s) { return narrow(lower(widen(s))); }
