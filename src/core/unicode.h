// UTF-8 text as code points (for regular expressions that must see letters, not bytes) and case for the scripts the launcher
// speaks: Latin, Cyrillic, Greek (the C locale's towlower knows only ASCII).
#pragma once

#include <string>

std::wstring widen (const std::string &utf8);
std::string narrow (const std::wstring &w);
wchar_t lower_char (wchar_t c);
wchar_t upper_char (wchar_t c);
std::wstring lower (const std::wstring &s);
std::string lower_utf8 (const std::string &s);
