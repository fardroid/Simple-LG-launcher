#include "core/tasks.h"

#include <SDL.h>

#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

static std::mutex jobs_lock, posted_lock;
static std::condition_variable jobs_ready;
static std::deque<std::function<void()>> jobs, soon, posted;     // soon: run_soon's jobs, taken first
static std::vector<std::thread> workers;
static bool stopping;
static unsigned wake;

// A worker: run_soon's jobs first; `only_soon`: the one kept for them, so that they never wait for slow jobs to end.
static void work (bool only_soon) {
	for (;;) {
		std::function<void()> job;
		{
			std::unique_lock<std::mutex> l(jobs_lock);
			jobs_ready.wait(l, [only_soon] { return stopping || !soon.empty() || (!only_soon && !jobs.empty()); });
			if (stopping) return;
			auto &q = !soon.empty() ? soon : jobs;
			job = std::move(q.front());
			q.pop_front();
		}
		job();
	}
}

void tasks_start (int count, unsigned wake_event) {
	wake = wake_event;
	for (int i = 0; i < count; i++) workers.emplace_back(work, false);
	workers.emplace_back(work, true);
}

void tasks_stop () {
	{
		std::lock_guard<std::mutex> l(jobs_lock);
		stopping = true;
	}
	jobs_ready.notify_all();
	for (auto &t : workers) t.join();
	workers.clear();
}

void run_background (std::function<void()> job) {
	{
		std::lock_guard<std::mutex> l(jobs_lock);
		jobs.push_back(std::move(job));
	}
	jobs_ready.notify_all();     // (one woken might be the worker kept for run_soon, which leaves this job alone)
}

void run_soon (std::function<void()> job) {
	{
		std::lock_guard<std::mutex> l(jobs_lock);
		soon.push_back(std::move(job));
	}
	jobs_ready.notify_all();
}

void post_main (std::function<void()> fn) {
	{
		std::lock_guard<std::mutex> l(posted_lock);
		posted.push_back(std::move(fn));
	}
	SDL_Event ev;
	SDL_zero(ev);
	ev.type = wake;
	SDL_PushEvent(&ev);
}

bool run_main_posted () {
	std::deque<std::function<void()>> now;
	{
		std::lock_guard<std::mutex> l(posted_lock);
		now.swap(posted);
	}
	for (auto &fn : now) fn();
	return !now.empty();
}
