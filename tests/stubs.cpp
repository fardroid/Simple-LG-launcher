// What the tests on this computer use in place of the TV's services: no Luna, no Homebrew Channel.
#include "sys/luna.h"

std::string hbc_exec (const std::string &, int, bool *ok, std::string *why) { if (ok) *ok = false; if (why) *why = "timeout"; return ""; }
std::string luna_call_sync (const std::string &, const std::string &, int) { return R"({"returnValue":false})"; }
std::string shell_quote (const std::string &value) { return "'" + value + "'"; }

// Background jobs run at once on this computer; what they post for the main thread waits for run_posted().
#include "core/tasks.h"
#include <deque>
static std::deque<std::function<void()>> posted_jobs;
void run_background (std::function<void()> job) { job(); }
void post_main (std::function<void()> fn) { posted_jobs.push_back(std::move(fn)); }
void run_posted () { while (!posted_jobs.empty()) { auto f = std::move(posted_jobs.front()); posted_jobs.pop_front(); f(); } }
