#include "gfx/image.h"

#include "sys/fs.h"

#include <png.h>
#include <turbojpeg.h>

#include <algorithm>
#include <cmath>
#include <cstring>

static bool is_png (const std::string &d) { return d.size() > 8 && memcmp(d.data(), "\x89PNG", 4) == 0; }
static bool is_jpeg (const std::string &d) { return d.size() > 3 && (unsigned char)d[0] == 0xFF && (unsigned char)d[1] == 0xD8; }

void image_size (const std::string &d, int &w, int &h) {
	w = h = 0;
	if (is_png(d) && d.size() >= 24) {
		auto be = [&] (size_t i) { return ((unsigned char)d[i] << 24) | ((unsigned char)d[i + 1] << 16) | ((unsigned char)d[i + 2] << 8) | (unsigned char)d[i + 3]; };
		w = (int)be(16);
		h = (int)be(20);
	} else if (is_jpeg(d)) {
		tjhandle tj = tjInitDecompress();
		int sub, cs;
		if (tjDecompressHeader3(tj, (const unsigned char *)d.data(), (unsigned long)d.size(), &w, &h, &sub, &cs) != 0) w = h = 0;
		tjDestroy(tj);
	}
}

static std::shared_ptr<Pixels> decode_jpeg (const std::string &d, int max_w, int max_h, std::string *why) {
	tjhandle tj = tjInitDecompress();
	int w, h, sub, cs;
	if (tjDecompressHeader3(tj, (const unsigned char *)d.data(), (unsigned long)d.size(), &w, &h, &sub, &cs) != 0) {
		if (why) *why = tjGetErrorStr2(tj);
		tjDestroy(tj);
		return nullptr;
	}
	int count = 0;
	tjscalingfactor *factors = tjGetScalingFactors(&count);
	int sw = w, sh = h;
	if (max_w > 0 && max_h > 0) {
		// the smallest scale that still covers max_w x max_h
		for (int i = 0; i < count; i++) {
			int cw = TJSCALED(w, factors[i]), ch = TJSCALED(h, factors[i]);
			if (factors[i].num > factors[i].denom) continue;
			if (cw >= max_w && ch >= max_h && cw * ch < sw * sh) { sw = cw; sh = ch; }
		}
	}
	auto p = std::make_shared<Pixels>();
	p->w = sw;
	p->h = sh;
	p->rgba.resize((size_t)sw * sh * 4);
	if (tjDecompress2(tj, (const unsigned char *)d.data(), (unsigned long)d.size(), p->rgba.data(), sw, 0, sh, TJPF_RGBA, TJFLAG_FASTDCT) != 0) {
		// a warning (a damaged end, say) still leaves a usable picture
		if (tjGetErrorCode(tj) == TJERR_FATAL) {
			if (why) *why = tjGetErrorStr2(tj);
			tjDestroy(tj);
			return nullptr;
		}
	}
	tjDestroy(tj);
	return p;
}

static std::shared_ptr<Pixels> decode_png (const std::string &d, std::string *why) {
	png_image img;
	memset(&img, 0, sizeof img);
	img.version = PNG_IMAGE_VERSION;
	if (!png_image_begin_read_from_memory(&img, d.data(), d.size())) {
		if (why) *why = img.message;
		return nullptr;
	}
	img.format = PNG_FORMAT_RGBA;
	auto p = std::make_shared<Pixels>();
	p->w = (int)img.width;
	p->h = (int)img.height;
	p->rgba.resize(PNG_IMAGE_SIZE(img));
	if (!png_image_finish_read(&img, nullptr, p->rgba.data(), 0, nullptr)) {
		if (why) *why = img.message;
		png_image_free(&img);
		return nullptr;
	}
	return p;
}

std::shared_ptr<Pixels> decode_memory (const std::string &data, int max_w, int max_h, std::string *why) {
	if (is_jpeg(data)) return decode_jpeg(data, max_w, max_h, why);
	if (is_png(data)) return decode_png(data, why);
	if (why) *why = "not a jpeg or png";
	return nullptr;
}

std::shared_ptr<Pixels> decode_file (const std::string &path, int max_w, int max_h, std::string *why) {
	std::string data;
	if (!read_file(path, data)) {
		if (why) *why = "cannot read";
		return nullptr;
	}
	return decode_memory(data, max_w, max_h, why);
}

