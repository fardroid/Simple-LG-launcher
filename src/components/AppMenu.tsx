import {useCallback, useState} from 'preact/hooks';
import {useKeys, clamp, type NavKey} from '../hooks/useKeys';
import {useOkPress} from '../hooks/useOkPress';
import type {MoveAction} from '../lib/tileorder';
import {tr} from '../lib/i18n';

interface Props {
	title: string;
	/** which moves are possible from the tile's place on the row */
	moves: {start: boolean; left: boolean; right: boolean; end: boolean};
	/** where to get a hidden tile back (differs for apps and inputs) */
	hideDetail: string;
	/** apps have an Uninstall entry; inputs don't */
	showUninstall: boolean;
	/** why Uninstall can't be used, or null when it can */
	blockedReason: string | null;
	onMove: (action: MoveAction) => void;
	onHide: () => void;
	onUninstall: () => void;
	onClose: () => void;
}

type Step = 'menu' | 'confirm' | 'working';

interface MenuRow {
	key: string;
	label: string;
	detail?: string;
	disabled?: boolean;
	run: () => void;
}

/** What opens when OK is held on an app or input: move it along the row, hide it, or (apps) uninstall it.
 *  Uninstall asks for confirmation, and the confirmation starts on Cancel. */
export function AppMenu ({title, moves, hideDetail, showUninstall, blockedReason, onMove, onHide, onUninstall, onClose}: Props) {
	const [step, setStep] = useState<Step>('menu');

	const menuRows = (): MenuRow[] => {
		const rows: MenuRow[] = [
			{key: 'start', label: tr('Move to start', 'В начало'), detail: moves.start ? undefined : tr('Already first', 'Уже первое'), disabled: !moves.start, run: () => onMove('start')},
			{key: 'left', label: tr('Move left', 'Сдвинуть влево'), detail: moves.left ? undefined : tr('Already first', 'Уже первое'), disabled: !moves.left, run: () => onMove('left')},
			{key: 'right', label: tr('Move right', 'Сдвинуть вправо'), detail: moves.right ? undefined : tr('Already last', 'Уже последнее'), disabled: !moves.right, run: () => onMove('right')},
			{key: 'end', label: tr('Move to end', 'В конец'), detail: moves.end ? undefined : tr('Already last', 'Уже последнее'), disabled: !moves.end, run: () => onMove('end')},
			{key: 'hide', label: tr('Hide from the home screen', 'Скрыть с главного экрана'), detail: hideDetail, run: onHide}
		];
		if (showUninstall) {
			rows.push({
				key: 'uninstall', label: tr('Uninstall', 'Удалить'), detail: blockedReason || tr('Removes the app from the TV', 'Удаляет приложение с ТВ'), disabled: !!blockedReason,
				run: () => { setStep('confirm'); setIndex(0); }
			});
		}
		rows.push({key: 'cancel', label: tr('Cancel', 'Отмена'), run: onClose});
		return rows;
	};
	const firstEnabled = (rows: MenuRow[]): number => {
		const i = rows.findIndex((r) => !r.disabled);
		return i === -1 ? 0 : i;
	};

	const [index, setIndex] = useState(() => firstEnabled(menuRows()));
	const rows: MenuRow[] = step === 'menu' ? menuRows() : step === 'confirm' ? [
		{key: 'cancel', label: tr('Cancel', 'Отмена'), run: onClose},
		{key: 'yes', label: tr('Uninstall ', 'Удалить ') + title, detail: tr('This cannot be undone', 'Это нельзя отменить'), run: () => { setStep('working'); onUninstall(); }}
	] : [];

	const handleKey = useCallback((key: NavKey): boolean => {
		if (step === 'working') return true;
		switch (key) {
			case 'up': setIndex((i) => clamp(i - 1, 0, rows.length - 1)); break;
			case 'down': setIndex((i) => clamp(i + 1, 0, rows.length - 1)); break;
			case 'back':
				if (step === 'confirm') { setStep('menu'); setIndex(firstEnabled(menuRows())); } else onClose();
				break;
			default: break;
		}
		return true;
	}, [step, rows.length, moves.start, moves.left, moves.right, moves.end, blockedReason, onClose]);

	useKeys(handleKey, true);
	useOkPress({
		onPress: () => { const row = rows[index]; if (row && !row.disabled) row.run(); },
		onLongPress: () => undefined
	}, step !== 'working');

	return (
		<div class="screen screen--dialog">
			<div class="dialog">
				<h1 class="dialog__title">{step === 'confirm' ? tr(`Uninstall ${title}?`, `Удалить ${title}?`) : step === 'working' ? tr(`Uninstalling ${title}…`, `Удаляется ${title}…`) : title}</h1>
				{step === 'confirm' && <p class="dialog__text">{tr('The app will be removed from the TV.', 'Приложение будет удалено с ТВ.')}</p>}
				{step === 'working' && <p class="dialog__text">{tr('Please wait, this can take a few seconds.', 'Подождите, это может занять несколько секунд.')}</p>}
				{rows.map((row, i) => (
					<div
						key={row.key}
						class={`picker__row${i === index ? ' picker__row--focused' : ''}${row.disabled ? ' picker__row--disabled' : ''}`}
						onMouseEnter={() => setIndex(i)}
						onClick={() => { setIndex(i); if (!row.disabled) row.run(); }}
					>
						<div class="picker__text">
							<div class="picker__label">{row.label}</div>
							{row.detail && <div class="picker__detail">{row.detail}</div>}
						</div>
					</div>
				))}
			</div>
		</div>
	);
}
