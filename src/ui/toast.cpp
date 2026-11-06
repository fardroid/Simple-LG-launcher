#include "ui/toast.h"

#include "gfx/text.h"
#include "ui/theme.h"

#include <cmath>

namespace { const double TOAST_MS = 2200; }       // TIMING.toastMs

void Toast::show (const std::string &text, bool error, double now) {
	text_ = text;
	error_ = error;
	until_ = now + TOAST_MS;
	shown_ = true;
}

double Toast::next_event (double now) const { return shown_ ? std::max(0.0, until_ - now) : -1; }

bool Toast::tick (double now) {
	if (!shown_ || now < until_) return false;
	shown_ = false;
	return true;
}

void Toast::draw (const std::string &theme) {
	if (!shown_) return;
	const Theme &t = theme_named(theme);
	TextStyle st{Face::ui, 24, 400, 0};
	std::string s = text::ellipsize(text_, st, 1920 - 200);
	const TextLine &l = text::line(s, st);
	// an even width keeps the text on whole pixels
	float w = 2 * std::ceil((l.w + 56) / 2.f), h = 30 + 28;
	Rect box{(1920 - w) / 2, 72, w, h};
	gfx::ring(box, h / 2, 1, t.tileBorder);
	gfx::rect(box, h / 2, t.tileBgFocus);
	text::draw(s, st, box.x + 28, std::round(box.y + 14 + (30 - l.h) / 2), error_ ? t.danger : t.textSecondary);
}
