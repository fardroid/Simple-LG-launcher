import {LUNA, SYSTEM_APP} from '../config/constants';
import {lunaCall} from './luna';

/** Launch an app or switch to an input (HDMI inputs are apps too: com.webos.app.hdmiN). */
export function launchApp (id: string, params: object = {}): Promise<void> {
	return lunaCall(LUNA.launch, {id, params}).then(() => undefined);
}

/** Bring up LG's own home screen. */
export function launchLgHome (): Promise<void> {
	return launchApp(SYSTEM_APP.lgHome);
}

/** Whether an app with this ID exists on the TV (permitted from a web app, unlike listing). */
export function appExists (id: string): Promise<boolean> {
	return lunaCall<{exist?: boolean}>(LUNA.appLoadStatus, {appId: id})
		.then((r) => r.exist === true)
		.catch(() => false);
}

/** Open the TV's own Settings app: the first of its known IDs that exists on this TV. When none can be
 *  confirmed (the check itself may be refused on some TVs) the most likely ID is launched anyway. */
export function launchSystemSettings (): Promise<void> {
	const ids = SYSTEM_APP.systemSettings;
	const next = (i: number): Promise<void> => {
		if (i >= ids.length) return launchApp(ids[0]);
		return appExists(ids[i]).then((found) => (found ? launchApp(ids[i]) : next(i + 1)));
	};
	return next(0);
}
