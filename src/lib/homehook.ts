// "Open this launcher instead of LG Home": a small background watcher opens the launcher whenever the
// TV brings LG Home to the front. The launcher writes the scripts itself, so there is nothing to copy
// by hand. Nothing is replaced. See homehookcmd.ts for the commands and src/homehook for the scripts.

import applySh from '../homehook/apply.sh?raw';
import diagnoseSh from '../homehook/diagnose.sh?raw';
import homekeyPy from '../homehook/homekey.py?raw';
import stockSh from '../homehook/stock.sh?raw';
import watchSh from '../homehook/watch.sh?raw';
import {exec} from './apps';
import {classifyFailure, failureReason} from './failure';
import {isWebOS} from './luna';
import {
	ALIVE_FILE, APPLY_COMMAND, HOOK_VERSION, installSteps, LINK_OFF_COMMAND, LINK_ON_COMMAND, lastLine, parseExit, parseHookResult,
	parseHookStatus, REMOVE_COMMAND, STATUS_COMMAND, STOCK_COMMAND, VERSION_COMMAND, withStatus,
	type HookResult, type HookStatus
} from './homehookcmd';
import {tr} from './i18n';

export type {HookResult, HookStatus};

const FILES = {watchSh, applySh, stockSh, diagnoseSh, homekeyPy};

export function getHookStatus (): Promise<HookStatus | null> {
	if (!isWebOS()) return Promise.resolve(null);
	return exec(STATUS_COMMAND, 6000).then(parseHookStatus).catch(() => null);
}

export interface HookOutcome {
	result: HookResult;
	/** for a failure: which step, and what the TV said */
	detail: string;
}

/** Writes the scripts, sets the boot link as asked, and starts the watcher, one small step at a time.
 *  If it can't be done everything it added is removed again, and the outcome says why. */
export function installHomeHook (boot: boolean): Promise<HookOutcome> {
	if (!isWebOS()) return Promise.resolve({result: 'failed', detail: tr('not on a TV', 'не на ТВ')});
	const steps = installSteps(FILES, boot);
	const run = (i: number): Promise<HookOutcome> => {
		const step = steps[i];
		return exec(withStatus(step.command), 15000).then(
			(output): HookOutcome | Promise<HookOutcome> => {
				if (i === steps.length - 1) return {result: parseHookResult(output), detail: lastLine(output)};
				const code = parseExit(output);
				if (code !== 0) return {result: 'failed', detail: `${step.name}: ${lastLine(output) || 'exit ' + code}`};
				return run(i + 1);
			},
			(error: Error): HookOutcome => ({result: 'failed', detail: `${step.name}: ${failureReason(classifyFailure(error), error && error.message ? error.message : '')}`})
		);
	};
	return run(0).then((outcome) => outcome.result === 'applied' ? outcome : removeHomeHook().then(() => outcome));
}

export interface Reapplied extends HookOutcome {
	/** it can't work on this TV (or was removed): turn the setting off */
	fatal: boolean;
	/** the Homebrew service did not answer (it is often not up yet just after the TV starts): try again later, change nothing */
	unreachable?: boolean;
}

/** Makes sure the watcher is running (after a reboot, or when the launcher opens). If the copy on the TV
 *  is from an older version of the launcher, it is rewritten first, so updating the launcher updates it. */
export function reapplyHomeHook (boot: boolean): Promise<Reapplied> {
	if (!isWebOS()) return Promise.resolve({result: 'failed', detail: tr('not on a TV', 'не на ТВ'), fatal: false});
	return exec(VERSION_COMMAND, 6000).then((out) => out.trim(), () => null).then((installed): Reapplied | Promise<Reapplied> => {
		if (installed === null) return {result: 'failed', detail: tr('the Homebrew service did not answer', 'служба Homebrew не ответила'), fatal: false, unreachable: true};
		if (installed !== HOOK_VERSION) {
			return installHomeHook(boot).then((outcome): Reapplied => ({...outcome, fatal: outcome.result !== 'applied'}));
		}
		return applyHomeHook().then((result): Reapplied => ({result, detail: '', fatal: result === 'launcher-missing' || result === 'files-missing'}));
	});
}

