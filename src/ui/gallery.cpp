#include "ui/gallery.h"

#include "core/log.h"
#include "core/sha256.h"
#include "core/tasks.h"
#include "data/apps.h"
#include "data/storage.h"
#include "gfx/image.h"
#include "gfx/text.h"
#include "i18n/i18n.h"
#include "ui/icons.h"
#include "ui/theme.h"
#include "ui/tokens.h"
#include "sys/http.h"

#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <unistd.h>

namespace {

// global.css .screen--gallery .settings (1560 px in the middle), .gallery (5 tiles across), .gallery__tile 280 x 158 + its name
const int COLUMNS = 5;
const float SHEET_X = (1920 - 1560) / 2.f, SHEET_Y = 84;
const float TILE_W = 280, TILE_H = 158, TILE_STEP = 280 + 40, ROW_GAP = 28, ROW_STEP = 158 + 74 + ROW_GAP, VIEWPORT = 760;
const int THUMB_W = 320, THUMB_H = 180;     // lib/thumbs.ts THUMB
const int STACK_W = 240, STACK_H = 135;     // a card of a folder's stack

int clampi (int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); }

void put (const std::string &s, const TextStyle &st, float x, float top, float line, Color c, float opacity = 1) {
	const TextLine &l = text::line(s, st);
	text::draw(s, st, std::round(x), std::round(top + (line - l.h) / 2), c, opacity);
}

Color mix (Color a, Color b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t}; }

/** Long paths keep their end (the current folder) and lose the start. */
std::string short_path (const std::string &path) {
	if (path.size() <= 90) return path;
	size_t from = path.size() - 89;
	while (from < path.size() && ((unsigned char)path[from] & 0xC0) == 0x80) from++;
	return "…" + path.substr(from);
}

std::string group_name (const std::string &group) {
	if (group == "mountains") return tr("Mountains", "Горы");
	if (group == "nature") return tr("Nature", "Природа");
	if (group == "abstract") return tr("Abstract", "Абстракции");
	return tr("Space", "Космос");
}

}  // namespace

void Gallery::open (const std::vector<std::string> &chosen, double now) {
	chosen_ = ticked_ = chosen;
	// the collection's list is read as the gallery opens, so its tile shows its pictures (the list is read once)
	if (store_ && !store_->listed() && !store_->listing()) store_->read_list();
	top_ = true;
	path_.clear();
	entries_.clear();
	remembered_.clear();
	index_ = 0;
	focus_at_ = now;
	scroll_.jump(0);
	focus_.clear();
	load(0);
}

void Gallery::release () {
	thumbs_.clear();
	no_thumb_.clear();
	stacks_.clear();
	generation_++;
	// what was ticked and not chosen in the end is no longer wanted (the wallpaper's settings say what is)
	if (store_) store_->set_ticked({});
}

const CollectionPhoto *Gallery::photo (const std::string &id) const { return store_ ? store_->photo(id) : nullptr; }

void Gallery::sync_ticks () {
	// the ticked photos of the collection that are not on the TV start downloading now, in the background
	if (store_) store_->set_ticked(ticked_);
}

void Gallery::load (int attempt) {
	(void)attempt;
	int gen = ++generation_;
	loading_ = true;
	failed_ = false;
	thumb_busy_ = false;
	bool top = top_;
	std::string path = path_;
	if (in_collection()) {
		// the list is read once while the launcher runs (again only when it could not be)
		if (!store_ || store_->listed()) { loading_ = false; failed_ = !store_; return; }
		store_->read_list([this, gen] {
			if (gen != generation_) return;
			loading_ = false;
			failed_ = !store_->listed();
		});
		return;
	}
	run_background([this, gen, top, path] {
		bool ok = false;
		std::vector<StorageRoot> roots;
		std::vector<DirEntry> entries;
		if (top) roots = list_roots(&ok);
		else entries = list_pictures(path, &ok);
		post_main([this, gen, top, ok, roots, entries] {
			if (gen != generation_) return;
			loading_ = false;
			failed_ = !ok;
			if (top) roots_ = roots;
			else entries_ = entries;
		});
	});
}

