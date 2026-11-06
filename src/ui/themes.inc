// The colour themes (Settings → Wallpaper → Colour theme), in one table for the drawing (ui/theme.cpp) and for the plates'
// brightness (ui/plates.cpp). The first three are the web version's (theme/tokens.ts); the others are the native version's own.
//   bg: under everything; start/mid/end: the default background's gradient (155°, mid at 55%)
//   text1-3: primary, secondary, muted; tile/tile_focus/tile_border/ring: the app tiles; accent: today, the weather curve, the
//   time of an event now, a switch that is on; plate: the colour of a dark plate; screen: the background of Settings
struct ThemeColors {
	const char *id, *en, *ru;
	const char *bg, *start, *mid, *end;
	const char *text1, *text2, *text3;
	const char *tile, *tile_focus, *tile_border, *ring;
	const char *accent, *plate, *screen;
};

static const ThemeColors THEMES[] = {
	{"dusk", "Evening sky", "Вечернее небо", "#0a0d1f", "#0b1636", "#26357a", "#a2506f",
		"#F2F5F8", "#B9C6D3", "#7F90A1", "#1b2142", "#2a3260", "#3a4478", "#F2F5F8", "#ffc36b", "#0e1228", "#08090e"},
	{"black", "OLED black", "Чёрная (OLED)", "#000000", "#000000", "#000000", "#000000",
		"#F2F2F2", "#A8A8A8", "#6A6A6A", "#101010", "#1D1D1D", "#2A2A2A", "#F2F2F2", "#ffc36b", "#0a0a0a", "#000000"},
	{"grey", "Calm blue-grey", "Серо-голубая", "#101a24", "#1c2b3a", "#25384c", "#101a24",
		"#F2F5F8", "#B9C6D3", "#7F90A1", "#1a2733", "#243546", "#34485c", "#F2F5F8", "#9fd0ff", "#0f1a26", "#0a1118"},
	{"forest", "Forest", "Лес", "#08140f", "#0d2a1d", "#1f4d36", "#0a1a12",
		"#EEF5F0", "#B3C9BB", "#7A9585", "#13261c", "#1d3a2b", "#2c4f3d", "#EEF5F0", "#a8e07a", "#0c1f15", "#07110c"},
	{"ocean", "Ocean", "Океан", "#04121a", "#062a3d", "#0e5a70", "#0a2233",
		"#EDF8FB", "#A9CBD6", "#6F98A6", "#0c2633", "#123a4c", "#1d5266", "#EDF8FB", "#5fdbe6", "#071c28", "#040e15"},
	{"sunset", "Sunset", "Закат", "#160a0a", "#2a0f1a", "#8a3b2a", "#d9853a",
		"#FFF3EC", "#E0BFAE", "#A6857A", "#2a1614", "#3d201c", "#5a3028", "#FFF3EC", "#ffb35c", "#24100e", "#120808"},
	{"lavender", "Lavender", "Лаванда", "#110d1f", "#1c1538", "#4b3a8a", "#b08ad6",
		"#F5F0FF", "#C8BEE6", "#8D82AD", "#1e1836", "#2c2450", "#3e3470", "#F5F0FF", "#d9bcff", "#171230", "#0d0a18"},
	{"graphite", "Graphite", "Графит", "#121212", "#1b1b1d", "#2c2c30", "#141416",
		"#F0F0F0", "#B5B5B5", "#7A7A7A", "#1c1c1e", "#2a2a2d", "#3a3a3e", "#F0F0F0", "#ff9f0a", "#141416", "#0e0e10"},
};

static const ThemeColors &theme_colors (const std::string &id) {
	for (const auto &t : THEMES) if (id == t.id) return t;
	return THEMES[0];
}
