// A copy of the app list kept in localStorage, so the home row can be drawn the instant the
// launcher opens (the real list is then refreshed in the background). Icons are not stored
// twice: they live in the icon cache, and an entry only remembers the icon's path.

import type {AppEntry} from './apps';

export const APPS_CACHE_KEY = 'customlauncher-apps-v1';

export function serializeApps (apps: AppEntry[]): string {
	return JSON.stringify(apps.map((a) => ({
		id: a.id,
		title: a.title,
		color: a.color,
		iconPath: a.iconPath,
		removable: a.removable,
		// data: icons are rebuilt from the icon cache; bundled ones are plain relative paths
		iconUrl: a.iconUrl && a.iconUrl.indexOf('data:') === 0 ? undefined : a.iconUrl
	})));
}

export function deserializeApps (json: string | null, iconByPath: (path: string) => string | undefined): AppEntry[] {
	if (!json) return [];
	try {
		const list = JSON.parse(json) as Partial<AppEntry>[];
		if (!Array.isArray(list)) return [];
		return list
			.filter((a) => typeof a.id === 'string' && typeof a.title === 'string')
			.map((a) => ({
				id: a.id as string,
				title: a.title as string,
				color: a.color,
				iconPath: a.iconPath,
				removable: typeof a.removable === 'boolean' ? a.removable : undefined,
				iconUrl: a.iconUrl || (a.iconPath ? iconByPath(a.iconPath) : undefined)
			}));
	} catch {
		return [];
	}
}

export function sameApps (a: AppEntry[], b: AppEntry[]): boolean {
	if (a.length !== b.length) return false;
	for (let i = 0; i < a.length; i++) {
		if (a[i].id !== b[i].id || a[i].title !== b[i].title || a[i].iconUrl !== b[i].iconUrl || a[i].color !== b[i].color) return false;
	}
	return true;
}

/** `next` with any real icon it lacks filled in from `previous` (same app, same icon file). A load during which some
 *  icons could not be read (the Homebrew service still starting up, say) then never loses an icon already known. */
export function keepKnownIcons (next: AppEntry[], previous: AppEntry[]): AppEntry[] {
	const before: Record<string, AppEntry> = {};
	previous.forEach((a) => { before[a.id] = a; });
	return next.map((a) => {
		const old = before[a.id];
		const hasReal = !!a.iconUrl && a.iconUrl.indexOf('data:') === 0;
		if (hasReal || !old || !old.iconUrl || old.iconUrl.indexOf('data:') !== 0 || old.iconPath !== a.iconPath) return a;
		return {...a, iconUrl: old.iconUrl};
	});
}
