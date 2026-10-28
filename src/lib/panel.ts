// Is this TV's screen OLED? Its model name says so (OLED65C1PUB, OLED55B4…; a QNED, NANO or UHD model is an LCD), so the
// pixel shift can follow the screen by itself (Settings → Appearance → Pixel shift: Auto). The answer is kept, since a
// TV does not change its screen.

import {exec} from './apps';
import {isWebOS, lunaCall} from './luna';

const KEY = 'launcher-oled';
const URI = 'luna://com.webos.service.tv.systemproperty/getSystemInfo';

export function isOledModel (model: string): boolean {
	return /^\s*OLED/i.test(model);
}

/** The model name out of a getSystemInfo answer (as JSON text), or ''. */
export function modelOf (out: string): string {
	const match = /"modelName"\s*:\s*"([^"]*)"/.exec(out);
	return match ? match[1] : '';
}

/** The answer kept from an earlier start: true, false, or null when not known yet. */
export function knownOled (): boolean | null {
	try {
		const v = localStorage.getItem(KEY);
		return v === '1' ? true : v === '0' ? false : null;
	} catch { return null; }
}

/** Asks the TV for its model name (straight over Luna; through the Homebrew Channel's shell if that is refused), or
 *  null when it cannot be told. */
export function detectOled (): Promise<boolean | null> {
	const known = knownOled();
	if (known !== null || !isWebOS()) return Promise.resolve(known);
	const keep = (model: string): boolean | null => {
		if (!model) return null;
		const oled = isOledModel(model);
		try { localStorage.setItem(KEY, oled ? '1' : '0'); } catch { /* asked again next time */ }
		return oled;
	};
	return lunaCall(URI, {keys: ['modelName']})
		.then((r) => modelOf(JSON.stringify(r)))
		.catch(() => exec(`luna-send -n 1 ${URI} '{"keys":["modelName"]}'`, 6000).catch(() => ''))
		.then(keep, () => null);
}
