// Work off the main thread, and results back onto it.
//  - run_background(job): a few worker threads run jobs in order of arrival (network, decoding, reading files);
//  - post_main(fn): fn runs on the main thread at the start of its next loop (the loop is woken up for it);
//  - run_main_posted(): the main loop calls it once per turn.
// Everything that touches the screen, the GL context or the app's state happens on the main thread.
#pragma once

#include <functional>

void tasks_start (int workers, unsigned wake_event);
void tasks_stop ();
void run_background (std::function<void()> job);
/** A job the screen waits for (the wallpaper): before the others, with a worker of its own besides, so that it does not wait
 *  behind slow ones (at the start the checks of the way of opening take seconds, and the picture showed only after them). */
void run_soon (std::function<void()> job);
void post_main (std::function<void()> fn);
/** Runs what was posted; true when there was anything. */
bool run_main_posted ();
