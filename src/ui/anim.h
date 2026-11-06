// Time and motion: CSS's easing curves and transitions, worked out from the clock (not from the number of frames).
#pragma once

#include <chrono>
#include <cmath>

inline double now_ms () {
	using namespace std::chrono;
	static const auto start = steady_clock::now();
	return duration<double, std::milli>(steady_clock::now() - start).count();
}

/** CSS cubic-bezier(x1, y1, x2, y2) at progress t (0-1). */
inline double cubic_bezier (double x1, double y1, double x2, double y2, double t) {
	if (t <= 0) return 0;
	if (t >= 1) return 1;
	auto bx = [&] (double s) { return 3 * x1 * s * (1 - s) * (1 - s) + 3 * x2 * s * s * (1 - s) + s * s * s; };
	auto by = [&] (double s) { return 3 * y1 * s * (1 - s) * (1 - s) + 3 * y2 * s * s * (1 - s) + s * s * s; };
	double lo = 0, hi = 1, s = t;
	for (int i = 0; i < 30; i++) {
		s = (lo + hi) / 2;
		if (bx(s) < t) lo = s; else hi = s;
	}
	return by(s);
}

enum class Ease { linear, ease_out, ease_in_out, focus, scroll, tile };

inline double ease (Ease e, double t) {
	switch (e) {
		case Ease::linear: return t < 0 ? 0 : t > 1 ? 1 : t;
		case Ease::ease_out: return cubic_bezier(0, 0, 0.58, 1, t);
		case Ease::ease_in_out: return cubic_bezier(0.42, 0, 0.58, 1, t);
		case Ease::scroll: return cubic_bezier(0.2, 0.7, 0.2, 1, t);       // .row__track
		case Ease::tile: return cubic_bezier(0.3, 0.9, 0.3, 1, t);         // .tile
		default: return cubic_bezier(0.25, 0.1, 0.25, 1, t);               // CSS "ease"
	}
}

/** A value that moves to a new target over `ms`, the way a CSS transition does: from wherever it is at that moment. */
struct Tween {
	double from = 0, to = 0, start = -1e9, ms = 1;
	Ease curve = Ease::ease_out;
	double at (double now) const {
		double t = (now - start) / ms;
		if (t >= 1) return to;
		return from + (to - from) * ease(curve, t);
	}
	bool moving (double now) const { return now - start < ms && from != to; }
	void set (double target, double now, double duration, Ease e) {
		if (target == to && !moving(now)) return;
		if (target == to) return;
		from = at(now);
		to = target;
		start = now;
		ms = duration > 0 ? duration : 1;
		curve = e;
	}
	void jump (double v) { from = to = v; start = -1e9; }
};
