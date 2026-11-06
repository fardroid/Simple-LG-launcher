#include "ui/wallpaper.h"

#include "core/log.h"
#include "core/tasks.h"
#include "gfx/image.h"
#include "sys/fs.h"
#include "ui/anim.h"

#include <algorithm>
#include <cstring>
#include <random>

namespace {
const double FADE_MS = 1400;                   // WALLPAPER.fadeMs
const double RETURN_GRACE_MS = 12000;          // WALLPAPER.returnGraceMs
const size_t MAX_FILES = 400;
const long MAX_BYTES = 8L * 1024 * 1024;
const long MAX_PIXELS = 25000000;

std::mt19937 &rng () { static std::mt19937 r{std::random_device{}()}; return r; }

// frostOf (lib/wallpaper.ts): the picture at 960 x 540 (its margins padded first, so the blur does not fade out at the screen's
// edge), blurred by up to 14 px at full strength (28 on the screen, as the web version's 7 px at 480 x 270), with the shade of
// .wallpaper__scrim over it. `small` is the picture at that size. At 480 x 270 (as the web version) stretching the glass over the
// screen blurred it about as much as the low steps of Frost did, so 30 % and 40 % looked the same (seen on the TV); at 960 x 540
// the stretching blurs half as much and each step shows.
std::shared_ptr<Pixels> frost_of (const Pixels &small, int strength) {
	float k = std::max(0.3f, std::min(1.f, strength / 100.f));
	const int W = 960, H = 540, PAD = 80;
	auto padded = resize(small, W + 2 * PAD, H + 2 * PAD);
	auto inner = resize(small, W, H);
	for (int y = 0; y < H; y++) memcpy(&padded->rgba[((size_t)(y + PAD) * padded->w + PAD) * 4], &inner->rgba[(size_t)y * W * 4], (size_t)W * 4);
	// the blur grows as the square of the setting: the eye tells small blurs apart and not large ones, so on a straight line the
	// low steps all looked alike
	blur(*padded, 14 * k * k);
	auto out = std::make_shared<Pixels>();
	out->w = W;
	out->h = H;
	out->rgba.resize((size_t)W * H * 4);
	for (int y = 0; y < H; y++) {
		// the shade: 0.22 at the top, 0.08 at 35 %, none from 55 %, 0.5 at the bottom
		float t = (y + 0.5f) / H;
		float shade = t < 0.35f ? 0.22f + (0.08f - 0.22f) * t / 0.35f : t < 0.55f ? 0.08f * (1 - (t - 0.35f) / 0.2f) : 0.5f * (t - 0.55f) / 0.45f;
		const unsigned char *src = &padded->rgba[((size_t)(y + PAD) * padded->w + PAD) * 4];
		unsigned char *dst = &out->rgba[(size_t)y * W * 4];
		for (int x = 0; x < W; x++) {
			for (int c = 0; c < 3; c++) dst[x * 4 + c] = (unsigned char)(src[x * 4 + c] * (1 - shade));
			dst[x * 4 + 3] = 255;
		}
	}
	return out;
}
}  // namespace

std::string Wallpaper::source_key () const {
	std::string k = cfg_.mode + "|" + cfg_.path + "|" + std::to_string(cfg_.intervalSec) + "|" + (cfg_.shuffle ? "1" : "0");
	for (auto &p : cfg_.paths) k += "|" + p;
	return k;
}

// The glass made ahead belongs to the picture on screen: another picture starts afresh.
void Wallpaper::own_cache () {
	if (cache_of_ == cur_.small.get()) return;
	glass_cache_.clear();
	glass_making_.clear();
	cache_of_ = cur_.small.get();
}

void Wallpaper::make_ahead (int frost) {
	if (!cur_.small || frost < ranges::frost.min || frost > ranges::frost.max || glass_cache_.count(frost) || glass_making_.count(frost)) return;
	glass_making_.insert(frost);
	auto small = cur_.small;
	run_background([this, small, frost] {
		auto f = frost_of(*small, frost);
		post_main([this, small, frost, f] {
			if (cache_of_ != small.get()) return;
			glass_making_.erase(frost);
			glass_cache_[frost] = make_texture(*f);
		});
	});
}

