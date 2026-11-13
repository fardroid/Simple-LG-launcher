// A short list of cities to pick from without typing (lib/cities.ts); the picker can also search any place by name.
#pragma once

#include "data/config.h"

#include <string>
#include <vector>

/** The list, named in the current language. First the cities of `region` (the TV's country, "IT"), then those of the countries of
 *  the launcher's language (`lang`), then the others by the name of their country. */
std::vector<City> preset_cities (const std::string &region, const std::string &lang);
/** "RU" of "ru-RU"; "" when the tag names no region. */
std::string region_of (const std::string &tag);
/** The countries a language of the launcher belongs to ("de": DE, AT). */
std::vector<std::string> language_countries (const std::string &lang);
