// The wallpaper collection (Settings → Wallpaper → Picture → Collection): photos of mountains, nature and space from Wikimedia
// Commons under open licences, and the launcher's own abstract pictures, kept on GitHub (a release's files, listed in
// repo/wallpapers.json) rather than in the package.
// A photo is downloaded when it is ticked in the gallery and checked against its SHA-256; one that is not part of the wallpaper
// any more is deleted when the gallery closes, so the TV keeps only the photos in use.
#pragma once

#include <set>
#include <string>
#include <vector>

extern const char *COLLECTION_FEED;    // repo/wallpapers.json on GitHub

struct CollectionPhoto {
	std::string id, group, title, author, licence, source;
	std::string url, sha256;          // the 1920 x 1080 JPEG
	std::string thumb, thumbSha256;   // its 384 x 216 preview
	long size = 0, thumbSize = 0;
};

/** The groups, in the order the gallery shows them. */
const std::vector<std::string> &collection_groups ();
/** The photos of the list; an entry that cannot be checked (no https address, no sum, an id that is not a plain name) is left out.
 *  `local` lets file:// and http://127.0.0.1: addresses through too (a test list, named in the data folder's collection-feed file). */
std::vector<CollectionPhoto> parse_collection (const std::string &json_text, bool local = false);

/** Where the downloaded photos are kept (data folder/collection) and the previews (data folder/collection-thumbs). */
std::string collection_dir (const std::string &data_dir);
std::string collection_thumbs_dir (const std::string &data_dir);
std::string collection_file (const std::string &data_dir, const std::string &id);
std::string collection_thumb (const std::string &data_dir, const std::string &id);

/** The files in the collection folder that are not `keep` (the wallpaper's pictures): the photos no longer in use, and the
 *  leftovers of a download that was cut short. */
std::vector<std::string> unused_collection_files (const std::string &data_dir, const std::set<std::string> &keep);
