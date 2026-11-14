// The home screen (screens/Home.tsx with components/Row.tsx and Tile.tsx): the clock and the cards in columns, and the tiles along
// the bottom. Left/Right move along a row, Up/Down change row (two rows), OK opens, Back goes to the first tile.
#pragma once

#include "data/config.h"
#include "data/rows.h"
#include "data/weather.h"
#include "ui/calendar_card.h"
#include "ui/news_card.h"
#include "ui/anim.h"
#include "ui/boardlayout.h"
#include "ui/tile.h"
#include "ui/wallpaper.h"

#include <ctime>
#include <optional>
#include <functional>
#include <string>
#include <vector>

class Home {
public:
	std::function<void(const TileModel &)> on_activate;
	/** OK held on a tile of the row (its Move / Hide / Uninstall menu). */
	std::function<void(const TileModel &)> on_long_press;
	/** Down from the (last) row of apps: the quick settings. */
	std::function<void()> on_quick;
	// Moving a tile (after a long press the app calls lift): ← → move it along the row, OK keeps the place, Back puts it back;
	// ↑ goes to the buttons over it (hide, and uninstall for an app that can be removed)
	std::function<void(const std::vector<std::string> &keys)> on_reorder;     // every tile's key, in the new order
	std::function<void(const std::string &key)> on_hide, on_uninstall;
	void lift (bool can_uninstall, double now);
	void end_lift (double now);
	bool lifting () const { return lifting_; }

	void set_items (const std::vector<TileModel> &items);
	void set_config (const UserConfig &c) { cfg_ = c; set_current_theme(c.theme); board_dirty_ = true; }
	void set_lang (const std::string &lang, bool h24) { lang_ = lang; h24_ = h24; board_dirty_ = true; }
	void set_wallpaper (const Wallpaper *w) { wp_ = w; }
	/** The weather, when it is on and known (null otherwise). */
	void set_weather (const std::optional<WeatherData> &w) { weather_ = w; board_dirty_ = true; }
	/** The calendar card's data (null while nothing has been read) and whether it is being read. */
	void set_calendar (const std::optional<CalendarData> &c, bool loading) { calendar_ = c; cal_loading_ = loading; board_dirty_ = true; }
	void set_news (const std::optional<NewsData> &n, bool loading) { news_ = n; news_loading_ = loading; board_dirty_ = true; }
	void set_quote (const std::optional<Quote> &q, bool loading) { quote_ = q; quote_loading_ = loading; board_dirty_ = true; }
	/** A quiet notice over the right end of the row (a new version of the launcher); empty: none. */
	void set_notice (const std::string &text) { notice_ = text; }
	/** The launcher is in front (the news pages turn by themselves only then). */
	void set_front (bool front) { front_ = front; }
	/** OLED care: the clock, the cards and the apps move a few pixels once a minute (pixelShift "auto" follows this). */
	void set_oled (bool oled) { oled_ = oled; board_dirty_ = true; }
	/** ms until something on the screen changes by itself (the news page), -1: nothing. */
	double next_event (double now) const;
	/** Turns the news page when its time came; true when it did. */
	bool tick (double now);
	/** Home was pressed (the launcher came to the front): the cards go back to today, the app row keeps its place. */
	void wake ();

	/** A key: true when it was used. */
	bool key (int scancode, bool repeat, double now);
	/** A key let go: OK on a tile acts on its release, so holding it opens the menu instead (useOkPress). */
	bool key_up (int scancode, double now);
	void pointer (float x, float y, double now);
	void pointer_left ();
	void click (float x, float y, double now);
	void wheel (int dy, double now);

	/** true while something moves */
	bool animating (double now) const;
	void draw (double now, const std::tm &clock);
	// Drawn in parts: the board (the clock, the cards and their plates) changes rarely and is kept in a picture; the app row and
	// the headline over the screen are drawn every frame.
	/** The board has to be drawn again (something on it changed, or it moves). */
	bool board_needed (double now) const;
	void mark_board () { board_dirty_ = true; }
	void draw_board (double now, const std::tm &clock);
	/** The cards the remote is on, over the picture of the board (drawn every frame they change). */
	void draw_live (double now, const std::tm &clock);
	void draw_row (double now);
	void draw_overlay (double now);

private:
	enum class Zone { dock, weather, calendar, news };
	struct TileAnim { Tween focus, slide; };     // slide: px it is drawn away from its place, going to 0 (a tile moved)
	struct RowView { Tween offset; double last_step = -1e9; int anchor = 0; bool fast = false; };

