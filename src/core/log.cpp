#include "core/log.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <mutex>

static FILE *file;
static std::mutex lock;
static long written;
static const long MAX_BYTES = 256 * 1024;
static const auto started = std::chrono::steady_clock::now();

void log_open (const char *path) {
	std::lock_guard<std::mutex> l(lock);
	file = fopen(path, "w");
	written = 0;
}

void logf (const char *fmt, ...) {
	char line[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(line, sizeof line, fmt, ap);
	va_end(ap);
	double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
	std::lock_guard<std::mutex> l(lock);
	printf("[%8.3f] %s\n", t, line);
	fflush(stdout);
	if (!file) return;
	if (written > MAX_BYTES) { rewind(file); written = 0; }
	written += fprintf(file, "[%8.3f] %s\n", t, line);
	fflush(file);
}
