// Reading an iCalendar (.ics) feed, such as Google Calendar's "secret address in iCal format", and working out which events
// fall in a stretch of time, repeating events expanded (lib/ics.ts). Pure functions, no network.
#pragma once

#include <string>
#include <vector>

struct Occurrence {
	long long start = 0, end = 0;      // ms since 1970
	bool allDay = false;
	std::string title, place;
	int src = 0;                       // which calendar it came from
};

struct Wall { int y, mo, d, h, mi, s; };

/** The moment a zone's wall-clock time happens; `tz` empty: the TV's own zone, "UTC", or an IANA name. */
long long wall_to_ms (const Wall &w, const std::string &tz);
/** Every event (repeats included) that overlaps [from, to), in time order. */
std::vector<Occurrence> expand_ics (const std::string &text, long long from, long long to);
/** The calendar's own name (X-WR-CALNAME), or "". */
std::string calendar_name (const std::string &text);
