// Design tokens (theme/tokens.ts): colours per theme (sizes and motion: ui/tokens.h).
#pragma once

#include "gfx/gfx.h"
#include "ui/tokens.h"

#include <string>
#include <vector>

struct Theme {
	Color bg, wallpaperStart, wallpaperMid, wallpaperEnd;
	Color textPrimary, textSecondary, textMuted;
	Color tileBg, tileBgFocus, tileBorder, tileBorderFocus;
	Color badge, badgeText, liftShadow;
	Color buttonBorder, buttonActiveBg, buttonActiveText;
	Color switchTrack, switchTrackOn, switchKnob, switchKnobOn;
	Color danger, overlay, pickerBg;
	Color accent;                // today, the weather curve, an event now
	Color plate;                 // the colour of a dark plate
	Color screen;                // the background of Settings and the screens opened from it
};

const Theme &theme_named (const std::string &name);
/** How opaque the background of Settings and the screens opened from it is: the wallpaper shows a little through it. */
constexpr float SCREEN_OPACITY = 0.9f;

/** The theme the cards are drawn in (the home screen sets it from the settings before it draws them). */
void set_current_theme (const std::string &name);
const Theme &current_theme ();
std::vector<std::string> theme_ids ();