/** Starts the watcher if it isn't running. */
export function applyHomeHook (): Promise<HookResult> {
	if (!isWebOS()) return Promise.resolve('failed');
	return exec(APPLY_COMMAND, 10000).then(parseHookResult).catch((): HookResult => 'failed');
}

/** Tells the watcher that LG's own Home is wanted, so it can be opened. */
export function stockHome (): Promise<void> {
	if (!isWebOS()) return Promise.resolve();
	return exec(STOCK_COMMAND, 10000).then(() => undefined).catch(() => undefined);
}

/** Adds or removes the start-at-boot link on its own. Resolves true when it worked. */
export function setBootLink (on: boolean): Promise<boolean> {
	if (!isWebOS()) return Promise.resolve(false);
	return exec(on ? LINK_ON_COMMAND : LINK_OFF_COMMAND, 8000).then((out) => parseExit(out) === 0 && /(^|\n)(linked|unlinked)\s/.test(out)).catch(() => false);
}

/** Stops the watcher and deletes everything the setting added. */
export function removeHomeHook (): Promise<void> {
	if (!isWebOS()) return Promise.resolve();
	return exec(REMOVE_COMMAND, 10000).then(() => undefined).catch(() => undefined);
}

const LOG_FILE = '/tmp/launcher-homehook.log';

function cleanForLog (text: string): string {
	return text.replace(/[^A-Za-z0-9 :._,-]/g, '_');
}

/** Adds a line to the helper's log, e.g. how long the launcher took to show its first screen. */
export function logLine (text: string): void {
	if (!isWebOS()) return;
	exec(`echo "$(date '+%H:%M:%S') ${cleanForLog(text)}" >> ${LOG_FILE}`, 4000).catch(() => undefined);
}

/** Tells the helper this launcher is on screen (it checks for this after opening the launcher). */
export function markAlive (): void {
	if (!isWebOS()) return;
	exec(`touch '${ALIVE_FILE}'`, 4000).catch(() => undefined);
}

/** markAlive, with an optional line for the log, in ONE call to the TV (each call starts a shell, and the TV is
 *  busiest just as the launcher comes up). With `repeat`, the TV itself touches the file again 1 and 2 seconds
 *  later. The helper starts its check when the TV reports the launcher in front, and that report can arrive just
 *  after the launcher became visible; the later touches make sure the check finds one newer than its start. */
export function markAliveSoon (note?: string, repeat: boolean = true): void {
	if (!isWebOS()) return;
	const parts = [`touch '${ALIVE_FILE}'`];
	if (note) parts.push(`echo "$(date '+%H:%M:%S') ${cleanForLog(note)}" >> ${LOG_FILE}`);
	if (repeat) parts.push(`( nohup sh -c "sleep 1; touch '${ALIVE_FILE}'; sleep 1; touch '${ALIVE_FILE}'" >/dev/null 2>&1 </dev/null & )`);
	exec(parts.join('; '), 4000).catch(() => undefined);
}

export function hookMessage (result: HookResult, detail = ''): string {
	switch (result) {
		case 'applied': return tr('On: LG Home now hands over to the launcher', 'Включено: LG Home теперь передаёт управление лаунчеру');
		case 'launcher-missing': return tr('The launcher has to be installed for this', 'Для этого лаунчер должен быть установлен');
		case 'files-missing': return tr('The helper files were not found; nothing was changed', 'Файлы помощника не найдены, ничего не изменено');
		default: return tr('Could not set it up', 'Не удалось настроить') + (detail ? ': ' + detail : '') + tr('. Nothing was changed', '. Ничего не изменено');
	}
}
