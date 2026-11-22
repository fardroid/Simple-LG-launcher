#include "ui/saver.h"

#include <algorithm>
#include <cmath>

namespace {
float clamp01 (double v) { return (float)std::min(1.0, std::max(0.0, v)); }
double cubic_in_out (double t) { return t < 0.5 ? 4 * t * t * t : 1 - std::pow(-2 * t + 2, 3) / 2; }
double cubic_out (double t) { return 1 - std::pow(1 - t, 3); }
double sine_in_out (double t) { return -(std::cos(M_PI * t) - 1) / 2; }
const float CARDS_DELAY = 0.2f;      // the widgets start fading when the row is this far
}  // namespace

void Saver::start (double now) {
	if (target_ == 1) return;
	float v = progress(now);
	if (v <= 0) {
		// a fresh start: it grows to ZOOM as it comes on, and drifts from the middle
		from_ = View();
		from_.s = ZOOM;
		to_ = pick(from_);
		drift_at_ = now;
	}
	seg_ = {v, 1, now, std::max(1.0, ENTER_MS * (1 - v)), true};
	target_ = 1;
	moving_ = true;
}

void Saver::wake (double now) {
	if (target_ == 0) return;
	float v = progress(now);
	seg_ = {v, 0, now, std::max(1.0, EXIT_MS * v), false};
	target_ = 0;
	moving_ = true;
}

void Saver::stop () {
	target_ = 0;
	moving_ = false;
	seg_ = Seg();
}

bool Saver::animating (double now) const { return moving_ && now - seg_.t0 < seg_.dur; }

float Saver::progress (double now) const {
	if (!moving_) return seg_.to;
	double t = std::min(1.0, std::max(0.0, (now - seg_.t0) / seg_.dur));
	double e = seg_.in ? cubic_in_out(t) : cubic_out(t);
	return clamp01(seg_.from + (seg_.to - seg_.from) * e);
}

float Saver::cards (double now) const { return clamp01((progress(now) - CARDS_DELAY) / (1 - CARDS_DELAY)); }

// The next place to drift to. The picture grows by ZOOM round a point inside the screen, which keeps the screen covered; moving
// that point pans the picture by (ZOOM - 1) times its way. Only the point moves: a picture that grows or shrinks slowly is
// resampled unevenly and shimmers like hot air (seen on the TV), one that only slides does not. The point goes at least a third
// of the screen's width away each time, so each drift is a clear slide of some 50-100 px.
Saver::View Saver::pick (const View &from) {
	std::uniform_real_distribution<float> u(0, 1);
	View v;
	v.s = ZOOM;
	for (int i = 0; i < 20; i++) {
		v.x = 1920 * (0.08f + 0.84f * u(rng_));
		v.y = 1080 * (0.08f + 0.84f * u(rng_));
		if (std::hypot(v.x - from.x, (v.y - from.y) * 1.78f) > 640) break;
	}
	return v;
}

Saver::View Saver::drift (double now) const {
	float t = (float)sine_in_out(clamp01((now - drift_at_) / STEP_MS));
	return {from_.s + (to_.s - from_.s) * t, from_.x + (to_.x - from_.x) * t, from_.y + (to_.y - from_.y) * t};
}

void Saver::view (double now, float &cx, float &cy, float &k) {
	// one drift after another, each from where the last one ended, as long as it is on
	while (on() && now - drift_at_ >= STEP_MS) {
		from_ = to_;
		to_ = pick(from_);
		drift_at_ += STEP_MS;
	}
	View d = drift(now);
	float v = progress(now);
	cx = d.x;
	cy = d.y;
	k = 1 + (d.s - 1) * v;
}
