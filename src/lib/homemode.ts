// How the launcher replaces LG Home, chosen in Settings → Home screen → "How the launcher opens". The switching itself is
// done on the TV by tools/homemode.sh (packaged in the app's tools/ folder), one way at a time: see that script.
// The helper is the exception: it is installed by the launcher itself (homehook.ts) after homemode.sh has switched
// everything else off.

import {exec} from './apps';
import {APP_ID} from '../config/constants';
import {isWebOS} from './luna';
import {tr} from './i18n';

export type HomeMode = 'none' | 'helper' | 'default' | 'mount';
export const HOME_MODES: readonly HomeMode[] = ['default', 'mount', 'helper', 'none'];

export interface HomeModeInfo {
	mode: HomeMode;
	/** the TV lets an app other than LG Home be chosen as Home (LG Home lists the "home" category) */
	canDefault: boolean;
	/** webOS release, such as 10.3.2 (webOS 25); '' when unknown */
	release: string;
}

const TOOLS = `/media/developer/apps/usr/palm/applications/${APP_ID}/tools`;
export const RESULT_FILE = '/tmp/launcher-homemode.result';
export const STATUS_COMMAND = `sh '${TOOLS}/homemode.sh' status`;
export const runCommand = (mode: Exclude<HomeMode, 'helper'>): string => `sh '${TOOLS}/homemode.sh' run ${mode}`;

export function parseModeStatus (out: string): HomeModeInfo | null {
	const mode = /^mode=(none|helper|default|mount)$/m.exec(out);
	if (!mode) return null;
	const release = /^release=(.*)$/m.exec(out);
	return {mode: mode[1] as HomeMode, canDefault: /^can_default=yes$/m.test(out), release: release ? release[1].trim() : ''};
}

/** The result line of a switch: ok, failed with the reason, or null while it is still running. */
export function parseResult (out: string): {ok: boolean; mode: string; why: string} | null {
	const line = out.trim().split('\n').pop() || '';
	const match = /^(ok|failed) (\w+)(?::\s*(.*))?$/.exec(line);
	return match ? {ok: match[1] === 'ok', mode: match[2], why: match[3] || ''} : null;
}

/** While the official way is on, its copies on the TV are brought up to this version's and its wake-up guard started
 *  (tools/defaulthome.sh refresh). Run once each time the launcher starts, so an update needs nothing more: the copies
 *  outlive the app, and an older version's lacked the guard. Nothing happens when the official way is off. */
export const REFRESH_COMMAND = `sh '${TOOLS}/defaulthome.sh' refresh; true`;

export function refreshDefaultHome (): Promise<string> {
	if (!isWebOS()) return Promise.resolve('off');
	return exec(REFRESH_COMMAND, 15000);
}

export function readHomeMode (): Promise<HomeModeInfo | null> {
	if (!isWebOS()) return Promise.resolve({mode: 'none', canDefault: true, release: ''});
	return exec(STATUS_COMMAND, 10000).then(parseModeStatus, () => null);
}

/** Starts a switch on the TV (it runs on its own, so the launcher may be closed by it) and waits for its result. */
export function switchHomeMode (mode: Exclude<HomeMode, 'helper'>): Promise<{ok: boolean; why: string}> {
	if (!isWebOS()) return Promise.resolve({ok: true, why: ''});
	return exec(`rm -f '${RESULT_FILE}'; ${runCommand(mode)}`, 10000).then((started) => {
		if (/busy/.test(started)) return {ok: false, why: tr('another switch is still running', 'ещё идёт другое переключение')};
		return new Promise<{ok: boolean; why: string}>((resolve) => {
			let tries = 0;
			const poll = () => {
				tries++;
				exec(`cat '${RESULT_FILE}' 2>/dev/null; true`, 6000).then((out) => {
					const result = parseResult(out);
					if (result) resolve({ok: result.ok, why: result.why});
					else if (tries < 60) window.setTimeout(poll, 1500);
					else resolve({ok: false, why: tr('no answer from the TV', 'ТВ не ответил')});
				}, () => { if (tries < 60) window.setTimeout(poll, 1500); else resolve({ok: false, why: tr('no answer from the TV', 'ТВ не ответил')}); });
			};
			window.setTimeout(poll, 1500);
		});
	}, () => ({ok: false, why: tr('the TV refused the command', 'ТВ отклонил команду')}));
}

/** The webOS release's major number (10 for webOS 25), or 0 when unknown. */
export function releaseMajor (release: string): number {
	const n = parseInt(release, 10);
	return isNaN(n) ? 0 : n;
}