// The glass of the picture on screen for frost_: at once when it was made ahead, else made now. Several workers make it: a step
// asked for after another may finish first, so only the last one asked for is shown. Then the steps beside it are made ahead.
void Wallpaper::remake_glass () {
	if (!cur_.small) return;
	own_cache();
	if (cur_.glass.frost && glass_frost_ >= 0) glass_cache_[glass_frost_] = cur_.glass.frost;
	int gen = generation_, seq = ++frost_seq_, frost = frost_;
	auto it = glass_cache_.find(frost);
	if (it != glass_cache_.end()) {
		cur_.glass.frost = it->second;
		glass_frost_ = frost;
		if (on_glass) on_glass();
	} else {
		auto small = cur_.small;
		run_background([this, gen, seq, small, frost] {
			auto f = frost_of(*small, frost);
			post_main([this, gen, seq, small, frost, f] {
				if (gen != generation_ || seq != frost_seq_ || cur_.small != small) return;
				cur_.glass.frost = make_texture(*f);
				glass_frost_ = frost;
				if (on_glass) on_glass();
			});
		});
	}
	// only the steps near this one are kept (each is 2 MB of video memory)
	for (auto c = glass_cache_.begin(); c != glass_cache_.end();) c = std::abs(c->first - frost) > 2 * ranges::frost.step ? glass_cache_.erase(c) : std::next(c);
	make_ahead(frost - ranges::frost.step);
	make_ahead(frost + ranges::frost.step);
}

void Wallpaper::configure (const WallpaperConfig &c, const std::string &theme, int frost) {
	theme_ = theme;
	cfg_ = c;
	if (frost != frost_) {
		// another frost: the glass of the picture on screen is made again (the picture itself stays)
		frost_ = frost;
		remake_glass();
	}
	std::string key = source_key();
	if (key == key_) return;
	key_ = key;
	generation_++;
	files_.clear();
	deck_.clear();
	next_at_ = -1;
	if (cfg_.mode == "gradient") {
		prev_ = cur_;
		prev_.shown_at = now_ms();
		cur_ = Layer();
		cur_.shown_at = now_ms();
		return;
	}
	if (cfg_.mode == "image") { load(cfg_.path); return; }
	int gen = generation_;
	WallpaperConfig cfg = cfg_;
	// the folder's pictures (or the ticked ones), listed off the main thread
	run_background([this, gen, cfg] {
		std::vector<std::string> files;
		if (cfg.mode == "folder") {
			for (auto &e : list_dir(cfg.path)) {
				if (e.dir || !is_picture(e.name)) continue;
				std::string f = join_path(cfg.path, e.name);
				long size = file_size(f);
				if (size > 0 && size <= MAX_BYTES) files.push_back(f);
				if (files.size() >= MAX_FILES) break;
			}
		} else files = cfg.paths;
		post_main([this, gen, files] {
			if (gen != generation_) return;
			files_ = files;
			logf("wallpaper: %zu pictures", files_.size());
			advance(now_ms());
		});
	});
}

void Wallpaper::advance (double now) {
	if (files_.empty()) return;
	std::string next;
	if (cfg_.shuffle) {
		// a shuffled deck: every picture once before any comes again, and never the same one twice in a row
		if (deck_.empty()) {
			deck_ = files_;
			std::shuffle(deck_.begin(), deck_.end(), rng());
			if (deck_.size() > 1 && deck_.front() == last_) std::swap(deck_[0], deck_[1 + rng()() % (deck_.size() - 1)]);
		}
		next = deck_.front();
		deck_.erase(deck_.begin());
	} else {
		auto it = std::find(files_.begin(), files_.end(), last_);
		next = it == files_.end() || it + 1 == files_.end() ? files_.front() : *(it + 1);
	}
	last_ = next;
	load(next);
	next_at_ = files_.size() > 1 ? now + cfg_.intervalSec * 1000.0 : -1;
}

