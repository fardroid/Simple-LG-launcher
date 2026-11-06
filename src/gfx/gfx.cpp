#include "gfx/gfx.h"

#include "core/log.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

Color Color::hex (const char *rgb, float a) {
	unsigned v = (unsigned)strtoul(rgb[0] == '#' ? rgb + 1 : rgb, nullptr, 16);
	return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f, a};
}

Texture::~Texture () { if (id) glDeleteTextures(1, &id); }

TexturePtr make_texture (const Pixels &p) {
	auto t = std::make_shared<Texture>();
	t->w = p.w;
	t->h = p.h;
	glGenTextures(1, &t->id);
	glBindTexture(GL_TEXTURE_2D, t->id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, p.w, p.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, p.rgba.data());
	return t;
}

namespace {

int screen_w = 1920, screen_h = 1080;          // the drawable, in real pixels
const float LAYOUT_W = 1920, LAYOUT_H = 1080;

// The quad covers `u_quad` (layout px); `v_p` is the fragment's position relative to the shape's centre, `v_uv` its place in 0..1.
const char *VERTEX = R"(
attribute vec2 a_pos;
uniform vec4 u_quad;
uniform vec2 u_layout;
uniform vec2 u_center;
uniform float u_flip;
uniform vec3 u_grow;      // everything grows by u_grow.z round the point u_grow.xy (a card in focus)
varying vec2 v_p;
varying vec2 v_uv;
void main () {
	vec2 at = u_quad.xy + a_pos * u_quad.zw;
	v_p = at - u_center;
	v_uv = a_pos;
	at = u_grow.xy + (at - u_grow.xy) * u_grow.z;
	// into a target the picture is drawn upside down, so that its first row is the top, as in an uploaded picture
	gl_Position = vec4(at.x / u_layout.x * 2.0 - 1.0, u_flip * (1.0 - at.y / u_layout.y * 2.0), 0.0, 1.0);
}
)";

// The signed distance to a rounded box of half size b, corner radius r (negative inside).
const char *SDF = R"(
precision mediump float;
float box (vec2 p, vec2 b, float r) {
	vec2 q = abs(p) - b + vec2(r);
	return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}
)";

// Shapes. u_mode 0: the box, anti-aliased. 1: its shadow, the edge spread over u_soft (~CSS blur), outside the box only (the box
// itself is u_dy above it: CSS draws an outer shadow outside the border edge). 2: a ring u_soft wide outside it.
const char *SHAPE = R"(
uniform vec4 u_color;
uniform vec2 u_half;
uniform float u_radius;
uniform float u_soft;
uniform float u_dy;
uniform int u_mode;
varying vec2 v_p;
void main () {
	float d = box(v_p, u_half, u_radius);
	float a;
	if (u_mode == 0) a = clamp(0.5 - d, 0.0, 1.0);
	else if (u_mode == 1) a = (1.0 - smoothstep(-u_soft, u_soft, d)) * clamp(0.5 + box(v_p + vec2(0.0, u_dy), u_half, u_radius), 0.0, 1.0);
	else if (u_mode == 3) a = mix(1.0 - clamp(d / u_soft, 0.0, 1.0), 1.0 - smoothstep(0.0, u_soft, d), u_dy);
	else if (u_mode == 4) {
		// the dark spreading along an edge of the screen: strongest in the middle of the right edge of the quad, thinning to the quad's
		// top, bottom and left sides (u_soft: how dense, u_dy: 1 = smooth curves, 0 = straight)
		float v = max(0.0, 1.0 - abs(v_p.y) / u_half.y), h = clamp(0.5 + v_p.x / (2.0 * u_half.x), 0.0, 1.0);
		a = min(1.0, mix(v, v * v * (3.0 - 2.0 * v), u_dy) * mix(h, h * h * (3.0 - 2.0 * h), u_dy) * u_soft);
	}
	else {
		float outer = box(v_p, u_half + vec2(u_soft), u_radius + u_soft);
		a = clamp(0.5 - outer, 0.0, 1.0) * clamp(0.5 + d, 0.0, 1.0);
	}
	a *= u_color.a;
	gl_FragColor = vec4(u_color.rgb * a, a);
}
)";