std::vector<Gallery::Row> Gallery::rows () const {
	std::vector<Row> out;
	Row gradient{"gradient", "gradient", tr("Default background", "Фон по умолчанию"), tr("Black or a calm gradient, depending on the theme", "Чёрный или спокойный градиент, в зависимости от темы"), ""};
	if (failed_) {
		if (top_) out.push_back(gradient);
		out.push_back({"retry", "retry", tr("Try again", "Повторить"), in_collection() ? tr("Check the internet connection", "Проверьте подключение к интернету") : tr("Read the storage again", "Прочитать память снова"), ""});
		return out;
	}
	if (in_collection()) {
		std::string group = path_.substr(11);
		if (group.empty()) {
			for (auto &g : collection_groups()) {
				long n = 0, on = 0;
				for (auto &p : store_->photos())
					if (p.group == g) { n++; on += std::count(ticked_.begin(), ticked_.end(), collection_file(data_dir_, p.id)); }
				if (n == 0) continue;
				std::string detail = count(n, "photo", "photos", "фото", "фото", "фото");
				if (on > 0) detail += " · " + fmt(tr("{0} chosen", "выбрано: {0}"), std::to_string(on));
				out.push_back({"cg:" + g, "folder", group_name(g), detail, "collection:" + g});
			}
			return out;
		}
		// first, a tile that chooses the whole group (what is not on the TV yet downloads in the background), or clears it
		long n = 0, on = 0, pending = 0, missing = 0;
		for (auto &p : store_->photos()) {
			if (p.group != group) continue;
			std::string file = collection_file(data_dir_, p.id);
			bool ticked = std::find(ticked_.begin(), ticked_.end(), file) != ticked_.end();
			n++;
			on += ticked;
			pending += ticked && !store_->on_tv(file);
			if (!store_->on_tv(file)) missing += p.size;
		}
		if (n > 0) {
			std::string photos = count(n, "photo", "photos", "фото", "фото", "фото");
			if (on == n) out.push_back({"ca:" + group, "callgroup", tr("Clear all", "Снять все"),
				pending > 0 ? fmt(tr("Downloading, {0} left", "Скачивается, осталось {0}"), std::to_string(pending)) : fmt(tr("All {0} chosen", "Выбраны все: {0}"), photos), ""});
			else out.push_back({"ca:" + group, "callgroup", tr("Choose all", "Выбрать все"),
				missing > 0 ? fmt(tr("{0} · {1} MB to download", "{0} · скачать {1} МБ"), {photos, std::to_string(std::max(1l, (missing + (1l << 19)) >> 20))}) : photos, ""});
		}
		for (auto &p : store_->photos()) {
			if (p.group != group) continue;
			std::string detail;      // (the authors and licences are in THIRD_PARTY_NOTICES.md)
			CollectionStore::State st = store_->state(p.id);
			if (st == CollectionStore::State::downloading) detail = fmt(tr("Downloading… {0}%", "Скачивается… {0}%"), std::to_string((int)(store_->fraction(p.id) * 100)));
			else if (st == CollectionStore::State::waiting) detail = tr("Waiting to download", "Ждёт загрузки");
			else if (st == CollectionStore::State::failed) detail = tr("Could not download it, will try again later", "Не скачалось, попробую позже");
			out.push_back({"cp:" + p.id, "cphoto", p.title, detail, collection_file(data_dir_, p.id)});
		}
		return out;
	}
	if (top_) {
		out.push_back(gradient);
		out.push_back({"collection", "collection", tr("Collection", "Коллекция"), tr("Mountains, nature, space, abstract", "Горы, природа, космос, абстракции"), "collection:"});
		bool usb = false;
		for (auto &r : roots_) {
			out.push_back({"root:" + r.path, "root", r.label, r.path, r.path});
			usb = usb || r.path != INTERNAL_ROOT_PATH;
		}
		if (!usb && !loading_) out.push_back({"no-usb", "info", tr("No USB drive found", "USB-накопитель не найден"), tr("Plug one in and open this screen again", "Подключите его и откройте этот экран снова"), ""});
		return out;
	}
	long pictures = 0;
	for (auto &e : entries_) if (!e.dir) pictures++;
	if (pictures > 0)
		out.push_back({"slideshow", "slideshow", tr("Slideshow", "Слайд-шоу"),
			fmt(tr("{0} in {1}", "{0} в папке {1}"), {count(pictures, "picture", "pictures", "картинка", "картинки", "картинок"), base_name(path_)}), ""});
	for (auto &e : entries_) out.push_back({(e.dir ? "dir:" : "pic:") + e.name, e.dir ? "folder" : "picture", e.name, "", join_path(path_, e.name)});
	if (out.empty() && !loading_) out.push_back({"empty", "info", tr("Nothing here", "Здесь пусто"), tr("This folder has no pictures or folders", "В этой папке нет картинок и папок"), ""});
	return out;
}