void Wallpaper::load (const std::string &path) {
	int gen = generation_, strength = frost_;
	loading_ = true;
	run_background([this, gen, path, strength] {
		std::string data, why;
		std::shared_ptr<Pixels> fitted, small, frost;
		BrightnessMap map;
		if (read_file(path, data, MAX_BYTES * 2)) {
			int w, h;
			image_size(data, w, h);
			if ((long)w * h > MAX_PIXELS) why = "too large";
			else {
				auto px = decode_memory(data, 1920, 1080, &why);
				if (px) fitted = cover(*px, 1920, 1080);
			}
		} else why = "cannot read";
		if (fitted) {
			small = resize(*fitted, 960, 540);
			map = map_from_pixels(resize(*small, 64, 36)->rgba.data());
			frost = frost_of(*small, strength);
		}
		post_main([this, gen, path, strength, fitted, small, frost, map, why] {
			if (gen != generation_) return;
			loading_ = false;
			if (!fitted) { logf("wallpaper %s: %s", path.c_str(), why.c_str()); return; }
			logf("wallpaper: %s", path.c_str());
			prev_ = cur_;
			cur_.tex = make_texture(*fitted);
			cur_.small = small;
			cur_.glass = {make_texture(*frost), map};
			glass_frost_ = strength;
			cur_.shown_at = now_ms();
			// the frost was changed while the picture was being read
			if (strength != frost_) remake_glass();
		});
	});
}

bool Wallpaper::update (double now) {
	if (next_at_ > 0 && now >= next_at_ && paused_at_ < 0 && !loading_) advance(now);
	return now - cur_.shown_at < FADE_MS;
}

double Wallpaper::next_event (double now) const {
	if (next_at_ < 0 || paused_at_ >= 0) return -1;
	return std::max(0.0, next_at_ - now);
}

void Wallpaper::set_paused (bool paused, double now) {
	if (paused) { if (paused_at_ < 0) paused_at_ = now; return; }
	if (paused_at_ < 0) return;
	// back in front: the next picture not sooner than the grace time, nor sooner than its own time
	if (next_at_ > 0) next_at_ = std::max(now + RETURN_GRACE_MS, next_at_);
	paused_at_ = -1;
}

float Wallpaper::fade (double now) const {
	if (settled_) return 1;
	double f = std::min(1.0, (now - cur_.shown_at) / FADE_MS);
	return (float)ease(Ease::ease_in_out, f);
}

void Wallpaper::draw (double now) {
	const Theme &t = theme_named(theme_);
	Rect screen{0, 0, 1920, 1080};
	// the theme's gradient (155deg, start 0%, mid 55%, end 100%) is always underneath
	GradientStop stops[] = {{t.wallpaperStart, 0}, {t.wallpaperMid, 0.55f}, {t.wallpaperEnd, 1}};
	gfx::gradient(screen, 155, stops, 3);
	double f = settled_ ? 1.0 : std::min(1.0, (now - cur_.shown_at) / FADE_MS);
	float fade = (float)ease(Ease::ease_in_out, f);
	if (prev_.tex && f < 1) gfx::image(*prev_.tex, screen, 1 - fade);
	if (cur_.tex) gfx::image(*cur_.tex, screen, fade);
	if (f >= 1) { prev_.tex.reset(); prev_.glass = Glass(); prev_.small.reset(); }
	// .wallpaper__scrim
	GradientStop scrim[] = {{{0, 0, 0, 0.22f}, 0}, {{0, 0, 0, 0.08f}, 0.35f}, {{0, 0, 0, 0}, 0.55f}, {{0, 0, 0, 0.5f}, 1}};
	gfx::gradient(screen, 180, scrim, 4);
}
