// One order for the apps and inputs on the home row (so, say, HDMI 1 can come first).
// The order is a list of tile keys. Tiles not in the list (a new app, say) follow the listed ones.

export type MoveAction = 'start' | 'left' | 'right' | 'end';

/** `items` sorted by the saved key list; items it doesn't name keep their usual order after the listed ones. */
export function orderTiles<T> (items: T[], keyOf: (item: T) => string, order: string[] | null): T[] {
	if (!order || order.length === 0) return items;
	const byKey: Record<string, T> = {};
	items.forEach((item) => { byKey[keyOf(item)] = item; });
	const placed: Record<string, boolean> = {};
	const listed: T[] = [];
	order.forEach((key) => {
		if (byKey[key] !== undefined && !placed[key]) { listed.push(byKey[key]); placed[key] = true; }
	});
	return listed.concat(items.filter((item) => !placed[keyOf(item)]));
}

/** Which moves are possible for `key` in `keys`. */
export function movesFor (keys: string[], key: string): {start: boolean; left: boolean; right: boolean; end: boolean} {
	const at = keys.indexOf(key);
	const notLast = at >= 0 && at < keys.length - 1;
	return {start: at > 0, left: at > 0, right: notLast, end: notLast};
}

/** `keys` with `key` moved to the start or the end, or one place left or right. Unchanged when the move isn't possible. */
export function moveKey (keys: string[], key: string, action: MoveAction): string[] {
	const at = keys.indexOf(key);
	if (at === -1) return keys.slice();
	const to = action === 'start' ? 0 : action === 'end' ? keys.length - 1 : action === 'left' ? at - 1 : at + 1;
	if (to < 0 || to > keys.length - 1 || to === at) return keys.slice();
	const next = keys.slice();
	next.splice(at, 1);
	next.splice(to, 0, key);
	return next;
}

/** `current` with the places held by `subset`'s keys filled in the order `subset` gives
 *  (used when Settings re-orders only the apps, or only the inputs). */
export function reslot (current: string[], subset: string[]): string[] {
	const member: Record<string, boolean> = {};
	current.forEach((k) => { member[k] = true; });
	const mine = subset.filter((k) => member[k]);
	const isMine: Record<string, boolean> = {};
	mine.forEach((k) => { isMine[k] = true; });
	let n = 0;
	return current.map((k) => (isMine[k] ? mine[n++] : k));
}

export interface OrderUpdate {
	tileOrder: string[];
	appIds: string[] | null;
	sourceIds: string[];
}

/** The settings to save after the home row was put in the order `keys` (apps and inputs only).
 *  The apps' and inputs' own saved orders follow it, and keep the entries that aren't on the row at the moment
 *  (a hidden app, an input that is unplugged) after the ones that are. */
export function orderUpdate (keys: string[], shownApps: string[], shownInputs: string[], oldAppIds: string[] | null, oldSourceIds: string[]): OrderUpdate {
	const isApp: Record<string, boolean> = {};
	shownApps.forEach((k) => { isApp[k] = true; });
	const isInput: Record<string, boolean> = {};
	shownInputs.forEach((k) => { isInput[k] = true; });
	const apps = keys.filter((k) => isApp[k]);
	const inputs = keys.filter((k) => isInput[k]);
	const rest = (old: string[], used: string[]) => old.filter((k) => used.indexOf(k) === -1);
	return {
		tileOrder: keys.slice(),
		appIds: oldAppIds === null ? null : apps.concat(rest(oldAppIds, apps)),
		sourceIds: inputs.concat(rest(oldSourceIds, inputs))
	};
}
