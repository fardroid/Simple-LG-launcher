// The calendar card's data (lib/calendar.ts, lib/calendarload.ts): one day's events from iCal feeds (Google Calendar's secret
// address), files on the TV or folders of .ics files; the saved copy; the words.
#pragma once

#include "data/ics.h"

#include <optional>
#include <string>
#include <vector>

constexpr int CAL_BACK_DAYS = 62, CAL_AHEAD_DAYS = 186;
constexpr double CAL_FRESH_MS = 15 * 60 * 1000.0;
constexpr double CAL_SHOWN_MS = 48 * 60 * 60 * 1000.0;
/** The colour of each calendar's bar, in the order the calendars are read. */
extern const char *CAL_COLORS[6];

struct CalendarSource { std::string src, name, sig; };
struct CalendarData {
	std::string key;
	double at = 0;
	std::vector<CalendarSource> sources;
	std::vector<Occurrence> events;
};
struct CalendarPart { CalendarSource source; std::vector<Occurrence> events; };

/** Local midnight `offset` days from the day `now` falls in. */
long long start_of_day (long long now, int offset);
std::optional<CalendarPart> parse_calendar (const std::string &text, const std::string &src, const std::string &sig, int index, long long now);
CalendarData join_calendars (const std::string &key, std::vector<CalendarPart> parts, long long now);
std::string normalize_calendar_url (const std::string &raw);
bool valid_calendar_path (const std::string &path);

struct DayEvent { std::string time, note, title, place; int src; std::string state; };    // state: past now next allday
std::vector<DayEvent> events_of_day (const CalendarData *data, long long day_start, long long now, bool h24);
std::vector<int> dots_of_day (const CalendarData *data, long long day_start);
std::string relative_day (int offset);
/** "Saturday, 24 October" for the day `offset` days from today. */
std::string day_heading (long long now, int offset);

struct MonthCell { long long start; int day; bool inMonth, today, selected; std::vector<int> dots; };
struct MonthView { std::string name; int year; std::vector<std::vector<MonthCell>> weeks; };
MonthView month_view (const CalendarData *data, long long now, int month_offset, long long selected_day);
int month_offset_of_day (long long now, int day_offset);
int day_offset_of_month (long long now, int month_offset);

std::string calendar_json (const CalendarData &d);
std::optional<CalendarData> calendar_from_json (const std::string &raw);
std::optional<CalendarData> usable_calendar (const std::optional<CalendarData> &d, const std::string &key, double now);
bool calendar_fresh (const std::optional<CalendarData> &d, const std::string &key, double now);

/** Reads every calendar (folders expanded) into one set of events (worker thread); files unchanged since `known` are taken from it. */
std::optional<CalendarData> fetch_calendars (const std::vector<std::string> &sources, const std::string &key, const std::optional<CalendarData> &known, std::string *why);
