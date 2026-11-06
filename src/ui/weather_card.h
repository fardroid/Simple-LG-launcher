// The weather card (components/WeatherCard.tsx): now, the chosen day by hours (a curve every 3 hours with a dashed line at the
// present moment), and the next days. ← → choose which day the curve is for.
#pragma once

#include "data/weather.h"
#include "gfx/gfx.h"
#include "gfx/text.h"
#include "ui/boardlayout.h"

#include <ctime>

/** The card's contents (its plate is drawn before). `focus`: 0-1, the remote is on the card. */
void draw_weather (const BoardCard &card, const WeatherView &view, int card_size, Face face, const std::tm &now, float focus);
/** The day (of `days` shown) under the point x, y, as draw_weather lays them out; -1: none. */
int weather_day_at (const BoardCard &card, int days, int card_size, float x, float y);
