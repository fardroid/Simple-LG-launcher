// The calendar card (components/CalendarCard.tsx): the events of one day with the days around it in a strip; Up turns it into
// the month (the day view goes down and out, the month comes down from above). The days and the months slide as one strip.
#pragma once

#include "data/calendar.h"
#include "gfx/text.h"
#include "ui/anim.h"
#include "ui/boardlayout.h"

#include <ctime>
#include <map>

/** Where the card is and how it moves: kept by the home screen, drawn here. */
struct CalendarState {
	int offset = 0;              // days from today
	bool month_mode = false;
	bool head = false;           // the remote is on the head (the date and its arrows), not the body
	int month = 0;               // the month shown, in months from the current one
	int dir = 0;                 // which way the day just moved, for the events' slide
	int scroll = 0;              // events scrolled past
	int max_scroll = 0;          // how far they can go (measured when drawn)
	Tween strip, layer, months, list, head_glow;
	double day_changed = -1e9;
	// what is worked out from the events, kept while the events, today and the chosen day stay the same (a frame of a slide
	// would otherwise walk every event again: the days of the strip, three months, the day's list)
	std::string memo_key;
	std::map<int, std::vector<int>> strip_dots;
	std::map<int, MonthView> month_views;
	std::map<int, std::vector<DayEvent>> day_events;
	void check_memo (const CalendarData *data, long long now_ms, bool h24);
	/** The day moved by `days` (clamped to the range the card shows). */
	void shift_day (int days, double now);
	void to_today (double now);
	void set_month (int m, double now);
	void set_mode (bool month, double now);
	void set_head (bool on, double now);
	void set_scroll (int s, double now);
	bool animating (double now) const;
};

void draw_calendar (const BoardCard &card, CalendarState &st, const CalendarData *data, bool loading, int card_size, Face face,
	long long now_ms, bool h24, bool focused, float focus);

/** What is under the point x, y, as draw_calendar lays it out: the arrow back (-1) or on (1) in the head, a day of the strip
 *  (`days` from the chosen one, 2), or nothing (0). */
int calendar_hit (const BoardCard &card, const CalendarState &st, int card_size, float x, float y, int &days);
