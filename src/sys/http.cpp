#include "sys/http.h"

#include "core/log.h"

#include <curl/curl.h>

#include <algorithm>
#include <cstdio>
#include <unistd.h>

namespace {

const char *CA_BUNDLE = "/etc/ssl/certs/ca-certificates.crt";

struct Sink { std::string *body; size_t max; bool over = false; };

size_t gather (char *data, size_t size, size_t n, void *user) {
	auto *s = static_cast<Sink *>(user);
	if (s->body->size() + size * n > s->max) { s->over = true; return 0; }
	s->body->append(data, size * n);
	return size * n;
}

struct FileSink { FILE *f; long written, max; bool over = false; };

size_t to_file (char *data, size_t size, size_t n, void *user) {
	auto *s = static_cast<FileSink *>(user);
	if (s->written + (long)(size * n) > s->max) { s->over = true; return 0; }
	size_t w = fwrite(data, 1, size * n, s->f);
	s->written += (long)w;
	return w;
}

struct Progress { const std::function<bool(long, long)> *fn; long last = -1; bool stopped = false; };

int on_progress (void *user, curl_off_t total, curl_off_t done, curl_off_t, curl_off_t) {
	auto *p = static_cast<Progress *>(user);
	// about every 1 % (or 256 KB while the size is unknown)
	long step = total > 0 ? std::max<long>(1, (long)total / 100) : 256 * 1024;
	if (p->last < 0 || done - p->last >= step || (total > 0 && done == total)) {
		p->last = (long)done;
		if (!(*p->fn)((long)done, (long)total)) { p->stopped = true; return 1; }
	}
	return 0;
}

const char *USER_AGENT = "SimpleLGLauncher/native (https://github.com/fardroid/Simple-LG-launcher)";

}  // namespace

void http_init () { curl_global_init(CURL_GLOBAL_DEFAULT); }

HttpResult http_get (const std::string &url, long timeout_s, size_t max_bytes) {
	HttpResult r;
	CURL *c = curl_easy_init();
	if (!c) { r.error = "no curl"; return r; }
	Sink sink{&r.body, max_bytes};
	curl_easy_setopt(c, CURLOPT_URL, url.c_str());
	curl_easy_setopt(c, CURLOPT_CAINFO, CA_BUNDLE);
	curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(c, CURLOPT_MAXREDIRS, 5L);
	curl_easy_setopt(c, CURLOPT_TIMEOUT, timeout_s);
	curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 10L);
	curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(c, CURLOPT_ACCEPT_ENCODING, "");          // gzip when the server offers it
	// Wikimedia asks scripts to say who they are
	curl_easy_setopt(c, CURLOPT_USERAGENT, USER_AGENT);
	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, gather);
	curl_easy_setopt(c, CURLOPT_WRITEDATA, &sink);
	CURLcode rc = curl_easy_perform(c);
	curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &r.status);
	char *type = nullptr;
	curl_easy_getinfo(c, CURLINFO_CONTENT_TYPE, &type);
	if (type) r.content_type = type;
	if (rc != CURLE_OK) r.error = sink.over ? "too big" : std::string(curl_easy_strerror(rc)) + " (curl " + std::to_string((int)rc) + ")";
	else if (r.status < 200 || r.status >= 300) r.error = "HTTP " + std::to_string(r.status);
	else r.ok = true;
	curl_easy_cleanup(c);
	if (!r.ok) {
		// the host only: some addresses are secret (Google Calendar's private links)
		size_t start = url.find("://"), end = start == std::string::npos ? std::string::npos : url.find('/', start + 3);
		logf("http %s: %s", url.substr(0, end).c_str(), r.error.c_str());
	}
	return r;
}

HttpResult http_download (const std::string &url, const std::string &path, const std::function<bool(long, long)> &progress, long timeout_s, long max_bytes) {
	HttpResult r;
	std::string part = path + ".part";
	FILE *f = fopen(part.c_str(), "wb");
	if (!f) { r.error = "cannot write " + part; return r; }
	CURL *c = curl_easy_init();
	if (!c) { fclose(f); unlink(part.c_str()); r.error = "no curl"; return r; }
	FileSink sink{f, 0, max_bytes};
	Progress prog{&progress};
	curl_easy_setopt(c, CURLOPT_URL, url.c_str());
	curl_easy_setopt(c, CURLOPT_CAINFO, CA_BUNDLE);
	curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);       // GitHub's release files are behind a redirect
	curl_easy_setopt(c, CURLOPT_MAXREDIRS, 5L);
	curl_easy_setopt(c, CURLOPT_TIMEOUT, timeout_s);
	curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 15L);
	curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(c, CURLOPT_USERAGENT, USER_AGENT);
	curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, to_file);
	curl_easy_setopt(c, CURLOPT_WRITEDATA, &sink);
	curl_easy_setopt(c, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, on_progress);
	curl_easy_setopt(c, CURLOPT_XFERINFODATA, &prog);
	CURLcode rc = curl_easy_perform(c);
	curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &r.status);
	curl_easy_cleanup(c);
	bool written = fflush(f) == 0;
	fsync(fileno(f));
	written = fclose(f) == 0 && written;
	if (rc != CURLE_OK) r.error = prog.stopped ? "stopped" : sink.over ? "too big" : std::string(curl_easy_strerror(rc)) + " (curl " + std::to_string((int)rc) + ")";
	else if ((r.status < 200 || r.status >= 300) && !(r.status == 0 && url.rfind("file://", 0) == 0)) r.error = "HTTP " + std::to_string(r.status);
	else if (!written) r.error = "cannot write " + part;
	else if (rename(part.c_str(), path.c_str()) != 0) r.error = "cannot write " + path;
	else r.ok = true;
	if (!r.ok) { unlink(part.c_str()); logf("download %s: %s", url.substr(0, url.find('?')).c_str(), r.error.c_str()); }
	return r;
}

std::string url_encode (const std::string &s) {
	std::string out;
	for (unsigned char ch : s) {
		if (isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '!' || ch == '~' || ch == '*' || ch == '\'' || ch == '(' || ch == ')') out += (char)ch;
		else {
			char b[4];
			snprintf(b, sizeof b, "%%%02X", ch);
			out += b;
		}
	}
	return out;
}