export interface ModeText {
	name: string;
	/** how it works, in one sentence */
	how: string;
	pros: string[];
	cons: string[];
}

export function modeText (mode: HomeMode): ModeText {
	switch (mode) {
		case 'default': return {
			name: tr('Official: the TV opens the launcher', 'Штатно: ТВ сам открывает лаунчер'),
			how: tr('webOS lets you choose which app is Home, as a computer lets you choose the default browser. The launcher is chosen.', 'webOS позволяет выбрать, какое приложение будет домашним, как на компьютере выбирают браузер по умолчанию. Выбирается лаунчер.'),
			pros: [
				tr('The TV\'s own setting: no LG file is replaced or mounted', 'Собственная настройка ТВ: ни один файл LG не подменяется'),
				tr('Works from the moment the TV is switched on, also after a full restart', 'Работает с момента включения, в том числе после полной перезагрузки'),
				tr('LG Home is still there, on its own tile', 'LG Home остаётся доступен со своей плитки')
			],
			cons: [
				tr('Only on newer TVs (webOS 25 and later) that let you choose', 'Только на новых ТВ (webOS 25 и новее), где такой выбор есть'),
				tr('Uninstalled while this is on, the Home button opens nothing until the TV is fully restarted; then LG Home is back by itself', 'Если удалить лаунчер, не выключив этот способ, кнопка Home ничего не откроет до полной перезагрузки ТВ; после неё LG Home вернётся сам')
			]
		};
		case 'mount': return {
			name: tr('Swap: the launcher in LG Home\'s folder', 'Подмена: лаунчер в папке LG Home'),
			how: tr('The TV still thinks it is opening LG Home, but the launcher is put in LG Home\'s folder for it to find.', 'Телевизор по-прежнему думает, что открывает LG Home, но в его папку подложен лаунчер.'),
			pros: [
				tr('Works on TVs that cannot choose a Home app', 'Работает на ТВ, где домашнее приложение выбрать нельзя'),
				tr('The TV treats the launcher exactly like its own Home', 'ТВ обращается с лаунчером точно как со своим Home')
			],
			cons: [
				tr('Puts a folder and a permissions file over LG\'s own (in memory only: gone after a restart)', 'Накрывает папку и файл прав LG своими (только в памяти: исчезает после перезагрузки)'),
				tr('After a full restart LG Home shows for about 40 seconds', 'После полной перезагрузки около 40 секунд виден LG Home'),
				tr('LG Home itself cannot be opened while it is on', 'Сам LG Home открыть нельзя, пока это включено')
			]
		};
		case 'helper': return {
			name: tr('Helper: catches LG Home and switches', 'Помощник: ловит LG Home и переключает'),
			how: tr('A small program on the TV notices when LG Home comes up and opens the launcher over it at once.', 'Небольшая программа на ТВ замечает, что открылся LG Home, и сразу открывает поверх него лаунчер.'),
			pros: [
				tr('Works on almost any rooted TV', 'Работает почти на любом рутованном ТВ'),
				tr('Nothing of LG\'s is replaced', 'Ничего от LG не подменяется')
			],
			cons: [
				tr('LG Home can flash for a moment', 'LG Home может мелькнуть на мгновение'),
				tr('A program keeps running in the background', 'В фоне всё время работает программа')
			]
		};
		default: return {
			name: tr('Off: Home opens LG Home', 'Выключено: Home открывает LG Home'),
			how: tr('The launcher is an ordinary app and opens from its own tile.', 'Лаунчер — обычное приложение и открывается со своей плитки.'),
			pros: [tr('Nothing runs and nothing is changed on the TV', 'Ничего не работает в фоне и ничего не меняется на ТВ')],
			cons: [tr('The Home button and waking up show LG Home', 'Кнопка Home и включение ТВ показывают LG Home')]
		};
	}
}

/** Why a way cannot be chosen on this TV, or a caution about it ('' when there is none). */
export function modeNote (mode: HomeMode, info: HomeModeInfo | null): {blocked: boolean; text: string} {
	if (mode === 'default' && info && !info.canDefault) return {blocked: true, text: tr('This TV does not let you choose a Home app', 'Этот ТВ не даёт выбрать домашнее приложение')};
	if (mode === 'mount' && info && releaseMajor(info.release) !== 10) return {blocked: false, text: tr('Tried only on webOS 25 so far', 'Пока проверено только на webOS 25')};
	return {blocked: false, text: ''};
}
