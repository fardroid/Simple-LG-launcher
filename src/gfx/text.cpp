#include "gfx/text.h"

#include "core/log.h"
#include "sys/fs.h"

#include <SDL.h>
#include <SDL_ttf.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_map>

namespace {

const std::string TV_FONTS = "/usr/share/fonts/";
// characters the main font lacks are looked for in these, in order
const char *FALLBACKS[] = {"LG_Smart_UI-Regular.ttf", "LG_Smart_UI_SC.ttf", "DroidSansFallback.ttf", "LG_Display_JP.ttf", "NanumGothic-Bold.ttf", "NotoEmoji-Regular.ttf"};

std::string bundled_dir;
// Light, the letters placed to a fraction of a pixel. With "normal" the TV's TrueType font got stems of one letter in different
// widths (the right stem of "п" spread over two faint pixels, measured on screenshots); plain light, each letter starting on a
// whole pixel, still left the stems of the news and the quote in uneven stripes (seen on the TV). This keeps them even.
int hinting = TTF_HINTING_LIGHT_SUBPIXEL;
std::map<std::pair<std::string, int>, TTF_Font *> fonts;      // (file, px)

struct Cached { TextLine line; unsigned used = 0; };
std::unordered_map<std::string, Cached> cache;
unsigned tick = 0;

// The file of LG Smart UI nearest to a CSS weight.
std::string lg_smart_ui (int weight, bool italic) {
	const char *w = weight <= 300 ? "Light" : weight <= 400 ? "Regular" : weight <= 600 ? "SemiBold" : "Bold";
	return TV_FONTS + "LG_Smart_UI-" + w + (italic ? "Oblique" : "") + ".ttf";
}

// A bundled family: <dir>/<family>-<weight>.ttf, the nearest weight there is.
std::string bundled (const char *family, int weight) {
	if (bundled_dir.empty()) return "";
	static const int WEIGHTS[] = {200, 300, 400, 500, 600, 700};
	std::string best;
	int best_d = 1000;
	for (int w : WEIGHTS) {
		std::string f = bundled_dir + "/" + family + "-" + std::to_string(w) + ".ttf";
		int d = std::abs(w - weight);
		if (d < best_d && file_exists(f)) { best = f; best_d = d; }
	}
	return best;
}

std::string file_of (const TextStyle &st) {
	std::string f;
	switch (st.face) {
		case Face::inter: f = bundled("inter", st.weight); break;
		case Face::poppins: f = bundled("poppins", st.weight); break;
		case Face::oswald: f = bundled("oswald", st.weight); break;
		case Face::mono: f = bundled("roboto-mono", st.weight); break;
		case Face::serif: f = bundled("cormorant", st.weight); break;
		default: break;
	}
	return f.empty() ? lg_smart_ui(st.weight, st.italic) : f;
}

TTF_Font *font (const std::string &file, int px) {
	auto key = std::make_pair(file, px);
	auto it = fonts.find(key);
	if (it != fonts.end()) return it->second;
	TTF_Font *f = TTF_OpenFont(file.c_str(), px);
	if (!f) logf("font %s %d: %s", file.c_str(), px, TTF_GetError());
	else TTF_SetFontHinting(f, hinting);
	fonts[key] = f;
	return f;
}

std::vector<Uint32> codepoints (const std::string &s) {
	std::vector<Uint32> out;
	for (size_t i = 0; i < s.size();) {
		unsigned char c = (unsigned char)s[i];
		Uint32 cp;
		int n;
		if (c < 0x80) { cp = c; n = 1; }
		else if ((c >> 5) == 6) { cp = c & 0x1F; n = 2; }
		else if ((c >> 4) == 14) { cp = c & 0x0F; n = 3; }
		else if ((c >> 3) == 30) { cp = c & 0x07; n = 4; }
		else { i++; continue; }
		for (int k = 1; k < n && i + k < s.size(); k++) cp = (cp << 6) | ((unsigned char)s[i + k] & 0x3F);
		out.push_back(cp);
		i += n;
	}
	return out;
}

std::string utf8 (const std::vector<Uint32> &cps, size_t from, size_t to) {
	std::string out;
	for (size_t i = from; i < to; i++) {
		Uint32 c = cps[i];
		if (c < 0x80) out += (char)c;
		else if (c < 0x800) { out += (char)(0xC0 | (c >> 6)); out += (char)(0x80 | (c & 0x3F)); }
		else if (c < 0x10000) { out += (char)(0xE0 | (c >> 12)); out += (char)(0x80 | ((c >> 6) & 0x3F)); out += (char)(0x80 | (c & 0x3F)); }
		else { out += (char)(0xF0 | (c >> 18)); out += (char)(0x80 | ((c >> 12) & 0x3F)); out += (char)(0x80 | ((c >> 6) & 0x3F)); out += (char)(0x80 | (c & 0x3F)); }
	}
	return out;
}

// The font for each character: the main one when it has the glyph, else the first fallback that has it.
std::vector<TTF_Font *> fonts_for (const std::vector<Uint32> &cps, TTF_Font *main, int px) {
	std::vector<TTF_Font *> out;
	for (Uint32 cp : cps) {
		TTF_Font *f = main;
		if (cp > 0x7F && cp <= 0xFFFF && !TTF_GlyphIsProvided(main, (Uint16)cp)) {
			for (const char *fb : FALLBACKS) {
				TTF_Font *other = font(TV_FONTS + fb, px);
				if (other && TTF_GlyphIsProvided(other, (Uint16)cp)) { f = other; break; }
			}
		}
		out.push_back(f);
	}
	return out;
}

// Pieces of the line, each in one font: drawn one after another on the main font's baseline.
struct Piece { SDL_Surface *s; int x; int ascent; };

// The line in memory (white, its alpha) and its metrics; out.tex is left empty.
TextLine render (const std::string &s, const TextStyle &st, Pixels &px) {
	TextLine out;
	px = Pixels();
	TTF_Font *main = font(file_of(st), st.size);
	if (!main || s.empty()) return out;
	// a bundled font has no italic of its own: FreeType slants it (the TV's font has its Oblique files)
	bool slant = st.italic && file_of(st).find("Oblique") == std::string::npos;
	TTF_SetFontStyle(main, slant ? TTF_STYLE_ITALIC : TTF_STYLE_NORMAL);
	auto cps = codepoints(s);
	auto per = fonts_for(cps, main, st.size);
	std::vector<Piece> pieces;
	int x = 0;
	const SDL_Color white{255, 255, 255, 255};
	if (st.spacing != 0) {
		// letter-spacing: each character on its own, its advance plus the spacing
		for (size_t i = 0; i < cps.size(); i++) {
			int minx, maxx, miny, maxy, adv = 0;
			if (TTF_GlyphMetrics32(per[i], cps[i], &minx, &maxx, &miny, &maxy, &adv) != 0) continue;
			SDL_Surface *g = TTF_RenderGlyph32_Blended(per[i], cps[i], white);
			if (g) pieces.push_back({g, x, TTF_FontAscent(per[i])});
			x += adv + (int)std::lround(st.spacing);
		}
		x -= (int)std::lround(st.spacing);
	} else {
		size_t from = 0;
		for (size_t i = 1; i <= cps.size(); i++) {
			if (i < cps.size() && per[i] == per[from]) continue;
			std::string run = utf8(cps, from, i);
			SDL_Surface *r = TTF_RenderUTF8_Blended(per[from], run.c_str(), white);
			if (r) {
				pieces.push_back({r, x, TTF_FontAscent(per[from])});
				int w = 0, h = 0;
				TTF_SizeUTF8(per[from], run.c_str(), &w, &h);
				x += w;
			}
			from = i;
		}
	}
	out.ascent = TTF_FontAscent(main);
	out.h = TTF_FontHeight(main);
	int top_extra = 0, bottom = out.h;
	for (auto &p : pieces) {
		top_extra = std::max(top_extra, p.ascent - out.ascent);
		bottom = std::max(bottom, out.ascent - p.ascent + p.s->h);
	}
	out.ascent += top_extra;
	out.h = bottom + top_extra;
	out.w = std::max(1, x);
	px.w = std::max(out.w, 1);
	for (auto &p : pieces) px.w = std::max(px.w, p.x + p.s->w);
	px.h = std::max(out.h, 1);
	px.rgba.assign((size_t)px.w * px.h * 4, 0);
	for (auto &p : pieces) {
		SDL_Surface *c = SDL_ConvertSurfaceFormat(p.s, SDL_PIXELFORMAT_ABGR8888, 0);
		SDL_FreeSurface(p.s);
		if (!c) continue;
		int dy = out.ascent - p.ascent;
		for (int y = 0; y < c->h; y++) {
			int ty = y + dy;
			if (ty < 0 || ty >= px.h) continue;
			const unsigned char *row = (const unsigned char *)c->pixels + (size_t)y * c->pitch;
			for (int xx = 0; xx < c->w; xx++) {
				int tx = p.x + xx;
				if (tx < 0 || tx >= px.w) continue;
				unsigned char *o = &px.rgba[((size_t)ty * px.w + tx) * 4];
				unsigned char a = row[xx * 4 + 3];
				o[0] = o[1] = o[2] = 255;
				o[3] = std::max(o[3], a);
			}
		}
		SDL_FreeSurface(c);
	}
	// where the ink is
	out.ink_top = px.h;
	out.ink_bottom = 0;
	for (int y = 0; y < px.h; y++) {
		for (int xx = 0; xx < px.w; xx++) {
			if (px.rgba[((size_t)y * px.w + xx) * 4 + 3] > 40) {
				out.ink_top = std::min(out.ink_top, y);
				out.ink_bottom = std::max(out.ink_bottom, y + 1);
				break;
			}
		}
	}
	if (out.ink_bottom == 0) out.ink_top = 0;
	out.w = px.w;
	return out;
}

TextLine render (const std::string &s, const TextStyle &st) {
	Pixels px;
	TextLine out = render(s, st, px);
	if (px.w > 0 && px.h > 0) out.tex = make_texture(px);
	return out;
}

// A CSS blur of `blur` px on the alpha (the colour stays white).
// Three box passes each way over one channel (a gaussian of the same spread), r px each side
void alpha_blur (std::vector<int> &a, int w, int h, int r) {
	std::vector<int> buf((size_t)std::max(w, h));
	auto pass = [&] (int len, int lines, size_t step, size_t line_step) {
		const int span = 2 * r + 1;
		for (int l = 0; l < lines; l++) {
			int *base = &a[l * line_step];
			for (int i = 0; i < len; i++) buf[i] = base[i * step];
			auto at = [&] (int i) { return i < 0 || i >= len ? 0 : buf[i]; };     // nothing beyond the margin
			int sum = 0;
			for (int i = -r; i <= r; i++) sum += at(i);
			for (int i = 0; i < len; i++) {
				base[i * step] = sum / span;
				sum += at(i + r + 1) - at(i - r);
			}
		}
	};
	for (int i = 0; i < 3; i++) {
		pass(w, h, 1, (size_t)w);
		pass(h, w, (size_t)w, 1);
	}
}

std::string key_of (const std::string &s, const TextStyle &st, float blur) {
	char k[96];
	snprintf(k, sizeof k, "%d|%d|%d|%.1f|%.1f|%d|", (int)st.face, st.size, st.weight, st.spacing, blur, st.italic ? 1 : 0);
	return k + s;
}

}  // namespace

