// Quick settings (Down from the row of apps): a strip of tiles over the bottom of the home screen. ← → choose a tile, ↑ or Back
// close the strip. OK on a setting opens the list of its values above the tile (↑ ↓ choose, OK takes one, Back closes it); OK on
// the others does what the tile says.
#pragma once

#include "ui/anim.h"
#include "ui/theme.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

class QuickPanel {
public:
	std::function<void(const std::string &id)> on_activate;                          // screen off, the TV's settings
	std::function<void(const std::string &id, const std::string &value)> on_choose;  // a value picked from a list
	std::function<void()> on_close;

	void open (double now);
	void close (double now);
	bool is_open () const { return open_; }
	/** What the TV says a setting is now (empty: not known), and the values it has. */
	void set_value (const std::string &id, const std::string &value) { values_[id] = value; }
	void set_options (const std::string &id, const std::vector<std::string> &values) { options_[id] = values; }
	bool key_down (int scancode, bool repeat, double now);
	bool key_up (int scancode, double now);
	bool animating (double now) const;
	void draw (const std::string &theme, double now);

private:
	bool open_ = false, ok_armed_ = false;
	int index_ = 0;
	Tween shown_, focus_;
	int focus_from_ = -1;
	std::map<std::string, std::string> values_;
	std::map<std::string, std::vector<std::string>> options_;
	// the list of values over a tile
	bool list_open_ = false;
	int list_index_ = 0, list_tile_ = 0;
	Tween list_shown_, list_focus_;
	int list_from_ = -1;

	std::vector<std::string> options_of (const std::string &id) const;
	void open_list (double now);
	void close_list (double now);
	void move_tile (int to, double now);
	void draw_list (const Theme &t, double now);
};