void Gallery::go_to (const std::string &path, bool top, int restore) {
	remembered_[top_ ? "" : path_] = index_;
	// the previews of other folders go when there are many (about 230 KB each)
	if (thumbs_.size() > 120)
		for (auto it = thumbs_.begin(); it != thumbs_.end();) it = parent_path(it->first) == path ? std::next(it) : thumbs_.erase(it);
	top_ = top;
	path_ = path;
	entries_.clear();
	index_ = restore;
	scroll_.jump(0);
	focus_.clear();
	load(0);
}

void Gallery::leave () {
	// the ticks are the new wallpaper when they changed (none left: the default background)
	bool same = ticked_.size() == chosen_.size();
	for (auto &p : ticked_) same = same && std::find(chosen_.begin(), chosen_.end(), p) != chosen_.end();
	if (same) { if (on_close) on_close(); return; }
	if (on_choose) on_choose(ticked_.empty() ? WallpaperChoice{"gradient", "", {}} : WallpaperChoice{"pictures", "", ticked_});
}

void Gallery::go_up () {
	if (top_) { leave(); return; }
	if (in_collection()) {
		std::string target = path_ == "collection:" ? "" : "collection:";
		int restore = remembered_.count(target) ? remembered_[target] : 0;
		remembered_[path_] = 0;
		go_to(target, target.empty(), restore);
		return;
	}
	bool at_root = false;
	for (auto &r : roots_) at_root = at_root || r.path == path_;
	std::string target = at_root ? "" : parent_path(path_);
	int restore = remembered_.count(target) ? remembered_[target] : 0;
	remembered_[path_] = 0;
	go_to(target, at_root, restore);
}

void Gallery::activate (int i) {
	auto r = rows();
	if (i < 0 || i >= (int)r.size() || loading_) return;
	const Row &row = r[i];
	if (row.kind == "gradient") { if (on_choose) on_choose({"gradient", "", {}}); }
	else if (row.kind == "retry") { index_ = 0; load(1); }
	else if (row.kind == "root" || row.kind == "folder" || row.kind == "collection") go_to(row.path, false, 0);
	else if (row.kind == "cphoto") {
		// ticked at once; a photo not on the TV downloads in the background, and goes on after the gallery closes
		auto it = std::find(ticked_.begin(), ticked_.end(), row.path);
		if (it != ticked_.end()) ticked_.erase(it); else ticked_.push_back(row.path);
		sync_ticks();
	} else if (row.kind == "callgroup") {
		std::string group = row.key.substr(3);
		bool all = true;
		for (auto &p : store_->photos())
			if (p.group == group) all = all && std::find(ticked_.begin(), ticked_.end(), collection_file(data_dir_, p.id)) != ticked_.end();
		for (auto &p : store_->photos()) {
			if (p.group != group) continue;
			std::string file = collection_file(data_dir_, p.id);
			auto it = std::find(ticked_.begin(), ticked_.end(), file);
			if (all) ticked_.erase(it);
			else if (it == ticked_.end()) ticked_.push_back(file);
		}
		sync_ticks();
	} else if (row.kind == "picture") {
		auto it = std::find(ticked_.begin(), ticked_.end(), row.path);
		if (it == ticked_.end()) ticked_.push_back(row.path); else ticked_.erase(it);
	} else if (row.kind == "slideshow") { if (on_choose) on_choose({"folder", path_, {}}); }
}

