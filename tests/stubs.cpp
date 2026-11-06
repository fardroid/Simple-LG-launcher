// What the tests on this computer use in place of the TV's services: no Luna, no Homebrew Channel.
#include "sys/luna.h"

std::string hbc_exec (const std::string &, int, bool *ok, std::string *why) { if (ok) *ok = false; if (why) *why = "timeout"; return ""; }
std::string luna_call_sync (const std::string &, const std::string &, int) { return R"({"returnValue":false})"; }
std::string shell_quote (const std::string &value) { return "'" + value + "'"; }