const char *IMAGE = R"(
uniform sampler2D u_tex;
uniform vec4 u_src;
uniform vec4 u_tint;
uniform vec4 u_tint2;
uniform int u_across;
uniform vec2 u_half;
uniform float u_radius;
uniform float u_opacity;
uniform float u_bright;
varying vec2 v_p;
varying vec2 v_uv;
void main () {
	vec4 t = texture2D(u_tex, u_src.xy + v_uv * u_src.zw);
	vec4 tint = u_across == 1 ? mix(u_tint, u_tint2, v_uv.x) : u_tint;
	float a = t.a * tint.a * u_opacity;
	if (u_radius > 0.0) a *= clamp(0.5 - box(v_p, u_half, u_radius), 0.0, 1.0);
	vec3 rgb = min(t.rgb * tint.rgb * u_bright, vec3(1.0));
	gl_FragColor = vec4(rgb * a, a);
}
)";

const char *GRADIENT = R"(
uniform vec4 u_c0, u_c1, u_c2, u_c3, u_c4;
uniform vec4 u_at0;    // stop positions 0-3
uniform float u_at4;
uniform int u_count;
uniform vec2 u_dir;
uniform float u_len;
uniform vec2 u_half;
uniform float u_radius;
uniform float u_opacity;
varying vec2 v_p;
vec4 mixed (vec4 a, vec4 b, float from, float to, float t) { return mix(a, b, clamp((t - from) / max(to - from, 0.0001), 0.0, 1.0)); }
void main () {
	float t = dot(v_p, u_dir) / u_len + 0.5;
	vec4 c = u_c0;
	if (u_count > 1 && t > u_at0.x) c = mixed(u_c0, u_c1, u_at0.x, u_at0.y, t);
	if (u_count > 2 && t > u_at0.y) c = mixed(u_c1, u_c2, u_at0.y, u_at0.z, t);
	if (u_count > 3 && t > u_at0.z) c = mixed(u_c2, u_c3, u_at0.z, u_at0.w, t);
	if (u_count > 4 && t > u_at0.w) c = mixed(u_c3, u_c4, u_at0.w, u_at4, t);
	float a = c.a * u_opacity;
	if (u_radius > 0.0) a *= clamp(0.5 - box(v_p, u_half, u_radius), 0.0, 1.0);
	gl_FragColor = vec4(c.rgb * a, a);
}
)";

struct Program {
	GLuint id = 0;
	GLint flip, grow, quad, layout, center, color, half, radius, soft, dy, mode, tex, src, tint, tint2, across, opacity, bright;
	GLint c[5], at0, at4, count, dir, len;
	GLint loc (const char *n) { return glGetUniformLocation(id, n); }
};

Program shape, img, grad;
GLuint quad_buffer;
std::vector<Rect> clips;
float group_alpha = 1;
float outer_alpha = 1;      // a whole screen faded (set_outer_alpha), times each group's own
bool into_target = false;
float grow_x = 0, grow_y = 0, grow_k = 1;

GLuint compile (GLenum type, const std::string &src) {
	GLuint s = glCreateShader(type);
	const char *text = src.c_str();
	glShaderSource(s, 1, &text, nullptr);
	glCompileShader(s);
	GLint ok = 0;
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		char info[1024];
		glGetShaderInfoLog(s, sizeof info, nullptr, info);
		logf("shader: %s", info);
	}
	return s;
}

