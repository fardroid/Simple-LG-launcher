// The banners at the top of the screen, over whatever screen is open: the cards' glass with a round icon, a title, a line under it
// and a ring that runs down until the banner goes. Several at once stand one under another, the newest at the top: a new one
// pushes the others down, and when one goes the ones under it move up.
#pragma once

#include "data/config.h"
#include "gfx/gfx.h"
#include "ui/anim.h"
#include "ui/plates.h"

#include <deque>
#include <string>
#include <vector>

class Wallpaper;

enum class NoticeKind {
	info,        // blue "i": something is going on, or a hint
	done,        // green tick: it is done
	error,       // red "!": it did not work
	download,    // blue arrow: something comes down in the background
	update,      // green arrow: a new version of the launcher
	restart,     // red circle arrow: the TV needs a restart
};

/** A round icon of colour with a white picture in it (the banners, and the line over the app row). */
void round_icon (Rect r, Color c, const char *glyph);
/** The colour and the picture of a kind's icon. */
Color notice_color (NoticeKind k);
const char *notice_glyph (NoticeKind k);

class Notices {
public:
	/** A banner; `text` (may be empty) is the dimmer line under the title. The same title and text while it is up: its time
	 *  starts again instead. */
	void show (NoticeKind kind, const std::string &title, const std::string &text, double now);
	/** true while any banner is up (their rings run down every frame). */
	bool animating (double now) const;
	/** What they are drawn with: the wallpaper's glass and the plates' colour, as the cards on the home screen. */
	void draw (double now, const Wallpaper *wp, const UserConfig &cfg, const PlateLook &look, const PlateLook &old_look);

private:
	struct Item {
		NoticeKind kind;
		std::string title, text;
		double at = 0, hold = 0;      // when it came, how long it stays (between coming in and going)
		Tween y;                      // its place, moving when the ones above it come or go
		float h = 0;
		std::vector<std::string> title_lines, text_lines;
		float w = 0;
	};
	std::vector<Item> items_;         // the newest first
	std::deque<Item> waiting_;        // more than fit at once: shown as others go
	void measure (Item &it) const;
	void settle (double now);         // drops the ones gone, takes in the waiting ones, moves each to its place
	double gone_at (const Item &it) const;
};
