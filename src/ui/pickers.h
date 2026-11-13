// The sheets opened from Settings (screens/CityPicker.tsx, CountryPicker.tsx, CalendarSetup.tsx, FeedsSetup.tsx): a title and a hint,
// the place as a small heading over one rounded group of rows. Some rows take text, typed with the TV's on-screen keyboard.
#pragma once

#include "data/config.h"

#include <functional>
#include <string>
#include <vector>

class Picker {
public:
	enum class Kind { none, city, country, calendars, feeds };

	std::function<void(const City &)> on_city;
	std::function<void(const std::string &)> on_country;
	std::function<void(const std::vector<std::string> &)> on_calendars, on_feeds;
	std::function<void()> on_close;

	/** Opens a sheet. `current`: the country in use; `list`: the calendars or the feeds in use; `can_read_files`: the Homebrew
	 *  Channel can read files (calendar.txt, the folder of .ics files). */
	void open (Kind kind, const std::string &current, const std::vector<std::string> &list, bool can_read_files, double now);
	/** The calendars or feeds changed while the sheet is open (it shows them). */
	void set_list (const std::vector<std::string> &list) { list_ = list; }
	/** The TV's country ("IT"): its cities come first in the city list. */
	void set_tv_country (const std::string &country) { tv_country_ = country; }
	Kind kind () const { return kind_; }
	bool typing () const { return typing_; }

	bool key_down (int scancode, bool repeat, double now);
	void text_input (const std::string &text);
	void set_theme (const std::string &theme) { theme_ = theme; }
	bool animating (double now) const;
	void draw (double now);

private:
	struct Row {
		std::string key;         // search back city info type file folder clear source feed country busy
		std::string icon;        // search info folder pin drive picture
		std::string label, detail;
		int index = -1;          // the city, the source or the feed it stands for
	};

	std::vector<Row> rows () const;
	void activate (int i);
	void start_typing ();
	void stop_typing ();
	void submit ();
	void run_search (const std::string &text);
	bool add_source (const std::string &raw);
	bool add_feed (const std::string &raw);
	void read_file ();

	Kind kind_ = Kind::none;
	std::string theme_ = "dusk";
	std::string current_;
	std::string tv_country_;
	std::vector<std::string> list_;
	bool can_read_ = false;
	int index_ = 0;
	bool typing_ = false;
	std::string typed_;
	std::string note_;
	// the city search: null while showing the list
	bool searching_ = false, busy_ = false, failed_ = false;
	std::vector<City> results_;
	int search_id_ = 0;
	std::vector<std::string> countries_;     // by name in the screen's language
	double focus_at_ = -1e9;
};
