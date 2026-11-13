// The wallpaper (components/Wallpaper.tsx): the theme's gradient, one picture, or a slideshow of a folder or of ticked
// pictures, cross-faded over 1.4 s, with the soft shade at the top and bottom that keeps the clock and the apps readable.
#pragma once

#include "data/config.h"
#include "gfx/gfx.h"
#include "ui/plates.h"
#include "ui/theme.h"

#include <functional>
#include <map>
#include <set>
#include <memory>
#include <string>
#include <vector>

class Wallpaper {
public:
	/** Settings opened: the glass for the Frost steps beside the one in use is made ahead, so even the first step shows at once. */
	void prepare_frost () { if (!cur_.small) return; own_cache(); make_ahead(frost_ - ranges::frost.step); make_ahead(frost_ + ranges::frost.step); }
	/** The glass of the picture on screen was made again (another Frost): what is drawn with it has to be drawn again. */
	std::function<void()> on_glass;
	/** `frost`: % of the glass's full blur (Settings → Frost). */
	void configure (const WallpaperConfig &c, const std::string &theme, int frost);
	/** Moves the slideshow on when its time came. Returns true while something on it is changing (a fade). */
	bool update (double now);
	void draw (double now);
	/** ms until the next slideshow step (-1: none). */
	double next_event (double now) const;
	/** The launcher went to the back or came to the front. */
	void set_paused (bool paused, double now);
	/** What the plates take from a picture: its frosted glass (960 x 540, blurred, with the shade; null for the gradient) and its
	 *  brightness grid (empty for the gradient: the theme's own is used). */
	struct Glass { TexturePtr frost; BrightnessMap map; };
	const Glass &glass () const { return cur_.glass; }
	const Glass &old_glass () const { return prev_.glass; }
	/** How far the change to the current picture has come (0-1, eased as the wallpaper's own fade). */
	float fade (double now) const;
	/** When the current picture came (it changes when another one is shown). */
	double shown_at () const { return cur_.shown_at; }
	/** Drawn as if the change were over (the screen then cross-fades whole pictures of the board instead). */
	void set_settled (bool s) { settled_ = s; }
	/** How much darker the picture is drawn (0: as it is, 0.4: 40% darker); the gradient of a theme is never dimmed. True when
	 *  it changed. The glass of the cards and their light-or-dark choice follow it (dim()). */
	bool set_dim (float d) { if (d == dim_) return false; dim_ = d; return true; }
	float dim () const { return cur_.tex || prev_.tex ? dim_ : 0; }

private:
	struct Layer { TexturePtr tex; Glass glass; std::shared_ptr<Pixels> small; double shown_at = -1e9; };
	void load (const std::string &path);
	void advance (double now);
	std::string source_key () const;

	WallpaperConfig cfg_;
	std::string theme_ = "dusk", key_;
	Layer cur_, prev_;
	std::vector<std::string> files_, deck_;
	std::string last_;
	double next_at_ = -1, paused_at_ = -1;
	int generation_ = 0, frost_ = 100, frost_seq_ = 0;
	void remake_glass ();
	// The glass of the picture on screen for the Frost steps next to the one in use, made ahead (a step from scratch takes a
	// moment on the TV): a step to one of them shows at once. Only the steps near the one in use are kept.
	std::map<int, TexturePtr> glass_cache_;
	std::set<int> glass_making_;
	const Pixels *cache_of_ = nullptr;     // the picture they are of
	int glass_frost_ = -1;                 // the step cur_.glass is of
	void own_cache ();
	void make_ahead (int frost);
	bool loading_ = false, settled_ = false;
	float dim_ = 0;
};
