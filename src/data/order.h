// Which apps show on the home row and in what order (lib/apporder.ts, lib/tileorder.ts).
#pragma once

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/** The apps to show: the saved order first (skipping hidden and uninstalled ones), then every other app that isn't hidden, in
 *  the TV's order. `hidden` empty optional: old settings not converted yet, then only the apps in the saved list show. */
template <class T>
std::vector<T> visible_apps (const std::vector<T> &apps, const std::optional<std::vector<std::string>> &order,
		const std::optional<std::vector<std::string>> &hidden, std::function<std::string(const T &)> id_of) {
	std::unordered_map<std::string, const T *> by_id;
	for (auto &a : apps) by_id[id_of(a)] = &a;
	std::vector<T> out;
	if (!hidden) {
		if (!order) return apps;
		for (auto &id : *order) if (by_id.count(id)) out.push_back(*by_id[id]);
		return out;
	}
	std::unordered_set<std::string> is_hidden(hidden->begin(), hidden->end()), placed;
	if (order)
		for (auto &id : *order)
			if (by_id.count(id) && !is_hidden.count(id) && !placed.count(id)) { out.push_back(*by_id[id]); placed.insert(id); }
	for (auto &a : apps) if (!placed.count(id_of(a)) && !is_hidden.count(id_of(a))) out.push_back(a);
	return out;
}

/** Old settings listed the apps to show: this is the hide-list that keeps the others hidden. */
template <class T>
std::vector<std::string> hidden_after_upgrade (const std::vector<T> &apps, const std::optional<std::vector<std::string>> &order,
		std::function<std::string(const T &)> id_of) {
	std::vector<std::string> out;
	if (!order) return out;
	for (auto &a : apps) if (std::find(order->begin(), order->end(), id_of(a)) == order->end()) out.push_back(id_of(a));
	return out;
}

std::vector<std::string> toggle_hidden (const std::vector<std::string> &hidden, const std::string &id);

/** `items` sorted by the saved key list; items it doesn't name keep their order after the listed ones. */
template <class T>
std::vector<T> order_tiles (const std::vector<T> &items, std::function<std::string(const T &)> key_of, const std::optional<std::vector<std::string>> &order) {
	if (!order || order->empty()) return items;
	std::unordered_map<std::string, const T *> by_key;
	for (auto &i : items) by_key[key_of(i)] = &i;
	std::unordered_set<std::string> placed;
	std::vector<T> out;
	for (auto &k : *order) if (by_key.count(k) && !placed.count(k)) { out.push_back(*by_key[k]); placed.insert(k); }
	for (auto &i : items) if (!placed.count(key_of(i))) out.push_back(i);
	return out;
}

enum class MoveAction { start, left, right, end };
struct Moves { bool start, left, right, end; };
Moves moves_for (const std::vector<std::string> &keys, const std::string &key);
std::vector<std::string> move_key (const std::vector<std::string> &keys, const std::string &key, MoveAction action);
std::vector<std::string> reslot (const std::vector<std::string> &current, const std::vector<std::string> &subset);

struct OrderUpdate {
	std::vector<std::string> tileOrder;
	std::optional<std::vector<std::string>> appIds;
	std::vector<std::string> sourceIds;
};
OrderUpdate order_update (const std::vector<std::string> &keys, const std::vector<std::string> &shown_apps, const std::vector<std::string> &shown_inputs,
		const std::optional<std::vector<std::string>> &old_app_ids, const std::vector<std::string> &old_source_ids);