namespace text {

void set_hinting (const std::string &mode) {
	hinting = mode == "light" ? TTF_HINTING_LIGHT : mode == "subpixel" ? TTF_HINTING_LIGHT_SUBPIXEL : mode == "mono" ? TTF_HINTING_MONO : mode == "none" ? TTF_HINTING_NONE : TTF_HINTING_NORMAL;
	logf("text: hinting %s", mode.c_str());
}

void init (const std::string &dir) {
	bundled_dir = dir;
	if (TTF_Init() != 0) logf("TTF_Init: %s", TTF_GetError());
}

const TextLine &line (const std::string &s, const TextStyle &st) {
	std::string k = key_of(s, st, 0);
	auto it = cache.find(k);
	if (it == cache.end()) it = cache.emplace(k, Cached{render(s, st), 0}).first;
	it->second.used = ++tick;
	return it->second.line;
}

int shadow_pad (float blur) { return (int)std::ceil(blur * 1.5f) + 2; }

const TextLine &shadow (const std::string &s, const TextStyle &st, float blur) {
	std::string k = key_of(s, st, blur);
	auto it = cache.find(k);
	if (it == cache.end()) {
		// the line's alpha with a margin round it, blurred on the CPU once. A wide blur (the clock's 44 px) loses nothing at a
		// quarter of the size or less, and the big digits made full size took long enough to hold up a frame at every new size.
		Pixels base;
		TextLine out = render(s, st, base);
		int scale = blur >= 8 ? std::min(8, (int)(blur / 4)) : 1;
		int pad = (shadow_pad(blur) + scale - 1) / scale * scale;
		int sw = (base.w + 2 * pad + scale - 1) / scale, sh = (base.h + 2 * pad + scale - 1) / scale;
		std::vector<int> a((size_t)sw * sh, 0);
		for (int y = 0; y < base.h; y++)
			for (int x = 0; x < base.w; x++)
				a[(size_t)((y + pad) / scale) * sw + (x + pad) / scale] += base.rgba[((size_t)y * base.w + x) * 4 + 3];
		for (auto &v : a) v /= scale * scale;
		alpha_blur(a, sw, sh, std::max(1, (int)std::lround(blur / 2 * 0.9 / scale)));
		Pixels px;
		px.w = sw;
		px.h = sh;
		px.rgba.assign((size_t)sw * sh * 4, 255);
		for (size_t i = 0; i < a.size(); i++) px.rgba[i * 4 + 3] = (unsigned char)a[i];
		if (px.w > 0 && px.h > 0) out.tex = make_texture(px);
		out.pad = pad;
		out.scale = scale;
		it = cache.emplace(k, Cached{out, 0}).first;
	}
	it->second.used = ++tick;
	return it->second.line;
}

int width (const std::string &s, const TextStyle &st) {
	// a line already drawn knows its width; otherwise it is measured without drawing it (font by font, as render does)
	auto it = cache.find(key_of(s, st, 0));
	if (it != cache.end()) return it->second.line.w;
	TTF_Font *main = font(file_of(st), st.size);
	if (!main || s.empty()) return 0;
	bool slant = st.italic && file_of(st).find("Oblique") == std::string::npos;
	TTF_SetFontStyle(main, slant ? TTF_STYLE_ITALIC : TTF_STYLE_NORMAL);
	auto cps = codepoints(s);
	auto per = fonts_for(cps, main, st.size);
	int x = 0;
	if (st.spacing != 0) {
		for (size_t i = 0; i < cps.size(); i++) {
			int minx, maxx, miny, maxy, adv = 0;
			if (TTF_GlyphMetrics32(per[i], cps[i], &minx, &maxx, &miny, &maxy, &adv) == 0) x += adv + (int)std::lround(st.spacing);
		}
		return std::max(1, x - (int)std::lround(st.spacing));
	}
	size_t from = 0;
	for (size_t i = 1; i <= cps.size(); i++) {
		if (i < cps.size() && per[i] == per[from]) continue;
		int w = 0, h = 0;
		TTF_SizeUTF8(per[from], utf8(cps, from, i).c_str(), &w, &h);
		x += w;
		from = i;
	}
	return std::max(1, x);
}

int height (const TextStyle &st) {
	TTF_Font *f = font(file_of(st), st.size);
	return f ? TTF_FontHeight(f) : st.size;
}

std::vector<std::string> wrap (const std::string &s, const TextStyle &st, int max_w, int max_lines) {
	std::vector<std::string> lines;
	bool more = false;
	size_t start = 0;
	while (start <= s.size() && !more) {
		size_t nl = s.find('\n', start);
		std::string para = s.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
		std::vector<std::string> words;
		size_t i = 0;
		while (i < para.size()) {
			size_t j = para.find(' ', i);
			if (j == std::string::npos) j = para.size();
			if (j > i) words.push_back(para.substr(i, j - i));
			i = j + 1;
		}
		std::string cur;
		size_t w = 0;
		for (; w < words.size(); w++) {
			std::string next = cur.empty() ? words[w] : cur + " " + words[w];
			if (width(next, st) <= max_w || cur.empty()) { cur = next; continue; }
			if ((int)lines.size() == max_lines) { more = true; break; }
			lines.push_back(cur);
			cur = words[w];
		}
		if (!more) {
			if ((int)lines.size() == max_lines) more = !cur.empty() || nl != std::string::npos;
			else if (!cur.empty()) lines.push_back(cur);
		}
		if (nl == std::string::npos) break;
		start = nl + 1;
		if ((int)lines.size() == max_lines && start <= s.size()) { more = more || start < s.size(); break; }
	}
	// what does not fit is cut: a line too long for the width, and the last line when anything was left out, end with "…"
	for (size_t i = 0; i < lines.size(); i++) {
		bool last = i + 1 == lines.size();
		if (width(lines[i], st) <= max_w && !(last && more)) continue;
		auto cps = codepoints(lines[i]);
		while (!cps.empty() && width(utf8(cps, 0, cps.size()) + "…", st) > max_w) cps.pop_back();
		lines[i] = utf8(cps, 0, cps.size()) + "…";
	}
	return lines;
}

std::string ellipsize (const std::string &s, const TextStyle &st, int max_w) {
	if (width(s, st) <= max_w) return s;
	auto cps = codepoints(s);
	while (!cps.empty() && width(utf8(cps, 0, cps.size()) + "…", st) > max_w) cps.pop_back();
	return utf8(cps, 0, cps.size()) + "…";
}

std::string upper (const std::string &s) {
	auto cps = codepoints(s);
	for (auto &c : cps) {
		if (c >= 'a' && c <= 'z') c -= 32;
		else if (c >= 0xE0 && c <= 0xFE && c != 0xF7) c -= 32;            // Latin-1
		else if (c >= 0x430 && c <= 0x44F) c -= 32;                         // Cyrillic а-я
		else if (c >= 0x450 && c <= 0x45F) c -= 80;                         // ѐ-џ
		else if (c >= 0x3B1 && c <= 0x3C9 && c != 0x3C2) c -= 32;           // Greek
		// Latin Extended-A: pairs of a capital and its small letter, the capital first (even) or second (odd) by range
		else if (((c >= 0x100 && c <= 0x137) || (c >= 0x14A && c <= 0x177)) && (c & 1) && c != 0x131) c -= 1;
		else if (((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E)) && !(c & 1)) c -= 1;
	}
	return utf8(cps, 0, cps.size());
}

void trim () {
	if (cache.size() < 400) return;
	for (auto it = cache.begin(); it != cache.end();) {
		if (tick - it->second.used > 2000) it = cache.erase(it);
		else ++it;
	}
}

void draw (const std::string &s, const TextStyle &st, float x, float y, Color c, float opacity, const Color *to) {
	const TextLine &l = line(s, st);
	if (!l.tex) return;
	gfx::image(*l.tex, {x, y, (float)l.tex->w, (float)l.tex->h}, opacity, 0, c, 1, {}, to);
}

void draw_shadow (const std::string &s, const TextStyle &st, float x, float y, float dy, float blur, Color c) {
	const TextLine &l = shadow(s, st, blur);
	if (!l.tex) return;
	gfx::image(*l.tex, {x - l.pad, y - l.pad + dy, (float)l.tex->w * l.scale, (float)l.tex->h * l.scale}, 1, 0, c);
}

}  // namespace text
