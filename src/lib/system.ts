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
const APP_ID_PATTERN = /^[A-Za-z0-9._-]+$/;

/** The shell command that sets the home app. Throws for anything that isn't a plain app id. */
export function homeSettingCommand (appId: string): string {
	if (!APP_ID_PATTERN.test(appId)) throw new Error('Invalid app id');
	return `luna-send -n 1 -f luna://com.webos.settingsservice/setSystemSettings '{"category":"general","settings":{"defaultApps":{"home":"${appId}"}}}'`;
}

const GET_HOME_COMMAND = `luna-send -n 1 -f luna://com.webos.settingsservice/getSystemSettings '{"category":"general","keys":["defaultApps"]}'`;

/** Reads the app id out of getSystemSettings' reply, or null. */
export function parseHomeApp (output: string): string | null {
	const match = /"home"\s*:\s*"([^"]+)"/.exec(output);
	return match ? match[1] : null;
}

export function getHomeApp (): Promise<string | null> {
	if (!isWebOS()) return Promise.resolve(null);
	return exec(GET_HOME_COMMAND, 6000).then(parseHomeApp).catch(() => null);
}

/** Sets the home app. Resolves true when the TV accepted it. */
export function setHomeApp (appId: string): Promise<boolean> {
	if (!isWebOS()) return Promise.resolve(false);
	return exec(homeSettingCommand(appId), 6000).then((out) => /"returnValue"\s*:\s*true/.test(out)).catch(() => false);
}

// ---------- the LG logo when the TV starts ----------
// LG's hidden menu (press Mute repeatedly on an input with no signal) has "Show LG logo when turning on".
// It is the system setting other.lgLogoDisplay, "on" or "off" (found by watching what the TV changed).

export type LogoState = 'on' | 'off' | 'unsupported';

export const GET_LOGO_COMMAND = `luna-send -n 1 -f luna://com.webos.settingsservice/getSystemSettings '{"category":"other","keys":["lgLogoDisplay"]}'`;

export function setLogoCommand (on: boolean): string {
	return `luna-send -n 1 -f luna://com.webos.settingsservice/setSystemSettings '{"category":"other","settings":{"lgLogoDisplay":"${on ? 'on' : 'off'}"}}'`;
}

/** Reads the setting out of getSystemSettings' reply: 'unsupported' when the TV answered but has no such setting, null when the reply isn't an answer. */
export function parseLogo (output: string): LogoState | null {
	const value = /"lgLogoDisplay"\s*:\s*"([^"]*)"/.exec(output);
	if (value) return value[1] === 'off' ? 'off' : value[1] === 'on' ? 'on' : 'unsupported';
	return /"returnValue"\s*:\s*true/.test(output) ? 'unsupported' : null;
}

export function getLogo (): Promise<LogoState | null> {
	if (!isWebOS()) return Promise.resolve(null);
	return exec(GET_LOGO_COMMAND, 6000).then(parseLogo).catch(() => null);
}

/** Sets it, then reads it back. Resolves true only when the TV now reports the wanted value. */
export function setLogo (on: boolean): Promise<boolean> {
	if (!isWebOS()) return Promise.resolve(false);
	return exec(setLogoCommand(on), 6000).then(() => getLogo()).then((now) => now === (on ? 'on' : 'off')).catch(() => false);
}