	int tile_size () const;
	int tile_gap () const;
	int label_font () const;
	float label_area () const;
	std::vector<int> sizes () const;
	float row_top (int r) const;
	float row_x0 () const { return (float)(cfg_.sideMargin + shift_x_); }
	float viewport_w () const { return 1920.f - 2 * cfg_.sideMargin; }
	double target_offset (int r, int anchor) const;
	void moved (double now);
	int tile_at (float x, float y, int &row) const;
	void draw_tile (const TileModel &m, float x, float y, float f, double now, bool label_on);
	BoardLayout layout () const;
	bool card_on (CardId id) const;
	/** Steps onto a card (false when it cannot be stepped onto). */
	bool enter (std::optional<CardId> id, double now);
	void enter_above (CardId from, double now);
	void leave_down (CardId from, double now);
	void leave_side (CardId from, int step, double now);
	void set_zone (Zone z, double now);
	float selected_tile_x () const;

	UserConfig cfg_;
	std::string lang_ = "en";
	bool h24_ = true;
	std::vector<TileModel> items_;
	std::vector<TileAnim> anim_;
	RowsPosition pos_;
	RowView rows_[2];
	bool pointing_ = false;
	// the pointer resting near an end of a row with more beyond it: the row glides on by itself (Row.tsx's edge scroll)
	float px_ = 0, py_ = 0;
	int edge_row_ = -1, edge_dir_ = 0;
	float edge_depth_ = 0;           // 0 where the zone begins, 1 at the very end: the speed between the two
	double edge_since_ = 0, edge_last_ = 0;
	void edge_zone (double now);
	void pointer_card (float x, float y, double now);
	bool click_card (float x, float y, double now);
	int weather_days () const;
	void edge_glide (double now);
	const Wallpaper *wp_ = nullptr;
	Zone zone_ = Zone::dock;
	std::optional<WeatherData> weather_;
	int weather_day_ = 0;
	std::optional<CalendarData> calendar_;
	bool cal_loading_ = false;
	CalendarState cal_;
	std::optional<NewsData> news_;
	bool news_loading_ = false;
	int news_page_ = 0, news_sel_ = 0;
	std::optional<FeedItem> detail_;
	double detail_at_ = -1e9, next_page_at_ = -1;
	std::optional<Quote> quote_;
	bool quote_loading_ = false, front_ = true, oled_ = false;
	int shift_x_ = 0, shift_y_ = 0;
	bool board_dirty_ = true;
	std::string notice_;
	void draw_notice ();
	bool lifting_ = false, lift_trash_ = false;
	int lift_btn_ = -1;          // -1: on the tile; 0, 1: the buttons over it
	std::vector<TileModel> lift_original_;
	int lift_original_index_ = 0;
	Tween lift_;
	int movable_count () const;
	bool lift_key (int scancode, double now);
	void lift_ok (double now);
	std::vector<std::string> lift_buttons () const;
	void draw_lift (float x, float top_y, double now);
	bool ok_armed_ = false, ok_fired_ = false;
	double ok_down_at_ = 0;
	int selected_index () const;
	bool board_animating (double now) const;
	int news_per_page () const;
	bool news_key (bool left, bool right, bool up, bool down, bool back, bool ok, double now);
	bool calendar_key (bool left, bool right, bool up, bool down, bool back, bool ok, double now);
	std::string logged_plates_;   // the plates' colour last written to the log
	Tween row_dim_;          // 0: the row has the focus, 1: the remote is on the cards (.row__viewport opacity 0.55)
	Tween card_focus_[3];    // the weather, the calendar, the news: their focus layer
	Tween card_grow_[3];     // and their growth by 1.02
	BoardLayout board_;
	PlateLook look_{}, old_look_{};
	int drawn_live_ = 0;
	int live_mask (double now) const;
	void prepare_board ();
	void draw_card (const BoardCard &c, double now, const std::tm &clock);
};
