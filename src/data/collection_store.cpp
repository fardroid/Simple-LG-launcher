#include "data/collection_store.h"

#include "core/log.h"
#include "core/sha256.h"
#include "core/tasks.h"
#include "data/storage.h"
#include "sys/fs.h"
#include "sys/http.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <unistd.h>

namespace {

const double RETRY_MS = 10 * 60 * 1000.0;

double now_ms_ () {
	using namespace std::chrono;
	return (double)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

/** GitHub's list, or a test one the data folder names (collection-feed: its address, https or file://). Worker thread. */
std::vector<CollectionPhoto> fetch_list (const std::string &data_dir) {
	std::string feed = COLLECTION_FEED, test, body;
	if (read_file(join_path(data_dir, "collection-feed"), test)) {
		while (!test.empty() && isspace((unsigned char)test.back())) test.pop_back();
		if (!test.empty()) { feed = test; logf("collection: the test list %s", feed.c_str()); }
	}
	bool ok;
	if (feed.rfind("file://", 0) == 0) ok = read_file(feed.substr(7), body, 1 << 20);
	else {
		HttpResult r = http_get(feed, 20, 1 << 20);
		ok = r.ok;
		body = r.body;
		if (!ok) logf("collection: the list: %s", r.error.c_str());
	}
	return ok ? parse_collection(body, feed != COLLECTION_FEED) : std::vector<CollectionPhoto>();
}

}  // namespace

const CollectionPhoto *CollectionStore::photo (const std::string &id) const {
	for (auto &p : photos_) if (p.id == id) return &p;
	return nullptr;
}

void CollectionStore::read_list (std::function<void()> done) {
	if (done) list_waiters_.push_back(done);
	if (!photos_.empty()) {
		auto waiters = std::move(list_waiters_);
		list_waiters_.clear();
		for (auto &w : waiters) w();
		return;
	}
	if (listing_) return;
	listing_ = true;
	std::string dir = data_dir_;
	run_background([this, dir] {
		std::vector<CollectionPhoto> photos = fetch_list(dir);
		post_main([this, photos] {
			listing_ = false;
			photos_ = photos;
			list_failed_at_ = photos.empty() ? now_ms_() : -1;
			logf("collection: %d pictures listed", (int)photos.size());
			drop_outdated();
			auto waiters = std::move(list_waiters_);
			list_waiters_.clear();
			for (auto &w : waiters) w();
			pump();
		});
	});
}

std::string CollectionStore::id_of (const std::string &path) const {
	std::string dir = collection_dir(data_dir_) + "/";
	if (path.rfind(dir, 0) != 0 || path.size() <= dir.size() + 4 || path.compare(path.size() - 4, 4, ".jpg") != 0) return "";
	return path.substr(dir.size(), path.size() - dir.size() - 4);
}

std::vector<std::string> CollectionStore::wanted_order () const {
	// the gallery's ticks first (what was just asked for), then the wallpaper's
	std::vector<std::string> out;
	for (auto *list : {&ticked_, &wanted_})
		for (auto &p : *list) {
			std::string id = id_of(p);
			if (!id.empty() && std::find(out.begin(), out.end(), id) == out.end()) out.push_back(id);
		}
	return out;
}

void CollectionStore::set_wanted (const std::vector<std::string> &paths) { wanted_ = paths; pump(); }
void CollectionStore::set_ticked (const std::vector<std::string> &paths) { ticked_ = paths; pump(); }

CollectionStore::State CollectionStore::state (const std::string &id) const {
	if (id == active_) return State::downloading;
	std::string path = collection_file(data_dir_, id);
	if (on_tv_.count(path)) return State::none;
	auto order = wanted_order();
	if (std::find(order.begin(), order.end(), id) == order.end()) return State::none;
	return failed_at_.count(id) ? State::failed : State::waiting;
}

double CollectionStore::fraction (const std::string &id) const { return id == active_ ? active_fraction_ : 0; }

int CollectionStore::missing () const {
	int n = 0;
	for (auto &id : wanted_order()) n += !on_tv_.count(collection_file(data_dir_, id));
	return n;
}

void CollectionStore::scan () {
	if (scanned_) return;
	// the photos on the TV now (looked up once; each download adds its own)
	scanned_ = true;
	for (auto &e : list_dir(collection_dir(data_dir_)))
		if (!e.dir) on_tv_.insert(join_path(collection_dir(data_dir_), e.name));
}

// A photo the list now has in another version (made again, e.g. sharper) is downloaded again: the file on the TV is not the size
// the list gives. It stays on the TV, and in the wallpaper, until the new one has come.
void CollectionStore::drop_outdated () {
	scan();
	for (auto &p : photos_) {
		std::string path = collection_file(data_dir_, p.id);
		if (p.size <= 0 || !on_tv_.count(path) || file_size(path) == p.size) continue;
		on_tv_.erase(path);
		logf("collection: %s is new in the list: downloading it again", p.id.c_str());
	}
}

void CollectionStore::pump () {
	if (data_dir_.empty()) return;
	scan();
	auto order = wanted_order();
	// the one downloading is no longer wanted: it stops (and the next one starts when it has)
	if (!active_.empty()) {
		if (std::find(order.begin(), order.end(), active_) == order.end() && stop_) *stop_ = true;
		return;
	}
	std::string next;
	for (auto &id : order)
		if (!on_tv_.count(collection_file(data_dir_, id)) && !failed_at_.count(id)) { next = id; break; }
	if (next.empty()) {
		// all on the TV: the list is still read once, for the photos that changed in it since
		if (photos_.empty() && !order.empty() && list_failed_at_ < 0) read_list();
		return;
	}
	if (photos_.empty()) { if (list_failed_at_ < 0) read_list(); return; }     // the list comes first (pump runs again when it has)
	const CollectionPhoto *p = photo(next);
	if (!p) { failed_at_[next] = now_ms_(); pump(); return; }   // not in the list (any more)
	CollectionPhoto ph = *p;
	active_ = next;
	active_fraction_ = 0;
	auto stop = stop_ = std::make_shared<std::atomic<bool>>(false);
	std::string file = collection_file(data_dir_, ph.id), dir = collection_dir(data_dir_);
	logf("collection: downloading %s", ph.id.c_str());
	run_background([this, ph, file, dir, stop] {
		make_dirs(dir);
		std::string why;
		bool no_room = false;
		long long room = free_bytes(dir);
		if (room >= 0 && room < ph.size + (20l << 20)) { why = "not enough space"; no_room = true; }
		else {
			HttpResult r = http_download(ph.url, file, [this, ph, stop] (long done, long total) {
				if (total <= 0) total = ph.size;
				double f = total > 0 ? std::min(1.0, (double)done / total) : 0;
				post_main([this, ph, f] { if (active_ == ph.id) active_fraction_ = f; });
				return !*stop;
			}, 120, 16l << 20);
			if (!r.ok) why = r.error;
			// (a download that did not finish leaves the file on the TV as it was: an older version of the photo stays)
			else if (sha256_file(file) != ph.sha256) { why = "the file came damaged"; unlink(file.c_str()); }
		}
		post_main([this, ph, file, why, no_room, stop] {
			active_.clear();
			stop_.reset();
			if (*stop) logf("collection: %s no longer wanted", ph.id.c_str());
			else if (!why.empty()) {
				logf("collection: %s: %s", ph.id.c_str(), why.c_str());
				failed_at_[ph.id] = now_ms_();
				if (no_room && !told_no_room_ && on_no_room) { told_no_room_ = true; on_no_room(); }
			} else {
				logf("collection: %s downloaded", ph.id.c_str());
				on_tv_.insert(file);
				if (on_arrived) on_arrived(file);
			}
			pump();
		});
	});
}

void CollectionStore::prune () {
	if (data_dir_.empty()) return;
	std::set<std::string> keep;
	for (auto &id : wanted_order()) keep.insert(collection_file(data_dir_, id));
	if (!active_.empty()) keep.insert(collection_file(data_dir_, active_) + ".part");
	std::vector<std::string> unused = unused_collection_files(data_dir_, keep);
	for (auto &path : unused) on_tv_.erase(path);
	if (unused.empty()) return;
	run_background([unused] {
		for (auto &path : unused)
			if (unlink(path.c_str()) == 0) logf("collection: %s deleted, not in use", base_name(path).c_str());
	});
}

void CollectionStore::tick () {
	double now = now_ms_();
	bool again = false;
	if (list_failed_at_ >= 0 && now - list_failed_at_ >= RETRY_MS) { list_failed_at_ = -1; again = true; }
	for (auto it = failed_at_.begin(); it != failed_at_.end();)
		if (now - it->second >= RETRY_MS) { it = failed_at_.erase(it); again = true; }
		else ++it;
	if (again) { told_no_room_ = false; pump(); }
}
