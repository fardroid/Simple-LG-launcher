// The plate under a card (.card, .frost and components/Frost.tsx): its soft shadow, the frosted glass (the blurred wallpaper,
// lined up with the picture behind the card), the plate's colour over it, the focus layer, and the rim.
#pragma once

#include "data/config.h"
#include "gfx/gfx.h"
#include "ui/plates.h"
#include "ui/wallpaper.h"

constexpr float CARD_RADIUS = 36;
/** The cards with liquid glass are rounder, so that the glass's bevel (never wider than the corner's radius) can be wide:
 *  archisvaze/liquid-glass's default, chosen on a slider page (radius 60, bevel 60, thickness 80, index 3). */
constexpr float LIQUID_RADIUS = 60;

/** `look` / `old_look`: the plates' colour for the current and the previous picture (they change in the wallpaper's fade).
 *  `focus`: 0-1, the stronger layer of a card the remote is on. `fade`: 0-1, how far the change from the previous picture's
 *  plate has come (Wallpaper::fade on the picture of the board, Wallpaper::change on a card drawn over it every frame). */
void draw_plate (Rect card, const Wallpaper &wallpaper, const UserConfig &cfg, const PlateLook &look, const PlateLook &old_look,
	float focus, float fade);

/** The corners of the cards (and the banners) under these settings: rounder with liquid glass. */
float card_radius (const UserConfig &cfg);
