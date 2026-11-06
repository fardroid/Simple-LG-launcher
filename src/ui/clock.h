// The clock card (components/Clock.tsx): the time (with AM/PM beside it in 12-hour mode) and, to its right, the day and the
// date; in a narrow column the date goes under the time on one line.
#pragma once

#include "data/config.h"
#include "gfx/text.h"
#include "ui/boardlayout.h"

#include <ctime>
#include <string>

/** The face of a clock font id; the cards use the clock's font too (--widget-font). */
Face clock_face (const std::string &font);

/** Draws the clock into its card. */
void draw_clock (const BoardCard &card, const std::tm &now, const ClockConfig &cfg, const std::string &lang, bool h24, bool narrow);
