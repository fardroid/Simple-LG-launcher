#include "sys/luna.h"

#include "core/log.h"
#include "core/tasks.h"

#include <nlohmann/json.hpp>
#include <webos-helpers/libhelpers.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

using json = nlohmann::json;

namespace {

// One call in flight. The HContext must come first: libhelpers hands it back as the callback's ctx.
// Who frees it: whoever touches it last, and only once outside its lock.
//  - async call (luna_call): its timeout thread, once the answer came; on a timeout, the late answer (or never: a few bytes);
//  - sync call (luna_call_sync): the caller once the answer came; on a timeout, the late answer;
//  - subscription: never (it lasts as long as the app).
struct Call {
	HContext ctx{};
	std::mutex m;
	std::condition_variable cv;
	bool done = false, gave_up = false, multiple = false;
	std::string payload;
	std::function<void(const std::string &)> on_main;
};

std::string failure (const std::string &why) { return json({{"returnValue", false}, {"errorText", why}}).dump(); }

bool on_message (LSHandle *, LSMessage *msg, void *ctx) {
	auto *c = reinterpret_cast<Call *>(ctx);
	const char *raw = HLunaServiceMessage(msg);
	std::string text = raw ? raw : "";
	if (c->multiple) {
		auto fn = c->on_main;
		post_main([fn, text] { fn(text); });
		return true;
	}
	bool free_it = false;
	{
		std::lock_guard<std::mutex> l(c->m);
		if (c->done) return true;
		c->done = true;
		if (c->gave_up) free_it = true;            // nobody waits any more
		else {
			c->payload = text;
			if (c->on_main) { auto fn = c->on_main; post_main([fn, text] { fn(text); }); }
			c->cv.notify_all();
		}
	}
	if (free_it) delete c;
	return true;
}

}  // namespace

void luna_call (const std::string &uri, const std::string &payload, std::function<void(const std::string &)> done, int timeout_ms) {
	auto *c = new Call();
	c->ctx.callback = on_message;
	c->ctx.pub = 1;
	c->on_main = done;
	if (HLunaServiceCall(uri.c_str(), payload.c_str(), &c->ctx) != 0) {
		logf("luna: %s refused", uri.c_str());
		delete c;
		post_main([done] { done(failure("call refused")); });
		return;
	}
	std::thread([c, done, timeout_ms, uri] {
		bool answered;
		{
			std::unique_lock<std::mutex> l(c->m);
			answered = c->cv.wait_for(l, std::chrono::milliseconds(timeout_ms), [c] { return c->done; });
			if (!answered) c->gave_up = true;
		}
		if (answered) { delete c; return; }
		logf("luna: %s timed out", uri.c_str());
		post_main([done] { done(failure("timeout")); });
	}).detach();
}

void luna_subscribe (const std::string &uri, const std::string &payload, std::function<void(const std::string &)> each) {
	auto *c = new Call();
	c->ctx.callback = on_message;
	c->ctx.pub = 1;
	c->ctx.multiple = 1;
	c->multiple = true;
	c->on_main = std::move(each);
	if (HLunaServiceCall(uri.c_str(), payload.c_str(), &c->ctx) != 0) {
		logf("luna: subscribe %s refused", uri.c_str());
		delete c;
	}
}

std::string luna_call_sync (const std::string &uri, const std::string &payload, int timeout_ms) {
	auto *c = new Call();
	c->ctx.callback = on_message;
	c->ctx.pub = 1;
	if (HLunaServiceCall(uri.c_str(), payload.c_str(), &c->ctx) != 0) { delete c; return failure("call refused"); }
	std::string out;
	bool answered;
	{
		std::unique_lock<std::mutex> l(c->m);
		answered = c->cv.wait_for(l, std::chrono::milliseconds(timeout_ms), [c] { return c->done; });
		if (answered) out = c->payload;
		else c->gave_up = true;
	}
	if (!answered) { logf("luna: %s timed out", uri.c_str()); return failure("timeout"); }
	delete c;
	return out;
}

std::string hbc_exec (const std::string &command, int timeout_ms, bool *ok, std::string *why) {
	std::string answer = luna_call_sync("luna://org.webosbrew.hbchannel.service/exec", json({{"command", command}}).dump(), timeout_ms);
	json r = json::parse(answer, nullptr, false);
	bool fine = r.is_object() && r.value("returnValue", false);
	if (ok) *ok = fine;
	// what the bus or the service said when it did not run (the root state in Settings is told from it)
	if (why) *why = fine ? "" : r.is_object() ? r.value("errorText", answer) : answer;
	if (!fine) {
		logf("hbc exec failed: %s", answer.substr(0, 200).c_str());
		return "";
	}
	return r.value("stdoutString", std::string());
}

std::string shell_quote (const std::string &value) {
	std::string out = "'";
	for (char ch : value) {
		if (ch == '\'') out += "'\\''";
		else out += ch;
	}
	return out + "'";
}
