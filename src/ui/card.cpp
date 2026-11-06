#include "ui/card.h"

#include <algorithm>

namespace {

Color color (const Rgba &c) { return {c.r, c.g, c.b, c.a}; }

// One layer of the plate: the glass (when there is one), then the colour over it, at `opacity`.
void layer (Rect card, const TexturePtr &frost, const PlateLook &look, float opacity) {
	if (opacity <= 0) return;
	if (frost) {
		// the 960 x 540 glass is stretched over the whole screen; the card shows the part behind it
		float kx = frost->w / 1920.f, ky = frost->h / 1080.f;
		gfx::image(*frost, card, opacity, CARD_RADIUS, {1, 1, 1, 1}, 1, {card.x * kx, card.y * ky, card.w * kx, card.h * ky});
	}
	Color c = color(look.bg);
	gfx::rect(card, CARD_RADIUS, c.with_alpha(c.a * opacity));
}

}  // namespace

void draw_plate (Rect card, const Wallpaper &wp, const UserConfig &cfg, const PlateLook &look, const PlateLook &old_look, float focus, double now) {
	if (card.w <= 0 || card.h <= 0) return;
	if (cfg.shadows) gfx::shadow(card, CARD_RADIUS, 34, 70, {0, 0, 0, 0.45f});
	float f = wp.fade(now);
	const TexturePtr &frost = cfg.glass ? wp.glass().frost : nullptr;
	const TexturePtr &old_frost = cfg.glass ? wp.old_glass().frost : nullptr;
	// The new layer fades in over the old one. With glass the old one stays whole until it is covered (the sharp wallpaper never
	// shows through half-way); with the colour alone both fade.
	if (f < 1) layer(card, old_frost, old_look, frost || old_frost ? 1 : 1 - f);
	layer(card, frost, look, f < 1 ? f : 1);
	if (focus > 0) gfx::rect(card, CARD_RADIUS, color(look.focus).with_alpha(look.focus.a * focus));
	// the rim: a bright edge along the top and a faint one all round (inset box-shadows)
	gfx::ring({card.x + 1, card.y + 1, card.w - 2, card.h - 2}, CARD_RADIUS - 1, 1, {1, 1, 1, 0.07f});
	gfx::push_clip({card.x, card.y, card.w, CARD_RADIUS * 0.6f});
	gfx::ring({card.x + 1, card.y + 1, card.w - 2, card.h + 40}, CARD_RADIUS - 1, 1, {1, 1, 1, 0.28f});
	gfx::pop_clip();
}
