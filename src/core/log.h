// A short log for finding out what happened on the TV: /tmp/simplelauncher-native.log (RAM, gone at the next restart),
// started afresh at each start and cut when it grows past 256 KB. Also printed to stdout.
#pragma once

void log_open (const char *path);
void logf (const char *fmt, ...) __attribute__((format(printf, 1, 2)));
