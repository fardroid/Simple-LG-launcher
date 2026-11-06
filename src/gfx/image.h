// Pictures decoded to RGBA in memory (on worker threads), ready to become textures on the main thread.
#pragma once

#include <memory>
#include <string>
#include <vector>

struct Pixels {
	int w = 0, h = 0;
	std::vector<unsigned char> rgba;   // w * h * 4, straight (not premultiplied) alpha
};

/** Decodes a jpeg or png file. `max_w` / `max_h` (0: no limit): a jpeg is decoded at the largest of libjpeg-turbo's scales
 *  (1/2, 1/4, 1/8...) that still covers that size, so a 4K photo for a 1920x1080 screen costs a quarter. */
std::shared_ptr<Pixels> decode_file (const std::string &path, int max_w = 0, int max_h = 0, std::string *why = nullptr);
std::shared_ptr<Pixels> decode_memory (const std::string &data, int max_w = 0, int max_h = 0, std::string *why = nullptr);

/** The picture as a JPEG file's bytes (`quality` 1-100); empty on failure. */
std::string encode_jpeg (const Pixels &p, int quality);

/** Picture size from its header only (0x0 when unknown). */
void image_size (const std::string &data, int &w, int &h);

/** A copy scaled to exactly w x h (box filter when shrinking, bilinear when growing). */
std::shared_ptr<Pixels> resize (const Pixels &src, int w, int h);
/** "cover" into w x h: scaled to fill, the middle kept (as background-size: cover; background-position: center). */
std::shared_ptr<Pixels> cover (const Pixels &src, int w, int h);
/** Gaussian-like blur (three box passes) of the whole picture, alpha included. */
void blur (Pixels &p, float radius);
