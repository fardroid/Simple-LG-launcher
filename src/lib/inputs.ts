// Sources: Live TV (the tuner, if the TV has one) followed by the external inputs
// (HDMI etc.) from the External Input Manager, which is callable from a web app. EIM
// returns the labels set in the TV's input settings and the icon LG's UI uses for
// each port; we ship copies of those icons.

import {APP_ID, LUNA, SYSTEM_APP} from '../config/constants';
import {lunaCall, type LunaResponse} from './luna';
import {appExists} from './launch';
import {tr} from './i18n';

export interface InputSource {
	id: string;        // e.g. HDMI_1
	appId: string;     // e.g. com.webos.app.hdmi1 — launch this to switch
	label: string;     // user-visible name from the TV's input settings
	port: number;
	connected: boolean;
	icon: string;      // file name under icons/inputs/
}

interface EimDevice {
	id: string;
	appId: string;
	label: string;
	port: number;
	connected?: boolean;
	icon?: string;
}

interface EimResponse extends LunaResponse {
	devices?: EimDevice[];
}

const FALLBACK_ICON = 'hdmigeneric.png';

/** Live TV isn't an EIM device, but launching its app switches to the tuner, so it
 *  behaves like any other source. Port 0 sorts it ahead of HDMI 1. */
const LIVE_TV = (): InputSource => ({
	id: 'LIVE_TV',
	appId: SYSTEM_APP.liveTv,
	label: tr('Live TV', 'Прямой эфир'),
	port: 0,
	connected: true,
	icon: 'tv.png'
});

export function listInputs (): Promise<InputSource[]> {
	return Promise.all([listExternalInputs(), appExists(SYSTEM_APP.liveTv)])
		.then(([external, hasTuner]) => (hasTuner ? [LIVE_TV()] : []).concat(external));
}

function listExternalInputs (): Promise<InputSource[]> {
	return lunaCall<EimResponse>(LUNA.inputStatus, {}).then((r) =>
		(r.devices || [])
			.filter((d) => d.appId && (d.label || d.appId === APP_ID))     // the launcher's own entry may have no label
			.sort((a, b) => a.port - b.port)
			.map((d) => ({
				id: d.id,
				appId: d.appId,
				label: d.label || tr('Launcher', 'Лаунчер'),
				port: d.port,
				connected: d.connected !== false,
				icon: d.icon || FALLBACK_ICON
			}))
	);
}

/** Sample data so the layout can be developed in a desktop browser. */
export const mockInputs = (): InputSource[] => [
	LIVE_TV(),
	{id: 'HDMI_1', appId: 'com.webos.app.hdmi1', label: tr('Streaming box', 'Приставка'), port: 1, connected: true, icon: 'streamingbox.png'},
	{id: 'HDMI_2', appId: 'com.webos.app.hdmi2', label: tr('Soundbar', 'Саундбар'), port: 2, connected: true, icon: 'hometheater.png'},
	{id: 'HDMI_3', appId: 'com.webos.app.hdmi3', label: tr('Game console', 'Игровая консоль'), port: 3, connected: true, icon: 'gameconsole.png'},
	{id: 'HDMI_4', appId: 'com.webos.app.hdmi4', label: 'HDMI 4', port: 4, connected: false, icon: 'HDMI_4.png'}
];
