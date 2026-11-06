#include "data/order.h"

std::vector<std::string> toggle_hidden (const std::vector<std::string> &hidden, const std::string &id) {
	std::vector<std::string> out;
	bool found = false;
	for (auto &h : hidden) { if (h == id) found = true; else out.push_back(h); }
	if (!found) out.push_back(id);
	return out;
}

Moves moves_for (const std::vector<std::string> &keys, const std::string &key) {
	auto it = std::find(keys.begin(), keys.end(), key);
	int at = it == keys.end() ? -1 : (int)(it - keys.begin());
	bool not_last = at >= 0 && at < (int)keys.size() - 1;
	return {at > 0, at > 0, not_last, not_last};
}

std::vector<std::string> move_key (const std::vector<std::string> &keys, const std::string &key, MoveAction action) {
	auto it = std::find(keys.begin(), keys.end(), key);
	if (it == keys.end()) return keys;
	int at = (int)(it - keys.begin()), last = (int)keys.size() - 1;
	int to = action == MoveAction::start ? 0 : action == MoveAction::end ? last : action == MoveAction::left ? at - 1 : at + 1;
	if (to < 0 || to > last || to == at) return keys;
	std::vector<std::string> next = keys;
	next.erase(next.begin() + at);
	next.insert(next.begin() + to, key);
	return next;
}

std::vector<std::string> reslot (const std::vector<std::string> &current, const std::vector<std::string> &subset) {
	std::unordered_set<std::string> member(current.begin(), current.end());
	std::vector<std::string> mine;
	for (auto &k : subset) if (member.count(k)) mine.push_back(k);
	std::unordered_set<std::string> is_mine(mine.begin(), mine.end());
	std::vector<std::string> out;
	size_t n = 0;
	for (auto &k : current) out.push_back(is_mine.count(k) ? mine[n++] : k);
	return out;
}

OrderUpdate order_update (const std::vector<std::string> &keys, const std::vector<std::string> &shown_apps, const std::vector<std::string> &shown_inputs,
		const std::optional<std::vector<std::string>> &old_app_ids, const std::vector<std::string> &old_source_ids) {
	std::unordered_set<std::string> is_app(shown_apps.begin(), shown_apps.end()), is_input(shown_inputs.begin(), shown_inputs.end());
	std::vector<std::string> apps, inputs;
	for (auto &k : keys) {
		if (is_app.count(k)) apps.push_back(k);
		if (is_input.count(k)) inputs.push_back(k);
	}
	auto rest = [] (const std::vector<std::string> &old, const std::vector<std::string> &used) {
		std::vector<std::string> out;
		for (auto &k : old) if (std::find(used.begin(), used.end(), k) == used.end()) out.push_back(k);
		return out;
	};
	OrderUpdate u;
	u.tileOrder = keys;
	if (old_app_ids) {
		auto r = rest(*old_app_ids, apps);
		std::vector<std::string> a = apps;
		a.insert(a.end(), r.begin(), r.end());
		u.appIds = a;
	}
	auto r = rest(old_source_ids, inputs);
	u.sourceIds = inputs;
	u.sourceIds.insert(u.sourceIds.end(), r.begin(), r.end());
	return u;
}
