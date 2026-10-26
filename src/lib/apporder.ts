// Which apps show on the home row. Apps are visible unless the user hides them, so an app installed
// later appears by itself (at the end of the row, or where the user's saved order puts it).
// Older settings kept a list of the apps TO SHOW instead (anything not listed stayed hidden, which is
// why new apps used to be hidden); those are converted once, see hiddenAfterUpgrade.

/** The apps to show: the user's saved order first (skipping hidden and uninstalled ones), then every other
 *  app that isn't hidden, in the TV's order. `hidden === null` means old settings that have not been
 *  converted yet: then only the apps in the saved list show, as before. */
export function visibleApps<T extends {id: string}> (apps: T[], order: string[] | null, hidden: string[] | null): T[] {
	const byId: Record<string, T> = {};
	apps.forEach((a) => { byId[a.id] = a; });
	if (hidden === null) {
		return order ? order.filter((id) => byId[id]).map((id) => byId[id]) : apps;
	}
	const isHidden: Record<string, boolean> = {};
	hidden.forEach((id) => { isHidden[id] = true; });
	const placed: Record<string, boolean> = {};
	const ordered: T[] = [];
	(order || []).forEach((id) => {
		if (byId[id] && !isHidden[id] && !placed[id]) { ordered.push(byId[id]); placed[id] = true; }
	});
	return ordered.concat(apps.filter((a) => !placed[a.id] && !isHidden[a.id]));
}

/** Old settings listed the apps to show: every app that is installed now but not in that list was hidden
 *  (by the user, or by default). This is the hide-list that keeps them hidden. */
export function hiddenAfterUpgrade<T extends {id: string}> (apps: T[], order: string[] | null): string[] {
	if (!order) return [];
	return apps.filter((a) => order.indexOf(a.id) === -1).map((a) => a.id);
}

/** Add an app to the hide-list, or take it off (OK in Settings → Apps, or Hide in the hold-OK menu). */
export function toggleHidden (hidden: string[], id: string): string[] {
	return hidden.indexOf(id) === -1 ? hidden.concat(id) : hidden.filter((x) => x !== id);
}
