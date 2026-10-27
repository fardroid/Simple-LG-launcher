// The launcher can run in LG Home's place: tools/takeover.sh mounts a copy of its files with LG Home's id
// (com.webos.app.home) over LG's Home app, so the TV opens it for the Home key, Back out of the last app and waking up.
// Running like that it must not start the hand-over helper (it would open the launcher over itself) or close
// "LG Home" (that is now this page), and its settings live in another app's storage, so they are also kept in a file.

import {exec, shellQuote} from './apps';
import {isWebOS} from './luna';
import type {UserConfig} from './storage';

/** True when this page is running as the TV's Home app. */
export const AS_HOME = typeof location !== 'undefined' && location.pathname.indexOf('/com.webos.app.home/') !== -1;

export const CONFIG_FILE = '/media/developer/launcher-config.json';

/** The settings as they apply here. Running as the TV's Home there is no helper and no "LG Home" to close; otherwise
 *  LG Home is closed when the launcher opens only together with the helper (they are one setting, Settings → Home screen). */
export function forHome (config: UserConfig): UserConfig {
	return AS_HOME ? {...config, replaceHome: false, closeHome: false} : {...config, closeHome: config.replaceHome};
}

/** Keeps a copy of the settings on the TV, so the other copy of the launcher (the normal app or the Home one) can pick them up. */
export function saveConfigFile (config: UserConfig): void {
	if (!isWebOS()) return;
	const text = JSON.stringify(config);
	exec(`printf '%s' ${shellQuote(text)} > ${CONFIG_FILE}.new && mv -f ${CONFIG_FILE}.new ${CONFIG_FILE}`, 8000).catch(() => undefined);
}

/** The settings from that file (as text), or null. */
export function readConfigFile (): Promise<string | null> {
	if (!isWebOS()) return Promise.resolve(null);
	return exec(`cat ${CONFIG_FILE} 2>/dev/null`, 8000).then((text) => (text && text.trim().charAt(0) === '{' ? text.trim() : null), () => null);
}