bool build (Program &p, const char *fragment) {
	p.id = glCreateProgram();
	glAttachShader(p.id, compile(GL_VERTEX_SHADER, VERTEX));
	glAttachShader(p.id, compile(GL_FRAGMENT_SHADER, std::string(SDF) + fragment));
	glBindAttribLocation(p.id, 0, "a_pos");
	glLinkProgram(p.id);
	GLint ok = 0;
	glGetProgramiv(p.id, GL_LINK_STATUS, &ok);
	if (!ok) {
		char info[1024];
		glGetProgramInfoLog(p.id, sizeof info, nullptr, info);
		logf("program: %s", info);
		return false;
	}
	p.flip = p.loc("u_flip"); p.grow = p.loc("u_grow"); p.quad = p.loc("u_quad"); p.layout = p.loc("u_layout"); p.center = p.loc("u_center");
	p.color = p.loc("u_color"); p.half = p.loc("u_half"); p.radius = p.loc("u_radius"); p.soft = p.loc("u_soft"); p.dy = p.loc("u_dy"); p.mode = p.loc("u_mode");
	p.tex = p.loc("u_tex"); p.src = p.loc("u_src"); p.tint = p.loc("u_tint"); p.tint2 = p.loc("u_tint2"); p.across = p.loc("u_across"); p.opacity = p.loc("u_opacity"); p.bright = p.loc("u_bright");
	const char *cs[5] = {"u_c0", "u_c1", "u_c2", "u_c3", "u_c4"};
	for (int i = 0; i < 5; i++) p.c[i] = p.loc(cs[i]);
	p.at0 = p.loc("u_at0"); p.at4 = p.loc("u_at4"); p.count = p.loc("u_count"); p.dir = p.loc("u_dir"); p.len = p.loc("u_len");
	return true;
}

// The program in use with the quad and the shape's centre and half size set.
void use (Program &p, Rect quad, Rect shape_rect, float radius) {
	glUseProgram(p.id);
	glUniform1f(p.flip, into_target ? -1.f : 1.f);
	glUniform3f(p.grow, grow_x, grow_y, grow_k);
	glUniform4f(p.quad, quad.x, quad.y, quad.w, quad.h);
	glUniform2f(p.layout, LAYOUT_W, LAYOUT_H);
	glUniform2f(p.center, shape_rect.x + shape_rect.w / 2, shape_rect.y + shape_rect.h / 2);
	if (p.half >= 0) glUniform2f(p.half, shape_rect.w / 2, shape_rect.h / 2);
	if (p.radius >= 0) glUniform1f(p.radius, std::min(radius, std::min(shape_rect.w, shape_rect.h) / 2));
}

void draw () { glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); }

// The quad `q` drawn as the frame round `hole` (nothing is drawn inside the hole): for shapes that are empty in their middle.
void draw_frame (Program &p, Rect q, Rect hole) {
	if (hole.w <= 0 || hole.h <= 0) { glUniform4f(p.quad, q.x, q.y, q.w, q.h); draw(); return; }
	Rect parts[4] = {
		{q.x, q.y, q.w, hole.y - q.y},                                   // above
		{q.x, hole.y + hole.h, q.w, q.y + q.h - hole.y - hole.h},        // below
		{q.x, hole.y, hole.x - q.x, hole.h},                             // left
		{hole.x + hole.w, hole.y, q.x + q.w - hole.x - hole.w, hole.h}   // right
	};
	for (auto &r : parts) {
		if (r.w <= 0 || r.h <= 0) continue;
		glUniform4f(p.quad, r.x, r.y, r.w, r.h);
		draw();
	}
}

void apply_clip () {
	if (clips.empty()) { glDisable(GL_SCISSOR_TEST); return; }
	Rect c = clips.back();
	// the clip grows with what it holds
	c = {grow_x + (c.x - grow_x) * grow_k, grow_y + (c.y - grow_y) * grow_k, c.w * grow_k, c.h * grow_k};
	float sx = screen_w / LAYOUT_W, sy = screen_h / LAYOUT_H;
	glEnable(GL_SCISSOR_TEST);
	int x = (int)std::floor(c.x * sx), y = (int)std::floor(c.y * sy);
	int w = (int)std::ceil((c.x + c.w) * sx) - x, h = (int)std::ceil((c.y + c.h) * sy) - y;
	glScissor(x, into_target ? y : screen_h - y - h, std::max(0, w), std::max(0, h));
}

}  // namespace