bool Gallery::key_down (int sc, double now) {
	int last = std::max(0, (int)rows().size() - 1);
	int before = index_;
	if (sc == SDL_SCANCODE_LEFT) index_ = clampi(index_ - 1, 0, last);
	else if (sc == SDL_SCANCODE_RIGHT) index_ = clampi(index_ + 1, 0, last);
	else if (sc == SDL_SCANCODE_UP) { if (index_ >= COLUMNS) index_ -= COLUMNS; }
	// down from the row above the last one goes to the last tile when there is none right under the highlight
	else if (sc == SDL_SCANCODE_DOWN) { if (index_ / COLUMNS < last / COLUMNS) index_ = std::min(last, index_ + COLUMNS); }
	else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) activate(index_);
	else if (sc == SDL_SCANCODE_WEBOS_BACK || sc == SDL_SCANCODE_ESCAPE) go_up();
	if (index_ != before) focus_at_ = now;
	return true;
}

void Gallery::next_thumb () {
	// the picture (or folder) nearest the highlight that has no preview yet
	if (thumb_busy_ || loading_) return;
	auto r = rows();
	int best = -1;
	for (int i = 0; i < (int)r.size(); i++) {
		bool picture = r[i].kind == "picture" || r[i].kind == "cphoto", folder = r[i].kind == "folder" || r[i].kind == "collection";
		if (picture && (thumbs_.count(r[i].path) || no_thumb_.count(r[i].path))) continue;
		if (folder && (stacks_.count(r[i].path) || (r[i].kind == "collection" && (!store_ || !store_->listed())))) continue;
		if (!picture && !folder) continue;
		if (best < 0 || std::abs(i - index_) < std::abs(best - index_)) best = i;
	}
	if (best < 0) return;
	thumb_busy_ = true;
	if (r[best].kind == "folder" || r[best].kind == "collection") { make_stack(r[best].path); return; }
	std::string path = r[best].path;
	int gen = generation_;
	// a collection photo not on the TV shows its preview, downloaded once and kept with the others
	std::string source = path, thumb_url, thumb_sum;
	if (r[best].kind == "cphoto")
		if (const CollectionPhoto *p = photo(r[best].key.substr(3))) {
			source = collection_thumb(data_dir_, p->id);
			thumb_url = p->thumb;
			thumb_sum = p->thumbSha256;
		}
	run_background([this, path, source, thumb_url, thumb_sum, gen] {
		std::string from = path;
		if (!thumb_url.empty() && !file_exists(path)) {
			from = source;
			if (sha256_file(source) != thumb_sum) {
				make_dirs(parent_path(source));
				HttpResult r = http_download(thumb_url, source, [] (long, long) { return true; }, 30, 1l << 20);
				if (!r.ok || sha256_file(source) != thumb_sum) { unlink(source.c_str()); from.clear(); }
			}
		}
		// a jpeg is decoded at a scale just over the preview's size, then cut to 16:9
		std::shared_ptr<Pixels> px = from.empty() ? nullptr : decode_file(from, THUMB_W, THUMB_H);
		if (px) px = cover(*px, THUMB_W, THUMB_H);
		post_main([this, path, px, gen] {
			if (gen != generation_) return;
			thumb_busy_ = false;
			if (px) thumbs_[path] = make_texture(*px);
			else no_thumb_.insert(path);
		});
	});
}

