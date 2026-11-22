// The screen saver: after a while without a key on the home screen the app row sinks 60 px and fades, the widgets fade where
// they are a beat later, and the wallpaper stays alone, slowly zooming and drifting (Ken Burns), so that on an OLED no point of
// it stands still. Any key brings the home screen back the same way, faster. It is the launcher's own: LG's screen saver is not
// touched (when the TV starts its own over the launcher, this one ends at once, under it).
#pragma once

#include <random>

class Saver {
public:
	static constexpr double ENTER_MS = 1000, EXIT_MS = 500;
	static constexpr double STEP_MS = 30000;     // each drift of the view (and the slideshow's picture under the saver)
	static constexpr float ROW_SINK = 60;        // px the app row goes down while it fades

	/** Starts coming on (from wherever it is, when it was going away). */
	void start (double now);
	/** A key: goes away (from wherever it is). */
	void wake (double now);
	/** Gone at once, with no way back (the launcher went to the back). */
	void stop ();
	/** It is on or coming on (a key now wakes it). */
	bool on () const { return target_ == 1; }
	/** Any of it is on the screen (on, coming on or going away). */
	bool showing (double now) const { return on() || progress(now) > 0; }
	/** Coming on or going away. */
	bool animating (double now) const;
	/** 0: the home screen, 1: the saver alone. */
	float progress (double now) const;
	/** How far the app row has gone (0-1: down ROW_SINK px and faded). */
	float row (double now) const { return progress(now); }
	/** How far the widgets have faded (0-1): they start a beat after the row. */
	float cards (double now) const;
	/** The wallpaper's slow zoom: it grows by k round cx, cy (k 1 on the home screen). */
	void view (double now, float &cx, float &cy, float &k);

private:
	struct View { float s = 1, x = 960, y = 540; };
	struct Seg { float from = 0, to = 0; double t0 = 0, dur = 1; bool in = true; };
	int target_ = 0;
	bool moving_ = false;
	Seg seg_;
	View from_, to_;
	double drift_at_ = 0;
	bool zoom_in_ = true;
	std::mt19937 rng_{std::random_device{}()};
	View pick ();
	View drift (double now) const;
};
