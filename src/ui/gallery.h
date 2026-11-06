// The wallpaper gallery (screens/Picker.tsx), in tiles like the apps in Settings, each picture with its preview.
// Top level: the default background, the USB drives, the TV storage. Inside a folder: its folders, its pictures, and (when it has
// pictures) a tile that plays the whole folder as a slideshow. OK ticks pictures, in any folders; leaving the gallery shows them
// (one picture, or a slideshow of them). The default background and a folder's slideshow are chosen at once.
#pragma once

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

	/** `chosen`: the pictures chosen now (one picture, or the ticked ones), ticked when it opens. */
	void open (const std::vector<std::string> &chosen, double now);
	void set_theme (const std::string &theme) { theme_ = theme; }
	bool key_down (int scancode, double now);
	bool animating (double now) const;
	void draw (double now);
	/** Lets the previews go (the gallery closed). */
	void release ();

private:
	struct Row { std::string key, kind, label, detail, path; };    // kind: gradient root folder picture slideshow info retry

	std::vector<Row> rows () const;
	void load (int attempt);
	void go_to (const std::string &path, bool top, int restore);
	void go_up ();
	void leave ();
	void activate (int i);
	void next_thumb ();

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
	bool thumb_busy_ = false;
};