namespace gfx {

bool init (int width, int height) {
	screen_w = width;
	screen_h = height;
	if (!build(shape, SHAPE) || !build(img, IMAGE) || !build(grad, GRADIENT)) return false;
	const float corners[] = {0, 0, 1, 0, 0, 1, 1, 1};
	glGenBuffers(1, &quad_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, quad_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof corners, corners, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);
	logf("gl: %s, %s", (const char *)glGetString(GL_RENDERER), (const char *)glGetString(GL_VERSION));
	return true;
}

std::unique_ptr<Target> make_target () {
	auto t = std::make_unique<Target>();
	t->tex = std::make_shared<Texture>();
	t->tex->w = screen_w;
	t->tex->h = screen_h;
	glGenTextures(1, &t->tex->id);
	glBindTexture(GL_TEXTURE_2D, t->tex->id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, screen_w, screen_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glGenFramebuffers(1, &t->fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, t->fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t->tex->id, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { logf("gl: the target is not complete"); t.reset(); }
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return t;
}

void begin_frame (Color c, Target *t) {
	glBindFramebuffer(GL_FRAMEBUFFER, t ? t->fbo : 0);
	into_target = t != nullptr;
	glViewport(0, 0, screen_w, screen_h);
	clips.clear();
	group_alpha = 1;
	outer_alpha = 1;
	grow_k = 1;
	apply_clip();
	glClearColor(c.r, c.g, c.b, 1);
	glClear(GL_COLOR_BUFFER_BIT);
}

void set_alpha (float a) { group_alpha = a; }
void set_outer_alpha (float a) { outer_alpha = a; }
float outer_alpha_now () { return outer_alpha; }

void set_grow (float cx, float cy, float k) {
	grow_x = cx;
	grow_y = cy;
	grow_k = k;
	apply_clip();
}

void rect (Rect r, float radius, Color c) {
	c.a *= group_alpha * outer_alpha;
	if (c.a <= 0 || r.w <= 0 || r.h <= 0) return;
	Rect q{r.x - 1, r.y - 1, r.w + 2, r.h + 2};
	use(shape, q, r, radius);
	glUniform4f(shape.color, c.r, c.g, c.b, c.a);
	glUniform1i(shape.mode, 0);
	draw();
}

void shadow (Rect r, float radius, float dy, float blur, Color c) {
	c.a *= group_alpha * outer_alpha;
	if (c.a <= 0 || blur <= 0) return;
	Rect s{r.x, r.y + dy, r.w, r.h};
	// a smoothstep over ±soft gives about the spread of CSS's blur (a gaussian with sigma blur/2)
	float soft = blur * 0.6f;
	Rect q{s.x - soft - 2, s.y - soft - 2, s.w + 2 * soft + 4, s.h + 2 * soft + 4};
	use(shape, q, s, radius);
	glUniform4f(shape.color, c.r, c.g, c.b, c.a);
	glUniform1f(shape.soft, soft);
	glUniform1f(shape.dy, dy);
	glUniform1i(shape.mode, 1);
	// nothing under the box itself: the middle of it is left out of the quad
	float rr = std::min(radius, std::min(r.w, r.h) / 2);
	draw_frame(shape, q, {r.x + rr, r.y + rr, r.w - 2 * rr, r.h - 2 * rr});
}

void halo (Rect r, float radius, float fade, Color c, bool smooth) {
	c.a *= group_alpha * outer_alpha;
	if (c.a <= 0 || fade <= 0) return;
	Rect q{r.x - fade - 2, r.y - fade - 2, r.w + 2 * fade + 4, r.h + 2 * fade + 4};
	use(shape, q, r, radius);
	glUniform4f(shape.color, c.r, c.g, c.b, c.a);
	glUniform1f(shape.soft, fade);
	glUniform1f(shape.dy, smooth ? 1.f : 0.f);
	glUniform1i(shape.mode, 3);
	draw();
}

void edge_glow (Rect r, float density, Color c, bool smooth) {
	c.a *= group_alpha * outer_alpha;
	if (c.a <= 0 || r.w <= 0 || r.h <= 0) return;
	use(shape, r, r, 0);
	glUniform4f(shape.color, c.r, c.g, c.b, c.a);
	glUniform1f(shape.soft, density);
	glUniform1f(shape.dy, smooth ? 1.f : 0.f);
	glUniform1i(shape.mode, 4);
	draw();
}

void ring (Rect r, float radius, float spread, Color c) {
	c.a *= group_alpha * outer_alpha;
	if (c.a <= 0 || spread <= 0) return;
	Rect q{r.x - spread - 1, r.y - spread - 1, r.w + 2 * spread + 2, r.h + 2 * spread + 2};
	use(shape, q, r, radius);
	glUniform4f(shape.color, c.r, c.g, c.b, c.a);
	glUniform1f(shape.soft, spread);
	glUniform1i(shape.mode, 2);
	float rr = std::min(radius, std::min(r.w, r.h) / 2) + 1;
	draw_frame(shape, q, {r.x + rr, r.y + rr, r.w - 2 * rr, r.h - 2 * rr});
}

void image (const Texture &t, Rect r, float opacity, float radius, Color tint, float brightness, Rect src, const Color *across) {
	opacity *= group_alpha * outer_alpha;
	if (!t.id || opacity <= 0 || r.w <= 0 || r.h <= 0) return;
	use(img, r, r, radius);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, t.id);
	glUniform1i(img.tex, 0);
	if (src.w > 0 && t.w > 0) glUniform4f(img.src, src.x / t.w, src.y / t.h, src.w / t.w, src.h / t.h);
	else glUniform4f(img.src, 0, 0, 1, 1);
	glUniform4f(img.tint, tint.r, tint.g, tint.b, tint.a);
	glUniform1i(img.across, across ? 1 : 0);
	if (across) glUniform4f(img.tint2, across->r, across->g, across->b, across->a);
	glUniform1f(img.opacity, opacity);
	glUniform1f(img.bright, brightness);
	glUniform1f(img.radius, radius > 0 ? std::min(radius, std::min(r.w, r.h) / 2) : 0.f);
	draw();
}

void gradient (Rect r, float angle, const GradientStop *stops, int count, float opacity, float radius) {
	opacity *= group_alpha * outer_alpha;
	if (count < 1 || opacity <= 0) return;
	count = std::min(count, 5);
	use(grad, r, r, radius);
	// CSS: 0deg points up, 90deg right; the gradient line's length is |w sin a| + |h cos a|
	float a = angle * (float)M_PI / 180.f;
	float dx = std::sin(a), dy = -std::cos(a);
	glUniform2f(grad.dir, dx, dy);
	glUniform1f(grad.len, std::fabs(r.w * std::sin(a)) + std::fabs(r.h * std::cos(a)));
	for (int i = 0; i < 5; i++) {
		const Color &c = stops[std::min(i, count - 1)].color;
		glUniform4f(grad.c[i], c.r, c.g, c.b, c.a);
	}
	auto at = [&] (int i) { return stops[std::min(i, count - 1)].at; };
	glUniform4f(grad.at0, at(0), at(1), at(2), at(3));
	glUniform1f(grad.at4, at(4));
	glUniform1i(grad.count, count);
	glUniform1f(grad.opacity, opacity);
	glUniform1f(grad.radius, radius > 0 ? std::min(radius, std::min(r.w, r.h) / 2) : 0.f);
	draw();
}

void push_clip (Rect r) {
	if (!clips.empty()) {
		const Rect &o = clips.back();
		float x0 = std::max(o.x, r.x), y0 = std::max(o.y, r.y), x1 = std::min(o.x + o.w, r.x + r.w), y1 = std::min(o.y + o.h, r.y + r.h);
		r = {x0, y0, std::max(0.f, x1 - x0), std::max(0.f, y1 - y0)};
	}
	clips.push_back(r);
	apply_clip();
}

void pop_clip () {
	if (!clips.empty()) clips.pop_back();
	apply_clip();
}

}  // namespace gfx
