// HTTP(S) through our own libcurl and OpenSSL, with the TV's certificates. A native app is not bound by CORS, so the weather,
// the calendars, the news and the quote are fetched directly (the web version needed the Homebrew Channel for most of them).
// Worker threads only: the call waits for the answer.
#pragma once

#include <functional>
#include <string>

struct HttpResult {
	bool ok = false;            // a reply with a 2xx status
	long status = 0;
	std::string body;
	std::string error;          // why it failed
	std::string content_type;
};

void http_init ();
HttpResult http_get (const std::string &url, long timeout_s = 20, size_t max_bytes = 8u << 20);
/** Downloads `url` into the file `path` (written as path + ".part" and moved into place when complete), with `progress` called
 *  now and then with the bytes so far and the total (0 while unknown); it returns false to stop the download. `body` stays empty. */
HttpResult http_download (const std::string &url, const std::string &path, const std::function<bool(long done, long total)> &progress,
	long timeout_s = 600, long max_bytes = 64l << 20);
/** A value made safe for a URL's query (encodeURIComponent). */
std::string url_encode (const std::string &s);
