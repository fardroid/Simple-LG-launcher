// Icons of the apps and inputs: decoded off the main thread, made into textures a few per frame, kept while they are used.
#pragma once

#include "gfx/gfx.h"

#include <string>

namespace icons {

/** Where icons read through the Homebrew Channel are kept (they load from there next time). */
void set_cache_dir (const std::string &dir);

/** The texture for a picture file, or null while it is loading (or could not be read). `size`: the largest side it is shown at. */
TexturePtr get (const std::string &path, int size);
/** Uploads what has been decoded meanwhile (at most a few per call); true when anything new can be drawn. */
bool upload_ready ();

}  // namespace icons
