#include "ui/theme.h"

#include "ui/themes.inc"

#include <map>

namespace {

Color h (const char *s) { return Color::hex(s); }

Theme make (const ThemeColors &c) {
	Theme t;
	t.bg = h(c.bg); t.wallpaperStart = h(c.start); t.wallpaperMid = h(c.mid); t.wallpaperEnd = h(c.end);
	t.textPrimary = h(c.text1); t.textSecondary = h(c.text2); t.textMuted = h(c.text3);
	t.tileBg = h(c.tile); t.tileBgFocus = h(c.tile_focus); t.tileBorder = h(c.tile_border); t.tileBorderFocus = h(c.ring);
	t.badge = h(c.text1); t.badgeText = h(c.bg); t.liftShadow = {0, 0, 0, std::string(c.id) == "black" ? 0.8f : 0.6f};
	t.buttonBorder = h(c.tile_border); t.buttonActiveBg = h(c.text1); t.buttonActiveText = h(c.bg);
	t.switchTrack = h(c.tile_border); t.switchTrackOn = h(c.text1); t.switchKnob = h(c.text2); t.switchKnobOn = h(c.bg);
	t.danger = h("#EF7A73"); t.overlay = h(c.screen).with_alpha(0.86f); t.pickerBg = h(c.screen);
	t.accent = h(c.accent); t.plate = h(c.plate); t.screen = h(c.screen);
	return t;
}

std::string current_name = "dusk";

}  // namespace

const Theme &theme_named (const std::string &name) {
	static std::map<std::string, Theme> made;
	const ThemeColors &c = theme_colors(name);
	auto it = made.find(c.id);
	if (it == made.end()) it = made.emplace(c.id, make(c)).first;
	return it->second;
}

void set_current_theme (const std::string &name) { current_name = name; }
const Theme &current_theme () { return theme_named(current_name); }

std::vector<std::string> theme_ids () {
	std::vector<std::string> out;
	for (const auto &t : THEMES) out.push_back(t.id);
	return out;
}
