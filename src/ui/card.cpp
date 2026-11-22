#include "ui/card.h"

#include <algorithm>

namespace {

Color color (const Rgba &c) { return {c.r, c.g, c.b, c.a}; }

// The liquid glass's shape (Settings → Appearance → Glass: Liquid): a squircle bevel as wide as the cards' corners round (35 px),
// 60 px thick, the colours parted fully, refracting as strongly as archisvaze/liquid-glass's default (index 3).
const gfx::LiquidGlass LIQUID{35, 60, 1, 3};

// One layer of the plate: the glass (when there is one), then the colour over it, at `opacity`.
void layer (Rect card, const TexturePtr &frost, const PlateLook &look, float opacity, float dim, bool liquid, bool shadow) {
	if (opacity <= 0) return;
	if (frost && liquid) gfx::liquid(*frost, card, CARD_RADIUS, opacity, 1 - dim, LIQUID);
	else if (frost) {
		// the 960 x 540 glass is stretched over the whole screen; the card shows the part behind where it is on the screen now (a
		// card growing in focus does not grow the picture behind it)
		float kx = frost->w / 1920.f, ky = frost->h / 1080.f;
		Rect at = gfx::grown(card);
		// a dimmed wallpaper: its glass is as much darker
		gfx::image(*frost, card, opacity, CARD_RADIUS, {1, 1, 1, 1}, 1 - dim, {at.x * kx, at.y * ky, at.w * kx, at.h * ky});
	}
	// the card's shadow falls on the picture behind it, which shows through: under the card too, not only round it (a light card
	// in a dark frame looked lit from inside)
	if (shadow) gfx::shadow_under(card, CARD_RADIUS, 34, 70, {0, 0, 0, 0.45f * opacity});
	Color c = color(look.bg);
	gfx::rect(card, CARD_RADIUS, c.with_alpha(c.a * opacity));
}

}  // namespace

void draw_plate (Rect card, const Wallpaper &wp, const UserConfig &cfg, const PlateLook &look, const PlateLook &old_look, float focus, float f) {
	if (card.w <= 0 || card.h <= 0) return;
	if (cfg.shadows) gfx::shadow(card, CARD_RADIUS, 34, 70, {0, 0, 0, 0.45f});
	const TexturePtr &frost = cfg.glass ? wp.glass().frost : nullptr;
	const TexturePtr &old_frost = cfg.glass ? wp.old_glass().frost : nullptr;
	bool liquid = cfg.glass && cfg.glassKind == "liquid" && gfx::liquid_available();
	// The new layer fades in over the old one. With glass the old one stays whole until it is covered (the sharp wallpaper never
	// shows through half-way); with the colour alone both fade.
	if (f < 1) layer(card, old_frost, old_look, frost || old_frost ? 1 : 1 - f, wp.dim(), liquid, cfg.shadows);
	layer(card, frost, look, f < 1 ? f : 1, wp.dim(), liquid, cfg.shadows);
	if (focus > 0) gfx::rect(card, CARD_RADIUS, color(look.focus).with_alpha(look.focus.a * focus));
	// liquid glass: only the lens, no rim of light (it showed the glass's volume)
	if (liquid) return;
	// the rim: a bright edge along the top and a faint one all round (inset box-shadows)
	gfx::ring({card.x + 1, card.y + 1, card.w - 2, card.h - 2}, CARD_RADIUS - 1, 1, {1, 1, 1, 0.07f});
	gfx::push_clip({card.x, card.y, card.w, CARD_RADIUS * 0.6f});
	gfx::ring({card.x + 1, card.y + 1, card.w - 2, card.h + 40}, CARD_RADIUS - 1, 1, {1, 1, 1, 0.28f});
	gfx::pop_clip();
}
