// App-wide constants that are not visual tokens (see theme/tokens.ts for those).

import appinfo from '../../public/appinfo.json';

export const APP_ID = 'tld.my.simplelauncher';

/** The version shown in Settings → General (from the app's manifest). */
export const APP_VERSION: string = appinfo.version;

/** webOS app IDs the launcher itself needs. */
export const SYSTEM_APP = {
	lgHome: 'com.webos.app.home',
	/** the TV's own Settings app (LG Home's gear opens it); the first that exists on the TV is used */
	systemSettings: ['com.palm.app.settings', 'com.webos.app.settings'],
	liveTv: 'com.webos.app.livetv',
	homebrewChannel: 'org.webosbrew.hbchannel'
} as const;

/** Luna service URIs. A sideloaded web app on webOS 6 may only call methods in the
 *  TV's "public" LS2 group; everything here except homebrewExec is in that group. */
export const LUNA = {
	launch: 'luna://com.webos.applicationManager/launch',
	appLoadStatus: 'luna://com.webos.applicationManager/getAppLoadStatus',
	inputStatus: 'luna://com.webos.service.eim/getAllInputStatus',
	preferences: 'luna://com.webos.service.systemservice/getPreferences',
	/** Homebrew Channel's root shell; only used when the user opts in (see lib/apps.ts) */
	homebrewExec: 'luna://org.webosbrew.hbchannel.service/exec'
} as const;

/** Remote-control key codes as delivered to a webOS web app. */
export const KEY = {
	left: 37, up: 38, right: 39, down: 40,
	enter: 13,
	back: 461,          // webOS "BACK" (needs disableBackHistoryAPI in appinfo.json)
	escape: 27          // handy when developing in a desktop browser
} as const;

export const TIMING = {
	lunaTimeoutMs: 4000,
	toastMs: 2200,
	clockTickMs: 1000,
	longPressMs: 600,       // OK held this long enters/leaves re-order mode
	/** Everything that is not needed to show the home screen (re-reading the app list, the helper check,
	 *  closing LG Home) waits this long after the launcher appears, so the screen comes up first. */
	backgroundWorkMs: 5000
} as const;

/** localStorage key for the user's selections. Bump the suffix on breaking changes. */
export const STORAGE_KEY = 'customlauncher-config-v1';

/** Wallpaper / slideshow settings. */
export const WALLPAPER = {
	releaseAfterMs: 30000,        // the launcher keeps its wallpaper this long after going to the background, so Home shows it at once
	returnGraceMs: 12000,         // after the launcher comes back to the front, the next slideshow picture is not loaded for at least this long
	fadeMs: 1400,                 // cross-fade between pictures (keep in sync with global.css)
	maxPixels: 25000000,          // pictures larger than this are skipped (too heavy for the TV)
	probeTimeoutMs: 15000,
	maxFiles: 400,                // at most this many pictures are used from a folder
	maxBytes: 8 * 1024 * 1024,    // pictures bigger than this file size are skipped
	chunkBytes: 98304,            // read per shell call: ~128 KB of base64, safely under the service's output limit; a multiple of 3 so every chunk's base64 stands alone
	parallelChunks: 3,
	intervals: [10, 30, 60, 300, 900, 1800, 3600]   // seconds, the choices in Settings
} as const;

