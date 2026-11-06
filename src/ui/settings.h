// Settings (screens/Settings.tsx with components/OptionList.tsx and Grid.tsx): the sections on the left, the chosen one on the right.
//
// ↑ ↓ pick a section, → or OK go into it, Back comes out again (and, from the list of sections, closes Settings).
// Option sections: a list of settings in groups; OK changes one, ← → nudge numbers and choices.
// Apps and Inputs: a grid of tiles; OK shows or hides one. Re-order mode (hold OK, or the Arrange button) shows only what is on the
// home screen, in order: OK picks an item up, arrows move it, OK puts it down.
#pragma once

#include "data/prefs.h"
#include "ui/anim.h"
#include "ui/tile.h"

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

struct SettingsTab {
	std::string id;
	std::string icon;            // apps inputs look widgets wallpaper home
	std::string label;
	std::string arrange;         // the re-order button (grid sections)
	std::vector<TileModel> items;    // everything available
	std::vector<TileModel> shown;    // what is on the home screen, in home-screen order
	bool refresh = false;        // a "Refresh list" button next to the re-order button
	bool has_options = false;    // a list of settings instead of a grid of tiles
	std::vector<OptionItem> options;
};

class Settings {
public:
	std::function<void(const std::string &tab, const std::string &key)> on_toggle;
	std::function<void(const std::string &tab, const std::vector<std::string> &keys)> on_reorder;
	std::function<void(const std::string &option, Press press)> on_option;
	std::function<void()> on_refresh, on_close;

	/** Opened from the home screen: the first section, the list of sections in focus. */
	void open (double now);
	void set_tabs (const std::vector<SettingsTab> &tabs) { tabs_ = tabs; }
	/** The focus on option `id` of section `tab`, in the list (the update row, from its tile on the home screen). */
	void focus_option (const std::string &tab, const std::string &id, double now);
	void set_theme (const std::string &theme) { theme_ = theme; }

	bool key_down (int scancode, bool repeat, double now);
	bool key_up (int scancode, double now);
	/** The Magic Remote pointer: over a section, a row, a button or a tile it takes the focus (as the keys would); a click acts as
	 *  OK there, and on a row of ‹ › values, left of the value steps back and right of it on. The wheel moves along the list. */
	void pointer (float x, float y, double now);
	void click (float x, float y, double now);
	void wheel (int dy, double now);
	/** OK held long enough: re-order mode (true when it fired, to draw). */
	bool tick (double now);
	double next_event (double now) const;
	bool animating (double now) const;
	void draw (double now);
	// Changing a setting that shows on the home screen: Settings steps aside for a moment and the home screen shows it
	// ("peek"): the dark cover and everything on it fade out except the changed row, which stays where it is; it comes back after
	// a pause, or with ↑ ↓ or Back
	/** 0-1: how far Settings has stepped aside (the home screen is drawn under it then). */
	float peek (double now) const { return (float)peek_.at(now); }

private:
	enum class Zone { nav, buttons, content };
	struct Held { int index; std::vector<std::string> original; };

	const SettingsTab *current () const { return tab_ < (int)tabs_.size() ? &tabs_[tab_] : nullptr; }
	const SettingsTab *grid_tab () const { auto c = current(); return c && !c->has_options ? c : nullptr; }
	std::vector<std::string> buttons () const;
	const std::vector<TileModel> &grid_items () const;
	int count () const;
	int option_at () const;
	void select_tab (int i, double now);
	void set_mode (bool reorder, double now);
	void move_held (int to, double now);
	void activate_grid (int i, double now);
	void press_button (const std::string &b, double now);
	void enter (double now);
	void press (double now);
	void long_press (double now);
	void set_zone (Zone z, double now);
	void set_option (int i, double now);
	void set_grid (int i, double now);
	void key (int scancode, double now);

	float list_shift (const std::vector<OptionItem> &options, int selected, float height);
	void draw_nav (double now);
	void draw_options (double now, float top);
	void draw_grid (double now, float top);
	void draw_footer ();

	std::vector<SettingsTab> tabs_;
	std::string theme_ = "dusk";
	int tab_ = 0, button_ = 0, grid_ = 0, option_ = 0;
	Zone zone_ = Zone::nav;
	bool reordering_ = false;
	std::optional<Held> held_;
	// OK: armed on its key-down (a key-up without one, like the OK that opened Settings, is ignored), fired once held
	bool ok_armed_ = false, ok_fired_ = false;
	double ok_down_at_ = 0;
	// motion: the focus pill fades from the row it left to the new one (.orow transition), the list and the grid slide
	Tween focus_;                // 0-1 towards the focused item
	int focus_from_ = -1;        // the item it left (in the same zone), -1: none
	Zone focus_zone_ = Zone::nav;
	Tween list_, grid_scroll_;
	double shift_ = 0;
	double lift_at_ = -1e9;
	Tween peek_;
	std::string peek_id_;
	double peek_until_ = -1;
	void start_peek (const std::string &id, double now);
	void end_peek (double now);
	// where things were drawn last, for the pointer: the rows of the list (and the middle of their value), the buttons, the tiles
	struct Spot { Rect r; int index; float mid = 0; };
	std::vector<Spot> row_spots_, button_spots_, tile_spots_;
	bool hit (float x, float y, Zone &zone, int &index, float &mid) const;
	std::map<std::string, Tween> knobs_, tiles_;     // the switches' knobs, the tiles' focus (by option id / tile key)
};
