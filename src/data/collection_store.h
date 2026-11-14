// The wallpaper collection's list and its downloads, kept by the launcher (not the gallery), so that what was ticked goes on
// downloading in the background after the gallery closes: tick, leave, and the pictures come one after another.
// What is wanted: the collection's pictures in the wallpaper's settings, and, while the gallery is open, the ones ticked in it.
// The missing ones are downloaded one at a time, in the order they were wanted; one that is no longer wanted stops. Files that
// are not wanted are deleted only by prune() (when the gallery closes), so a photo unticked and ticked again stays.
// Main thread only, except where said.
#pragma once

#include "data/collection.h"

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

class CollectionStore {
public:
	enum class State { none, waiting, downloading, failed };

	void set_data_dir (const std::string &dir) { data_dir_ = dir; }
	const std::string &data_dir () const { return data_dir_; }

	// --- the list (repo/wallpapers.json)
	const std::vector<CollectionPhoto> &photos () const { return photos_; }
	const CollectionPhoto *photo (const std::string &id) const;
	bool listed () const { return !photos_.empty(); }
	bool listing () const { return listing_; }
	/** Reads the list once (again when it could not be read); `done` runs on the main thread when it is back. */
	void read_list (std::function<void()> done = nullptr);

	// --- the downloads
	/** The collection's pictures the wallpaper shows (from the settings). */
	void set_wanted (const std::vector<std::string> &paths);
	/** The ones ticked in the open gallery (empty when it closes). */
	void set_ticked (const std::vector<std::string> &paths);
	bool on_tv (const std::string &path) const { return on_tv_.count(path) > 0; }
	State state (const std::string &id) const;
	/** The id of a photo by its file in the collection folder; empty for any other file. */
	std::string id_of (const std::string &path) const;
	/** 0-1 while it downloads. */
	double fraction (const std::string &id) const;
	/** Pictures wanted and not on the TV yet. */
	int missing () const;
	/** A picture is downloading now. */
	bool busy () const { return !active_.empty(); }
	/** Deletes the collection's files that are not wanted (and not being downloaded). */
	void prune ();
	/** Now and then (the minute tick): a list or downloads that could not be had are tried again 10 minutes later. */
	void tick ();

	/** A picture has come (its path), or a download failed for want of room (`no_room`). */
	std::function<void(const std::string &path)> on_arrived;
	std::function<void()> on_no_room;

private:
	std::vector<std::string> wanted_order () const;
	void pump ();

	std::string data_dir_;
	std::vector<CollectionPhoto> photos_;
	bool listing_ = false;
	double list_failed_at_ = -1;                    // the list could not be read (offline): not asked again until tick()
	std::vector<std::function<void()>> list_waiters_;
	std::vector<std::string> wanted_, ticked_;
	std::set<std::string> on_tv_;
	bool scanned_ = false;
	void scan ();
	void drop_outdated ();
	std::string active_;                            // the id downloading now
	double active_fraction_ = 0;
	std::shared_ptr<std::atomic<bool>> stop_;       // set when the one downloading is no longer wanted
	std::map<std::string, double> failed_at_;       // id: when its download failed (tried again 10 minutes later)
	bool told_no_room_ = false;
};