void Gallery::make_stack (const std::string &path) {
	// a folder shows a stack of its first three pictures: a group of the collection its first photos (their previews,
	// downloaded once), the collection a photo of each of its first groups, a folder on a drive its first pictures
	struct Source { std::string file, preview, url, sum; };
	std::vector<Source> sources;
	auto add = [&] (const CollectionPhoto &p) {
		sources.push_back({collection_file(data_dir_, p.id), collection_thumb(data_dir_, p.id), p.thumb, p.thumbSha256});
	};
	if (path == "collection:") {
		for (auto &g : collection_groups())
			for (auto &p : store_->photos()) if (p.group == g) { add(p); break; }
	} else if (path.rfind("collection:", 0) == 0) {
		for (auto &p : store_->photos()) if (p.group == path.substr(11)) add(p);
	}
	if (sources.size() > 3) sources.resize(3);
	int gen = generation_;
	run_background([this, path, sources, gen] {
		std::vector<std::string> files;
		if (path.rfind("collection:", 0) != 0) {
			bool ok = false;
			for (auto &e : list_pictures(path, &ok))
				if (!e.dir && files.size() < 3) files.push_back(join_path(path, e.name));
		}
		for (auto &s : sources) {
			if (file_exists(s.file)) { files.push_back(s.file); continue; }
			if (sha256_file(s.preview) != s.sum) {
				make_dirs(parent_path(s.preview));
				HttpResult r = http_download(s.url, s.preview, [] (long, long) { return true; }, 30, 1l << 20);
				if (!r.ok || sha256_file(s.preview) != s.sum) { unlink(s.preview.c_str()); continue; }
			}
			files.push_back(s.preview);
		}
		std::vector<std::shared_ptr<Pixels>> pixels;
		for (auto &f : files) {
			std::shared_ptr<Pixels> px = decode_file(f, STACK_W, STACK_H);
			if (px) pixels.push_back(cover(*px, STACK_W, STACK_H));
		}
		post_main([this, path, pixels, gen] {
			if (gen != generation_) return;
			thumb_busy_ = false;
			std::vector<TexturePtr> cards;
			for (auto &px : pixels) cards.push_back(make_texture(*px));
			stacks_[path] = cards;     // empty: the folder has no pictures (its sign stays)
		});
	});
}

bool Gallery::animating (double now) const {
	if (now - focus_at_ < motion::FOCUS_MS || scroll_.moving(now)) return true;
	for (auto &f : focus_) if (f.second.moving(now)) return true;
	// the bar of the picture downloading now moves on
	return store_ && in_collection() && store_->busy();
}

