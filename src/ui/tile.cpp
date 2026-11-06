#include "ui/tile.h"

#include "data/apps.h"
#include "gfx/text.h"
#include "sys/fs.h"
#include "ui/icons.h"
#include "ui/tokens.h"

#include <algorithm>
#include <cmath>

void draw_tile_art (const TileModel &m, Rect frame, float r, float s, int ts, int icon_scale, const Theme &t, Color box) {
	TexturePtr art = !m.imagePath.empty() ? icons::get(m.imagePath, (int)(ts * motion::FOCUS_SCALE)) : nullptr;
	if (!m.color.empty() && !art) box = Color::hex(m.color.c_str());
	gfx::rect(frame, r, box);
	if (art) {
		// object-fit: cover
		float k = std::max(frame.w / art->w, frame.h / art->h);
		float sw = frame.w / k, sh = frame.h / k;
		gfx::image(*art, frame, 1, r, {1, 1, 1, 1}, 1, {(art->w - sw) / 2, (art->h - sh) / 2, sw, sh});
	} else if (!m.glyphPath.empty()) {
		TexturePtr g = icons::get(m.glyphPath, (int)(ts * 0.64f * motion::FOCUS_SCALE));
		if (g) {
			// object-fit: contain in the middle 64%, lightened (filter: brightness(2.4))
			Rect in{frame.x + frame.w * 0.18f, frame.y + frame.h * 0.18f, frame.w * 0.64f, frame.h * 0.64f};
			float k = std::min(in.w / g->w, in.h / g->h);
			float gw = g->w * k, gh = g->h * k;
			gfx::image(*g, {in.x + (in.w - gw) / 2, in.y + (in.h - gh) / 2, gw, gh}, 1, 0, {1, 1, 1, 1}, 2.4f);
		}
	} else if (!m.glyph.empty()) {
		// .tile__glyph: the line icon in the middle half, in the text colour
		TexturePtr g = icons::get(join_path(package_dir(), "glyphs/" + m.glyph + ".png"), (int)(ts * 0.5f * motion::FOCUS_SCALE));
		// white on a tile of its own colour (the system tiles, the inputs), else in the text colour
		if (g) gfx::image(*g, {frame.x + frame.w * 0.25f, frame.y + frame.h * 0.25f, frame.w * 0.5f, frame.h * 0.5f}, 1, 0, m.color.empty() ? t.textPrimary : Color{1, 1, 1, 1});
	} else {
		// the monogram, drawn at the focused size and scaled down
		std::string letter;
		if (!m.label.empty()) {
			unsigned char c = (unsigned char)m.label[0];
			size_t n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4;
			letter = m.label.substr(0, n);
			if (letter.size() == 1) letter[0] = (char)toupper((unsigned char)letter[0]);
		}
		TextStyle st{Face::ui, (int)std::lround(64 * motion::FOCUS_SCALE * icon_scale / 100.0), 600, 0};
		const TextLine &l = text::line(letter, st);
		if (l.tex) {
			float k = s / (float)motion::FOCUS_SCALE;
			float lw = l.tex->w * k, lh = l.tex->h * k;
			gfx::image(*l.tex, {frame.x + (frame.w - lw) / 2, frame.y + (frame.h - lh) / 2, lw, lh}, 1, 0, t.textPrimary);
		}
	}
}