std::shared_ptr<Pixels> resize (const Pixels &src, int w, int h) {
	auto out = std::make_shared<Pixels>();
	out->w = w;
	out->h = h;
	out->rgba.resize((size_t)w * h * 4);
	if (src.w <= 0 || src.h <= 0 || w <= 0 || h <= 0) return out;
	const float sx = (float)src.w / w, sy = (float)src.h / h;
	if (sx >= 1 && sy >= 1) {
		// shrinking: the average of the source pixels each target pixel covers
		for (int y = 0; y < h; y++) {
			int y0 = (int)(y * sy), y1 = std::max(y0 + 1, (int)((y + 1) * sy));
			for (int x = 0; x < w; x++) {
				int x0 = (int)(x * sx), x1 = std::max(x0 + 1, (int)((x + 1) * sx));
				unsigned sum[4] = {0, 0, 0, 0}, n = 0;
				for (int yy = y0; yy < y1 && yy < src.h; yy++) {
					const unsigned char *row = &src.rgba[((size_t)yy * src.w + x0) * 4];
					for (int xx = x0; xx < x1 && xx < src.w; xx++, row += 4) {
						sum[0] += row[0]; sum[1] += row[1]; sum[2] += row[2]; sum[3] += row[3];
						n++;
					}
				}
				unsigned char *o = &out->rgba[((size_t)y * w + x) * 4];
				for (int k = 0; k < 4; k++) o[k] = (unsigned char)(n ? sum[k] / n : 0);
			}
		}
		return out;
	}
	for (int y = 0; y < h; y++) {
		float fy = std::max(0.f, (y + 0.5f) * sy - 0.5f);
		int y0 = std::min((int)fy, src.h - 1), y1 = std::min(y0 + 1, src.h - 1);
		float ty = fy - y0;
		for (int x = 0; x < w; x++) {
			float fx = std::max(0.f, (x + 0.5f) * sx - 0.5f);
			int x0 = std::min((int)fx, src.w - 1), x1 = std::min(x0 + 1, src.w - 1);
			float tx = fx - x0;
			const unsigned char *a = &src.rgba[((size_t)y0 * src.w + x0) * 4], *b = &src.rgba[((size_t)y0 * src.w + x1) * 4];
			const unsigned char *c = &src.rgba[((size_t)y1 * src.w + x0) * 4], *d = &src.rgba[((size_t)y1 * src.w + x1) * 4];
			unsigned char *o = &out->rgba[((size_t)y * w + x) * 4];
			for (int k = 0; k < 4; k++) {
				float top = a[k] + (b[k] - a[k]) * tx, bottom = c[k] + (d[k] - c[k]) * tx;
				o[k] = (unsigned char)std::lround(top + (bottom - top) * ty);
			}
		}
	}
	return out;
}

std::shared_ptr<Pixels> cover (const Pixels &src, int w, int h) {
	if (src.w <= 0 || src.h <= 0) return resize(src, w, h);
	double scale = std::max((double)w / src.w, (double)h / src.h);
	int cw = std::min(src.w, (int)std::lround(w / scale)), ch = std::min(src.h, (int)std::lround(h / scale));
	int cx = (src.w - cw) / 2, cy = (src.h - ch) / 2;
	Pixels crop;
	crop.w = cw;
	crop.h = ch;
	crop.rgba.resize((size_t)cw * ch * 4);
	for (int y = 0; y < ch; y++) memcpy(&crop.rgba[(size_t)y * cw * 4], &src.rgba[((size_t)(y + cy) * src.w + cx) * 4], (size_t)cw * 4);
	return resize(crop, w, h);
}

// One box pass along rows (step 4) or columns (step w*4), in place through a line buffer. The radius may be fractional: the
// whole pixels within r, and the two just beyond it weighed by the fraction (a frost of 30-100 % moves in even steps, where a
// whole-pixel radius gave a few jumps).
static void box_pass (Pixels &p, float radius, bool horizontal) {
	if (radius <= 0) return;
	const int r = (int)std::floor(radius);
	const float frac = radius - r;
	const int len = horizontal ? p.w : p.h, lines = horizontal ? p.h : p.w;
	const size_t step = horizontal ? 4 : (size_t)p.w * 4, line_step = horizontal ? (size_t)p.w * 4 : 4;
	std::vector<unsigned char> buf((size_t)len * 4);
	const float span = 2 * r + 1 + 2 * frac;
	for (int l = 0; l < lines; l++) {
		unsigned char *base = &p.rgba[l * line_step];
		for (int i = 0; i < len; i++) memcpy(&buf[(size_t)i * 4], base + i * step, 4);
		int sum[4] = {0, 0, 0, 0};
		auto at = [&] (int i, int k) { return (int)buf[(size_t)std::min(len - 1, std::max(0, i)) * 4 + k]; };
		for (int k = 0; k < 4; k++) for (int i = -r; i <= r; i++) sum[k] += at(i, k);
		for (int i = 0; i < len; i++) {
			unsigned char *o = base + i * step;
			for (int k = 0; k < 4; k++) {
				float v = sum[k] + frac * (at(i - r - 1, k) + at(i + r + 1, k));
				o[k] = (unsigned char)std::min(255.f, v / span + 0.5f);
				sum[k] += at(i + r + 1, k) - at(i - r, k);
			}
		}
	}
}

void blur (Pixels &p, float radius) {
	// three box passes approach a gaussian of the same spread (the CSS blur radius is its standard deviation)
	float r = std::max(0.5f, radius * 0.9f);
	for (int i = 0; i < 3; i++) {
		box_pass(p, r, true);
		box_pass(p, r, false);
	}
}

std::string encode_jpeg (const Pixels &p, int quality) {
	if (p.w <= 0 || p.h <= 0) return "";
	tjhandle tj = tjInitCompress();
	if (!tj) return "";
	unsigned char *buf = nullptr;
	unsigned long size = 0;
	std::string out;
	if (tjCompress2(tj, p.rgba.data(), p.w, 0, p.h, TJPF_RGBA, &buf, &size, TJSAMP_420, quality, TJFLAG_FASTDCT) == 0) out.assign((const char *)buf, size);
	if (buf) tjFree(buf);
	tjDestroy(tj);
	return out;
}