void Gallery::draw (double now) {
	const Theme &t = theme_named(theme_);
	next_thumb();
	gfx::rect({0, 0, 1920, 1080}, 0, t.screen.with_alpha(t.screen.a * SCREEN_OPACITY));      // the wallpaper a little through it
	put(tr("Wallpaper", "Выбор обоев", "Choose wallpaper"), TextStyle{Face::ui, 46, 600, -0.46f}, SHEET_X, SHEET_Y, 56, t.textPrimary);
	float path_y = SHEET_Y + 56 + 34;
	TextStyle pst{Face::ui, 19, 600, 19 * 0.08f};
	std::string where = top_ ? tr("Locations", "Расположения") : !in_collection() ? short_path(path_)
		: path_ == "collection:" ? tr("Collection", "Коллекция") : tr("Collection", "Коллекция") + " · " + group_name(path_.substr(11));
	put(text::ellipsize(text::upper(where), pst, 1500), pst, SHEET_X + 26, path_y, 24, t.textMuted);
	float top = path_y + 24 + 12;
	if (loading_) {
		put(tr("Loading…", "Загрузка…"), TextStyle{Face::ui, 26, 400, 0}, SHEET_X + 8, top + 40, 34, t.textSecondary);
		return;
	}
	if (failed_ && !top_)
		put(tr("Could not read the folder", "Не удалось прочитать папку"), TextStyle{Face::ui, 26, 400, 0}, SHEET_X + 8, top + 40 - 74, 34, t.danger);
	auto r = rows();
	index_ = clampi(index_, 0, std::max(0, (int)r.size() - 1));
	// the highlighted row of tiles stays in the middle of the gallery (clamped at the ends)
	int lines = ((int)r.size() + COLUMNS - 1) / COLUMNS;
	float total = lines * ROW_STEP - ROW_GAP;
	float offset = total <= VIEWPORT ? 0 : std::max(0.f, std::min((index_ / COLUMNS) * ROW_STEP - (VIEWPORT - ROW_STEP) / 2, total - VIEWPORT));
	scroll_.set(-std::round(offset), now, motion::SCROLL_MS, Ease::scroll);
	float x0 = SHEET_X, y0 = top + 24 + (float)scroll_.at(now);
	gfx::push_clip({SHEET_X - 40, top, 1560 + 80, VIEWPORT + 48});
	auto draw_one = [&] (int i) {
		const Row &row = r[i];
		float x = x0 + (i % COLUMNS) * TILE_STEP, y = y0 + (i / COLUMNS) * ROW_STEP;
		if (y > 1080 || y + ROW_STEP < top) return;
		bool focused = i == index_;
		if (!focus_.count(row.key)) focus_[row.key].jump(focused ? 1 : 0);
		Tween &tw = focus_[row.key];
		tw.set(focused ? 1 : 0, now, motion::FOCUS_MS, Ease::ease_out);
		float f = (float)tw.at(now);
		float opacity = row.kind == "info" ? 0.8f : 1;
		float s = 1 + (float)(motion::FOCUS_SCALE - 1) * f;
		Rect frame{x + TILE_W * (1 - s) / 2, y + TILE_H * (1 - s) / 2, TILE_W * s, TILE_H * s};
		float radius = 18 * s;
		gfx::set_alpha(opacity);
		bool photo_row = row.kind == "picture" || row.kind == "cphoto";
		auto stack = row.kind == "folder" || row.kind == "collection" ? stacks_.find(row.path) : stacks_.end();
		bool stacked = stack != stacks_.end() && !stack->second.empty();
		if (!stacked) {
			gfx::ring(frame, radius, 2 * s, t.tileBorder);
			gfx::rect(frame, radius, {1, 1, 1, 0.08f});
		}
		auto thumb = photo_row ? thumbs_.find(row.path) : thumbs_.end();
		if (stacked) {
			// the folder's pictures as a stack of cards: the front one low and left, each one behind higher and to the right
			const auto &cards = stack->second;
			int n = (int)cards.size();
			float ox = 18 * s, oy = 12 * s, spread = (float)(n - 1);
			float w = frame.w - 24 * s - spread * ox, h = w * 9 / 16;
			if (h > frame.h - 10 * s - spread * oy) { h = frame.h - 10 * s - spread * oy; w = h * 16 / 9; }
			float x0 = frame.x + (frame.w - w - spread * ox) / 2, y0 = frame.y + (frame.h - h - spread * oy) / 2 + spread * oy;
			for (int k = n - 1; k >= 0; k--) {
				Rect card{x0 + k * ox, y0 - k * oy, w, h};
				gfx::shadow(card, 10 * s, 4 * s, 14 * s, {0, 0, 0, 0.55f});
				gfx::image(*cards[k], card, 1, 10 * s, {1, 1, 1, 1}, k == 0 ? 1.f : 0.8f);
				gfx::ring(card, 10 * s, 1.5f * s, {1, 1, 1, k == 0 ? 0.85f : 0.6f});
			}
		}
		else if (thumb != thumbs_.end() && thumb->second) gfx::image(*thumb->second, frame, 1, radius);
		else if (row.kind == "gradient") {
			GradientStop stops[] = {{Color::hex("#1B2240"), 0}, {Color::hex("#3A2350"), 0.55f}, {Color::hex("#0D0F18"), 1}};
			gfx::gradient(frame, 135, stops, 3, 1, radius);
		} else {
			std::string icon = row.kind == "retry" ? "info" : row.kind == "collection" ? "collection" : row.kind == "callgroup" ? "check" : row.kind == "root" ? (row.path == INTERNAL_ROOT_PATH ? "folder" : "drive") : row.kind;
			bool waiting = photo_row && !no_thumb_.count(row.path);
			if (TexturePtr g = icons::get(join_path(package_dir(), "glyphs/" + icon + ".png"), 64)) {
				float k = 64 * s;
				gfx::image(*g, {frame.x + (frame.w - k) / 2, frame.y + (frame.h - k) / 2, k, k}, waiting ? 0.35f : 1, 0, mix(t.textSecondary, t.textPrimary, f));
			}
		}
		// a collection photo: a bar while it downloads, and an arrow while it is not on the TV
		if (row.kind == "cphoto") {
			std::string id = row.key.substr(3);
			if (store_->state(id) == CollectionStore::State::downloading) {
				Rect bar{frame.x + 14 * s, frame.y + frame.h - 22 * s, frame.w - 28 * s, 8 * s};
				gfx::rect(bar, bar.h / 2, {0, 0, 0, 0.55f});
				gfx::rect({bar.x, bar.y, std::max(bar.h, bar.w * (float)store_->fraction(id)), bar.h}, bar.h / 2, t.badge);
			} else if (!store_->on_tv(row.path) && std::find(ticked_.begin(), ticked_.end(), row.path) == ticked_.end()) {
				Rect badge{frame.x + frame.w - (12 + 34) * s, frame.y + 12 * s, 34 * s, 34 * s};
				gfx::rect(badge, badge.w / 2, {0, 0, 0, 0.55f});
				if (TexturePtr g = icons::get(join_path(package_dir(), "glyphs/download.png"), 24)) gfx::image(*g, {badge.x + 7 * s, badge.y + 7 * s, 20 * s, 20 * s}, 1, 0, {1, 1, 1, 1});
			}
		}
		if (photo_row && std::find(ticked_.begin(), ticked_.end(), row.path) != ticked_.end()) {
			Rect badge{frame.x + frame.w - (12 + 34) * s, frame.y + 12 * s, 34 * s, 34 * s};
			gfx::rect(badge, badge.w / 2, t.badge);
			if (TexturePtr g = icons::get(join_path(package_dir(), "glyphs/check-bold.png"), 24)) gfx::image(*g, {badge.x + 7 * s, badge.y + 7 * s, 20 * s, 20 * s}, 1, 0, t.badgeText);
		}
		if (f > 0) gfx::ring(frame, radius, 5 * s, t.tileBorderFocus.with_alpha(f));
		TextStyle lst{Face::ui, 23, 400, 0}, dst{Face::ui, 18, 400, 0};
		std::string label = text::ellipsize(row.label, lst, (int)TILE_W - 8);
		const TextLine &l = text::line(label, lst);
		put(label, lst, x + (TILE_W - l.w) / 2, y + TILE_H + 14, 30, mix(t.textSecondary, t.textPrimary, f));
		if (!row.detail.empty()) {
			std::string d = text::ellipsize(row.detail, dst, (int)TILE_W - 8);
			const TextLine &dl = text::line(d, dst);
			put(d, dst, x + (TILE_W - dl.w) / 2, y + TILE_H + 14 + 30, 24, t.textMuted);
		}
		gfx::set_alpha(1);
	};
	for (int i = 0; i < (int)r.size(); i++) if (i != index_) draw_one(i);
	if (!r.empty()) draw_one(index_);        // over its neighbours, as it grows
	gfx::pop_clip();
}
