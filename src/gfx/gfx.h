// Drawing with OpenGL ES 2. Everything is in the layout's pixels: 1920 x 1080, origin top left (the web version's CSS pixels).
// Colours go in straight (not premultiplied); blending is premultiplied inside.
#pragma once

#include "gfx/image.h"

#include <SDL_opengles2.h>

#include <memory>

struct Color {
	float r = 0, g = 0, b = 0, a = 1;
	Color () = default;
	Color (float r_, float g_, float b_, float a_ = 1) : r(r_), g(g_), b(b_), a(a_) {}
	/** "#RRGGBB" with an optional alpha */
	static Color hex (const char *rgb, float a = 1);
	Color with_alpha (float a_) const { return {r, g, b, a_}; }
};

struct Texture {
	GLuint id = 0;
	int w = 0, h = 0;
	Texture () = default;
	Texture (const Texture &) = delete;
	Texture &operator= (const Texture &) = delete;
	~Texture ();
};
using TexturePtr = std::shared_ptr<Texture>;

/** A picture drawn into instead of the screen (the parts of the screen that rarely change are kept in one). */
struct Target {
	GLuint fbo = 0;
	TexturePtr tex;
};
/** Uploads decoded pixels (main thread). */
TexturePtr make_texture (const Pixels &p);

struct Rect { float x = 0, y = 0, w = 0, h = 0; };

struct GradientStop { Color color; float at; };

namespace gfx {

bool init (int width, int height);
/** A target the size of the screen (null when the GPU cannot make one). */
std::unique_ptr<Target> make_target ();
/** Starts drawing into `t` (null: the screen), cleared to `clear`. */
void begin_frame (Color clear, Target *t = nullptr);

/** A rounded rectangle, its edge anti-aliased. */
void rect (Rect r, float radius, Color c);
/** A soft shadow under a rounded rectangle: the CSS box-shadow `0 dy blur color` (blur is CSS's blur radius). */
void shadow (Rect r, float radius, float dy, float blur, Color c);
/** A ring around a rounded rectangle: `spread` px wide, outside it (CSS box-shadow 0 0 0 spread). */
/** A solid area `r` that fades out over `fade` pixels outside its edge (the same all the way round), over the whole of it. */
void halo (Rect r, float radius, float fade, Color c, bool smooth = true);
/** Dark spreading along an edge of the screen: `r` is the area (its right side is the screen's edge); it is `density` (1 = as is)
 *  at the middle of that side and thins towards the top, the bottom and the left of `r`. */
void edge_glow (Rect r, float density, Color c, bool smooth = true);
void ring (Rect r, float radius, float spread, Color c);
/** A texture over `r`, faded by `opacity`, multiplied by `tint` (white = as it is; its alpha fades too), corners rounded by
 *  `radius`, `brightness` > 1 lightens (CSS filter: brightness). `src`: the part of the texture, in its pixels (w 0: all).
 *  `across`: the tint goes from `tint` on the left to this colour on the right (text filled with a gradient). */
void image (const Texture &t, Rect r, float opacity = 1, float radius = 0, Color tint = {1, 1, 1, 1}, float brightness = 1, Rect src = {},
	const Color *across = nullptr);
/** A CSS linear-gradient(angle, stops) over `r` (angle in degrees, 180 = top to bottom), clipped to a rounded rectangle. */
void gradient (Rect r, float angle, const GradientStop *stops, int count, float opacity = 1, float radius = 0);

/** Everything drawn from now on is faded by this much too (1: as it is), as a CSS opacity on a whole group. */
void set_alpha (float a);
/** A whole screen faded by this much, over the groups' own set_alpha (1: as it is). */
void set_outer_alpha (float a);
/** The value set by set_outer_alpha. */
float outer_alpha_now ();
/** Everything drawn from now on grows by `k` round the point cx, cy (CSS transform: scale); k 1 puts it back. */
void set_grow (float cx, float cy, float k);

/** Drawing inside this rectangle only (screen pixels of the layout); pop restores the one before. */
void push_clip (Rect r);
void pop_clip ();

}  // namespace gfx
