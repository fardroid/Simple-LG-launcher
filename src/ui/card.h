// The plate under a card (.card, .frost and components/Frost.tsx): its soft shadow, the frosted glass (the blurred wallpaper,
// lined up with the picture behind the card), the plate's colour over it, the focus layer, and the rim.
#pragma once

#include "data/config.h"
#include "gfx/gfx.h"
#include "ui/plates.h"
#include "ui/wallpaper.h"

constexpr float CARD_RADIUS = 36;

/** `look` / `old_look`: the plates' colour for the current and the previous picture (they change in the wallpaper's fade).
 *  `focus`: 0-1, the stronger layer of a card the remote is on. */
void draw_plate (Rect card, const Wallpaper &wallpaper, const UserConfig &cfg, const PlateLook &look, const PlateLook &old_look,
	float focus, double now);
