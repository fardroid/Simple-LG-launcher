// A short list of cities to pick from without typing (lib/cities.ts); the picker can also search any place by name.
#pragma once

#include "data/config.h"

#include <vector>

/** The list, named in the current language. */
std::vector<City> preset_cities ();
