// Small system helpers that go through the Homebrew Channel's root shell.

import {exec} from './apps';
import {isWebOS} from './luna';

/** Ends LG's own home app so its memory is free. The pattern matches the exact app id (not
 *  look-alikes such as com.webos.app.homeconnect), and the leading "[c]" stops the pattern
 *  from matching the very shell command that carries it. webOS starts the home app again
 *  by itself the next time it is needed (for example the "LG Home" tile). */
export const CLOSE_HOME_COMMAND = `H=com.webos.app; front=$(luna-send -n 1 luna://com.webos.applicationManager/getForegroundAppInfo '{}' 2>/dev/null); case "$front" in *"\\"appId\\":\\"$H.home\\""*) echo skipped;; *) pkill -f '[c]om\\.webos\\.app\\.home([^A-Za-z0-9._-]|$)'; echo done;; esac`;

export function closeLgHome (): Promise<void> {
	if (!isWebOS()) return Promise.resolve();
	return exec(CLOSE_HOME_COMMAND, 5000).then(() => undefined);
}

/** Ends LG Home and reports available memory before and after (-1 when unknown). */
export function closeHomeAndMeasure (): Promise<{before: number; after: number}> {
	return memAvailableMB().then((before) => closeLgHome()
		.then(() => new Promise<void>((resolve) => window.setTimeout(resolve, 1500)))
		.then(() => memAvailableMB())
		.then((after) => ({before, after})));
}

/** Available memory in MB (MemAvailable from /proc/meminfo), or -1 when unknown. */
export function memAvailableMB (): Promise<number> {
	if (!isWebOS()) return Promise.resolve(-1);
	return exec("grep MemAvailable /proc/meminfo", 5000).then((out) => {
		const match = /MemAvailable:\s+(\d+) kB/.exec(out);
		return match ? Math.round(Number(match[1]) / 1024) : -1;
	}).catch(() => -1);
}

// ---------- which app the Home button (and leaving an app with Back) opens ----------
// webOS keeps this in a system setting, defaultApps.home. LG's default is com.webos.app.home.

export const LG_HOME_ID = 'com.webos.app.home';
/** Is the launcher in LG Home's place (tools/takeover.sh mounts a folder over LG Home's app folder)? null when unknown. */
export function takeoverActive (): Promise<boolean | null> {
	if (!isWebOS()) return Promise.resolve(false);
	return exec(`grep -c ' /usr/palm/applications/${LG_HOME_ID} ' /proc/mounts; true`, 6000).then((out) => parseInt(out.trim(), 10) > 0, () => null);
}
