// The wallpaper gallery (screens/Picker.tsx), in tiles like the apps in Settings, each picture with its preview.
// Top level: the default background, the USB drives, the TV storage. Inside a folder: its folders, its pictures, and (when it has
// pictures) a tile that plays the whole folder as a slideshow. OK ticks pictures, in any folders; leaving the gallery shows them
// (one picture, or a slideshow of them). The default background and a folder's slideshow are chosen at once.
#pragma once

#include "data/collection_store.h"
#include "data/gallery.h"
#include "gfx/gfx.h"
#include "ui/anim.h"

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

struct WallpaperChoice {
	std::string mode;                // gradient image folder pictures
	std::string path;
	std::vector<std::string> paths;
};

class Gallery {
public:
	std::function<void(const WallpaperChoice &)> on_choose;
	std::function<void()> on_close;
	/** Leaves the gallery: the ticks, when they changed, are the new wallpaper (on_choose), else on_close. Back from its top does it,
	 *  and so does Home or the launcher going out of sight. */
	void leave ();

	/** `chosen`: the pictures chosen now (one picture, or the ticked ones), ticked when it opens. */
	void open (const std::vector<std::string> &chosen, double now);
	void set_theme (const std::string &theme) { theme_ = theme; }
	/** The wallpaper collection's list and downloads (the launcher's, so that they go on after the gallery closes). */
	void set_store (CollectionStore *store) { store_ = store; data_dir_ = store->data_dir(); }
	bool key_down (int scancode, double now);
	bool animating (double now) const;
	void draw (double now);
	/** Lets the previews go (the gallery closed). */
	void release ();

private:
	struct Row { std::string key, kind, label, detail, path; };    // kind: gradient collection root folder picture cphoto callgroup slideshow info retry

	std::vector<Row> rows () const;
	void load (int attempt);
	void go_to (const std::string &path, bool top, int restore);
	void go_up ();
	void activate (int i);
	void next_thumb ();
	// the collection: path_ is "collection:" for its groups and "collection:<group>" for a group's photos
	bool in_collection () const { return path_.rfind("collection:", 0) == 0; }
	const CollectionPhoto *photo (const std::string &id) const;
	void sync_ticks ();
	void make_stack (const std::string &path);

	std::string theme_ = "dusk";
	std::vector<std::string> chosen_, ticked_;
	std::vector<StorageRoot> roots_;
	bool top_ = true;                // the list of places, not a folder
	std::string path_;
	std::vector<DirEntry> entries_;
	bool loading_ = false, failed_ = false;
	int generation_ = 0, index_ = 0;
	std::map<std::string, int> remembered_;
	double focus_at_ = -1e9;
	Tween scroll_;
	std::map<std::string, Tween> focus_;
	// the previews: made one at a time, the nearest to the highlight first
	std::map<std::string, TexturePtr> thumbs_;
	std::set<std::string> no_thumb_;
	std::map<std::string, std::vector<TexturePtr>> stacks_;    // a folder's first pictures (empty: none), by its path
	bool thumb_busy_ = false;
	// the collection (its list and downloads are the store's)
	CollectionStore *store_ = nullptr;
	std::string data_dir_;
};
