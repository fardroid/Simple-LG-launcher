// Settings → Home screen → How the launcher opens (screens/HomeMethod.tsx): the four ways, the one that is on ticked, and for the
// focused one how it works, its pros and cons. OK on another way asks once more ("press OK again") before switching, so a stray
// press never changes what the Home button does.
#pragma once

#include "data/homemode.h"

#include <functional>
#include <optional>
#include <string>

class HomeMethod {
public:
	std::function<void(const std::string &mode)> on_pick;
	std::function<void()> on_close;

	void open (double now);
	/** What is on now and what this TV can do (none while it is being read, or when it could not be). */
	void set_info (const std::optional<HomeModeInfo> &info, bool reading);
	/** The way being switched to, while the switch runs (empty: none). */
	void set_busy (const std::string &mode) { busy_ = mode; }
	void set_theme (const std::string &theme) { theme_ = theme; }
	bool key_down (int scancode, double now);
	/** The Magic Remote: the pointer on a way focuses it, a click is OK on it (asking once more, as OK does). */
	void pointer (float x, float y, double now);
	void click (float x, float y, double now);
	bool animating (double now) const;
	void draw (double now);

private:
	void activate (int i);
	int row_at (float x, float y) const;
	std::string theme_ = "dusk";
	std::optional<HomeModeInfo> info_;
	bool reading_ = true;
	std::string busy_, armed_;
	int index_ = 0;
	double focus_at_ = -1e9;
};
