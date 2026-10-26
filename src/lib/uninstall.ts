// Uninstalling an app from the launcher (hold OK on a tile). It runs through the Homebrew Channel's
// root shell, so it is careful about WHAT it will remove: only apps that look user-installed, and
// never the launcher itself, LG Home, the Homebrew Channel (which the launcher needs to run commands)
// or system apps. The shell command is built only from an app ID that has passed a strict check.

import {APP_ID} from '../config/constants';
import {exec} from './apps';
import {tr} from './i18n';

export const UNINSTALL_LOG = '/tmp/launcher-uninstall.log';

const ID_PATTERN = /^[A-Za-z0-9._-]+$/;

/** Never offered for removal, with the reason shown to the user. */
const PROTECTED: Record<string, [string, string]> = {
	[APP_ID]: ['This is the launcher itself', 'Это сам лаунчер'],
	'com.webos.app.home': ['LG Home is part of the TV', 'LG Home — часть ТВ'],
	'org.webosbrew.hbchannel': ['The launcher needs the Homebrew Channel to run commands', 'Лаунчеру нужен Homebrew Channel для выполнения команд']
};
const SYSTEM_PREFIXES = ['com.webos.', 'com.palm.', 'com.lge.'];
/** Where apps installed by the user live (developer-mode apps, and apps from the LG Content Store). */
const USER_APP_DIRS = ['/media/developer/apps/', '/media/cryptofs/apps/'];

export interface UninstallCandidate {
	id: string;
	/** where the app's icon file is on the TV; tells where the app is installed */
	iconPath?: string;
	/** LG's own "this app can be removed" flag from the launch point, when the TV gives it */
	removable?: boolean;
}

export interface Verdict {
	ok: boolean;
	/** why not (empty when ok) */
	reason: string;
}

/** May this app be offered for uninstalling? Needs positive evidence that it is user-installed. */
export function uninstallVerdict (app: UninstallCandidate): Verdict {
	if (!ID_PATTERN.test(app.id)) return {ok: false, reason: tr('This app ID cannot be used', 'Этот идентификатор приложения нельзя использовать')};
	if (PROTECTED[app.id]) return {ok: false, reason: tr(PROTECTED[app.id][0], PROTECTED[app.id][1])};
	if (SYSTEM_PREFIXES.some((prefix) => app.id.indexOf(prefix) === 0)) return {ok: false, reason: tr('System apps cannot be removed', 'Системные приложения удалять нельзя')};
	if (app.removable === false) return {ok: false, reason: tr('The TV says this app cannot be removed', 'ТВ сообщает, что это приложение нельзя удалить')};
	if (app.removable === true) return {ok: true, reason: ''};
	if (app.iconPath && USER_APP_DIRS.some((dir) => (app.iconPath as string).indexOf(dir) === 0)) return {ok: true, reason: ''};
	return {ok: false, reason: tr('This does not look like an app you installed', 'Похоже, это не установленное вами приложение')};
}

export type UninstallResult = 'removed' | 'still-installed' | 'failed';

/** The shell command: ask the developer-mode remover, then the store remover, checking after each that
 *  the app is really gone. Everything the TV says is also written to UNINSTALL_LOG. Throws for a bad ID. */
export function uninstallCommand (id: string): string {
	if (!ID_PATTERN.test(id)) throw new Error('Invalid app id');
	const present = `luna-send -n 1 luna://com.webos.applicationManager/getAppInfo "{\\"id\\":\\"$ID\\"}" 2>/dev/null | grep -q '"returnValue":true'`;
	const remove = (service: string) => `luna-send -w 20000 -n 1 luna://com.webos.appInstallService/${service} "{\\"id\\":\\"$ID\\",\\"subscribe\\":false}" >> "$LOG" 2>&1; sleep 2`;
	return [
		`ID='${id}'`,
		`LOG='${UNINSTALL_LOG}'`,
		`present () { ${present}; }`,
		'echo "$(date +%H:%M:%S) uninstall $ID" >> "$LOG"',
		`if present; then ${remove('dev/remove')}; fi`,
		`if present; then ${remove('remove')}; fi`,
		'if present; then echo still-installed; else echo removed; fi'
	].join('; ');
}

/** Reads the last word of the command's output. */
export function parseUninstall (output: string): UninstallResult {
	const lines = output.split('\n').map((l) => l.trim()).filter((l) => l.length > 0);
	const last = lines.length ? lines[lines.length - 1] : '';
	return last === 'removed' || last === 'still-installed' ? last : 'failed';
}

export function uninstallApp (id: string): Promise<UninstallResult> {
	return exec(uninstallCommand(id), 70000).then(parseUninstall).catch((): UninstallResult => 'failed');
}
