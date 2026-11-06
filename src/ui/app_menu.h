// What opens when OK is held on an app or input (components/AppMenu.tsx): move it along the row, hide it, or (apps) uninstall it.
// Uninstall asks first, and the question starts on Cancel.
#pragma once

#include "data/order.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

class AppMenu {
public:
	std::function<void(MoveAction)> on_move;
	std::function<void()> on_hide, on_uninstall, on_close;

	/** `blocked`: why Uninstall cannot be used (empty: it can); `app`: an app (inputs have no Uninstall). */
	void open (const std::string &title, Moves moves, const std::string &hide_detail, bool app, const std::string &blocked, double now);
	/** Straight to the question "Uninstall …?" (the trash button over a tile being moved). */
	void open_confirm (const std::string &title, double now);
	/** A question of its own: Cancel, or `yes` which runs `fn` (Restart the TV). */
	void open_question (const std::string &title, const std::string &text, const std::string &yes, std::function<void()> fn, double now);
	/** "Please wait" with a title and a line of its own, no buttons (Restart the TV, a switch of the way of opening on Home);
	 *  closed with close(). */
	void open_working (const std::string &title, const std::string &text, double now);
	bool open () const { return open_; }
	void close () { open_ = false; }
	bool key_down (int scancode, bool repeat, double now);
	bool key_up (int scancode, double now);
	/** The pointer: over a row it takes the focus; a click runs it. */
	void pointer (float x, float y, double now);
	void click (float x, float y, double now);
	bool animating (double now) const;
	void draw (const std::string &theme, double now);

private:
	struct Row { std::string key, label, detail; bool disabled = false; };
	std::vector<Row> rows () const;
	int first_enabled () const;
	void run (const Row &row);

	bool open_ = false;
	std::string title_, hide_detail_, blocked_;
	Moves moves_{};
	bool app_ = false;
	enum class Step { menu, confirm, working } step_ = Step::menu;
	int index_ = 0;
	std::string q_title_, q_text_, q_yes_;
	std::function<void()> q_fn_;     // set: the question of open_question
	std::string w_text_;             // set: the "please wait" of open_working
	bool confirm_only_ = false;   // opened as the question alone: Back closes it
	bool ok_armed_ = false;      // OK acts on its release; the release of the hold that opened the menu is not a press
	double focus_at_ = -1e9;
	std::vector<std::pair<float, float>> row_spans_;     // where each row was drawn (top, bottom), for the pointer
	float rows_x_ = 0, rows_w_ = 0;
	int row_at (float x, float y) const;
};
