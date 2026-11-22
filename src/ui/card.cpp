#include "ui/card.h"

#include <algorithm>

namespace {

Color color (const Rgba &c) { return {c.r, c.g, c.b, c.a}; }

// The liquid glass's shape (Settings → Appearance → Glass: Liquid): archisvaze/liquid-glass's default, picked on a page of sliders
// on 2026-11-23: a squircle bevel as wide as the cards' corners round (60 px; the shader keeps it a pixel under), 80 px thick, the
// colours parted fully, index 3.
const gfx::LiquidGlass LIQUID{60, 80, 1, 3};

bool liquid_on (const UserConfig &cfg) { return cfg.glass && cfg.glassKind == "liquid" && gfx::liquid_available(); }

// One layer of the plate: the glass (when there is one), then the colour over it, at `opacity`.
void layer (Rect card, float radius, const TexturePtr &frost, const PlateLook &look, float opacity, float dim, bool liquid, bool shadow) {
	if (opacity <= 0) return;
	if (frost && liquid) gfx::liquid(*frost, card, radius, opacity, 1 - dim, LIQUID);
	else if (frost) {
		// the 960 x 540 glass is stretched over the whole screen; the card shows the part behind where it is on the screen now (a
		// card growing in focus does not grow the picture behind it)
		float kx = frost->w / 1920.f, ky = frost->h / 1080.f;
		Rect at = gfx::grown(card);
		// a dimmed wallpaper: its glass is as much darker
		gfx::image(*frost, card, opacity, radius, {1, 1, 1, 1}, 1 - dim, {at.x * kx, at.y * ky, at.w * kx, at.h * ky});
	}
	// the card's shadow falls on the picture behind it, which shows through: under the card too, not only round it (a light card
	// in a dark frame looked lit from inside). Even all over, as dark as it is at the card's sides: a shadow that thinned out
	// towards the card's edges, as it does under a real card, made the edges light, like sanded glass (seen on a page of sliders)
	if (shadow) gfx::rect(card, radius, {0, 0, 0, 0.45f * 0.5f * opacity});
	Color c = color(look.bg);
	gfx::rect(card, radius, c.with_alpha(c.a * opacity));
}

}  // namespace

void draw_plate (Rect card, const Wallpaper &wp, const UserConfig &cfg, const PlateLook &look, const PlateLook &old_look, float focus, float f) {
	if (card.w <= 0 || card.h <= 0) return;
	const float radius = card_radius(cfg);
	if (cfg.shadows) gfx::shadow(card, radius, 34, 70, {0, 0, 0, 0.45f});
	const TexturePtr &frost = cfg.glass ? wp.glass().frost : nullptr;
	const TexturePtr &old_frost = cfg.glass ? wp.old_glass().frost : nullptr;
	bool liquid = liquid_on(cfg);
	// The new layer fades in over the old one. With glass the old one stays whole until it is covered (the sharp wallpaper never
	// shows through half-way); with the colour alone both fade.
	if (f < 1) layer(card, radius, old_frost, old_look, frost || old_frost ? 1 : 1 - f, wp.dim(), liquid, cfg.shadows);
	layer(card, radius, frost, look, f < 1 ? f : 1, wp.dim(), liquid, cfg.shadows);
	if (focus > 0) gfx::rect(card, radius, color(look.focus).with_alpha(look.focus.a * focus));
	// liquid glass: only the lens, no rim of light (it showed the glass's volume)
	if (liquid) return;
	// the rim: a bright edge along the top and a faint one all round (inset box-shadows)
	gfx::ring({card.x + 1, card.y + 1, card.w - 2, card.h - 2}, CARD_RADIUS - 1, 1, {1, 1, 1, 0.07f});
	gfx::push_clip({card.x, card.y, card.w, CARD_RADIUS * 0.6f});
	gfx::ring({card.x + 1, card.y + 1, card.w - 2, card.h + 40}, CARD_RADIUS - 1, 1, {1, 1, 1, 0.28f});
	gfx::pop_clip();
}

float card_radius (const UserConfig &cfg) { return liquid_on(cfg) ? LIQUID_RADIUS : CARD_RADIUS; }
