// A short message at the top of the screen (components/Toast.tsx): a pill in the middle, gone after a couple of seconds.
#pragma once

#include <string>

class Toast {
public:
	void show (const std::string &text, bool error, double now);
	/** ms until it goes, -1: none shown. */
	double next_event (double now) const;
	bool tick (double now);
	void draw (const std::string &theme);
private:
	std::string text_;
	bool error_ = false;
	double until_ = 0;
	bool shown_ = false;
};
