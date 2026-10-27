import {useCallback, useState} from 'preact/hooks';
import {Grid} from '../components/Grid';
import {OptionList, type OptionItem} from '../components/OptionList';
import type {TileModel} from '../components/Tile';
import {useKeys, clamp, type NavKey} from '../hooks/useKeys';
import {useOkPress} from '../hooks/useOkPress';
import {moveItem} from '../lib/order';
import {LAYOUT} from '../theme/tokens';
import {tr} from '../lib/i18n';

export type SectionIcon = 'apps' | 'inputs' | 'look' | 'clock' | 'weather' | 'calendar' | 'wallpaper' | 'home' | 'general';

export interface SettingsTab {
	id: string;
	icon: SectionIcon;
	label: string;
	/** the re-order button (grid sections) */
	arrange: string;
	/** everything available */
	items: TileModel[];
	/** what's on the home screen, in home-screen order */
	shown: TileModel[];
	/** a "Refresh list" button next to the re-order button */
	refresh?: boolean;
	/** present on option sections (a list of settings instead of a grid of tiles) */
	options?: OptionItem[];
}

interface Props {
	tabs: SettingsTab[];
	active: boolean;
	onToggle: (tabId: string, key: string) => void;
	onReorder: (tabId: string, keys: string[]) => void;
	onOption: (optionId: string) => void;
	/** the "Refresh list" button of the Apps section */
	onRefresh?: () => void;
	/** ← or → on a stepper option: -1 or +1 */
	onAdjust: (optionId: string, direction: -1 | 1) => void;
	onClose: () => void;
}

/** Focus is in the list of sections, on the buttons above a grid, or in the section's content. */
type Zone = 'nav' | 'buttons' | 'content';

/** A tile that has been picked up in re-order mode, and the order to restore on Back. */
interface Held {
	index: number;
	original: string[];
}

/** The pictures of the sections (24 x 24, drawn as lines), each on its own colour like the iOS settings. */
const ICONS: Record<SectionIcon, {color: string; path: string}> = {
	apps: {color: '#0A84FF', path: 'M4.5 4.5h5v5h-5z M14.5 4.5h5v5h-5z M4.5 14.5h5v5h-5z M14.5 14.5h5v5h-5z'},
	inputs: {color: '#30B0C7', path: 'M4 8h16v5l-3 3H7l-3-3z M8 11.5h8 M12 16v4'},
	look: {color: '#5E5CE6', path: 'M12 3.5a8.5 8.5 0 1 0 0 17c1 0 1.6-.7 1.6-1.5s-.6-1.3-.6-2.1c0-.9.7-1.6 1.6-1.6h2a3.9 3.9 0 0 0 3.9-3.9c0-4.4-3.8-7.9-8.5-7.9z M7.8 11.5h.01 M10.2 7.6h.01 M14.6 7.8h.01'},
	clock: {color: '#FF9F0A', path: 'M12 3.5a8.5 8.5 0 1 0 0 17a8.5 8.5 0 1 0 0-17z M12 7.5v4.8l3.2 2'},
	weather: {color: '#32ADE6', path: 'M7.5 18.5h9.5a3.8 3.8 0 0 0 .3-7.6 5.2 5.2 0 0 0-10 1.2 3.2 3.2 0 0 0 .2 6.4z'},
	calendar: {color: '#FF453A', path: 'M4.5 6h15v13.5h-15z M4.5 10h15 M8.5 3.5v4 M15.5 3.5v4'},
	wallpaper: {color: '#BF5AF2', path: 'M4 5h16v14H4z M4 16l4.5-4.5 4 4 3-3 4.5 4.5 M15.5 9h.01'},
	home: {color: '#30D158', path: 'M4.5 11l7.5-6.5 7.5 6.5v8.5h-15z M10 19.5v-5h4v5'},
	general: {color: '#8E8E93', path: 'M12 9a3 3 0 1 0 0 6a3 3 0 1 0 0-6z M12 2.8v2.6 M12 18.6v2.6 M2.8 12h2.6 M18.6 12h2.6 M5.5 5.5l1.8 1.8 M16.7 16.7l1.8 1.8 M5.5 18.5l1.8-1.8 M16.7 7.3l1.8-1.8'}
};

const COLUMNS = LAYOUT.gridColumns;

/** Settings: the sections on the left, the chosen one on the right.
 *
 *  ↑ ↓ pick a section, → or OK go into it, Back comes out again (and, from the list of sections, closes Settings).
 *  Option sections: a list of settings in groups; OK changes one, ← → nudge numbers and choices.
 *  Apps and Inputs: a grid of tiles; OK shows or hides one. Re-order mode (hold OK, or the Arrange button) shows only
 *  what's on the home screen, in order: OK picks an item up, arrows move it, OK puts it down. */
export function Settings ({tabs, active, onToggle, onReorder, onOption, onRefresh, onAdjust, onClose}: Props) {
	const [tabIndex, setTabIndex] = useState(0);
	const [zone, setZone] = useState<Zone>('nav');
	const [buttonIndex, setButtonIndex] = useState(0);
	const [gridIndex, setGridIndex] = useState(0);
	const [optionIndex, setOptionIndex] = useState(0);
	const [reordering, setReordering] = useState(false);
	const [held, setHeld] = useState<Held | null>(null);

	const current: SettingsTab | undefined = tabs[tabIndex];
	const options: OptionItem[] = (current && current.options) || [];
	const onOptions = !!(current && current.options);
	const tab: SettingsTab | undefined = onOptions ? undefined : current;     // grid section, if any
	const buttons: ('refresh' | 'arrange')[] = !tab ? [] : reordering ? ['arrange'] : (tab.refresh && onRefresh ? ['refresh', 'arrange'] : ['arrange']);
	const items = tab ? (reordering ? tab.shown : tab.items) : [];
	const count = onOptions ? options.length : items.length;
	const optionAt = clamp(optionIndex, 0, Math.max(0, options.length - 1));      // a row may have gone (rows show only when they apply)
	const keysOf = (list: TileModel[]) => list.map((t) => t.key);

	const selectTab = (i: number) => {
		if (reordering) return;          // the section is locked while re-ordering
		setTabIndex(i);
		setGridIndex(0);
		setOptionIndex(0);
		setButtonIndex(0);
	};

	/** Switch re-order mode, keeping focus on the same item where it exists in both lists. */
	const setMode = (next: boolean) => {
		if (!tab || next === reordering) return;
		const focusedKey = items[gridIndex] && items[gridIndex].key;
		const target = next ? tab.shown : tab.items;
		const i = focusedKey ? keysOf(target).indexOf(focusedKey) : -1;
		setGridIndex(Math.max(0, i));
		setHeld(null);
		setReordering(next);
		setButtonIndex(0);
		if (next && target.length === 0) setZone('buttons');
	};

	/** Move the held tile to `to`, updating the home screen as it goes. */
	const moveHeld = (to: number) => {
		if (!tab || !held) return;
		const dest = clamp(to, 0, count - 1);
		if (dest === held.index) return;
		onReorder(tab.id, moveItem(keysOf(tab.shown), held.index, dest));
		setHeld({...held, index: dest});
		setGridIndex(dest);
	};

	const activateGridItem = (i: number) => {
		if (!tab || !items[i]) return;
		if (!reordering) { onToggle(tab.id, items[i].key); return; }
		if (held) {                                                   // put down (pointer: at the clicked tile)
			if (i !== held.index) moveHeld(i);
			setHeld(null);
			return;
		}
		setHeld({index: i, original: keysOf(tab.shown)});             // pick up
		setGridIndex(i);
	};

	const pressButton = (b: 'refresh' | 'arrange') => {
		if (b === 'refresh') { if (onRefresh) onRefresh(); }
		else setMode(!reordering);
	};

	const enter = () => {
		if (count > 0) setZone('content');
		else if (buttons.length) setZone('buttons');
	};

	const onPress = () => {
		if (zone === 'nav') { enter(); return; }
		if (zone === 'buttons') { if (buttons[buttonIndex]) pressButton(buttons[buttonIndex]); return; }
		if (onOptions) { if (options[optionAt]) onOption(options[optionAt].id); return; }
		activateGridItem(gridIndex);
	};

	const onLongPress = () => { if (tab && zone !== 'nav') setMode(!reordering); };

	useOkPress({onPress, onLongPress}, active);

	const handleKey = useCallback((key: NavKey): boolean => {
		if (key === 'enter') return true;           // handled by useOkPress (press vs hold)

		if (key === 'back') {
			if (held && tab) { onReorder(tab.id, held.original); setGridIndex(held.index); setHeld(null); }
			else if (reordering) setMode(false);
			else if (zone !== 'nav') setZone('nav');
			else onClose();
			return true;
		}

		if (held) {
			if (key === 'left') moveHeld(held.index - 1);
			else if (key === 'right') moveHeld(held.index + 1);
			else if (key === 'up') moveHeld(held.index - COLUMNS);
			else if (key === 'down') moveHeld(held.index + COLUMNS);
			return true;
		}

		if (zone === 'nav') {
			if (key === 'up') selectTab(clamp(tabIndex - 1, 0, tabs.length - 1));
			else if (key === 'down') selectTab(clamp(tabIndex + 1, 0, tabs.length - 1));
			else if (key === 'right') enter();
			return true;
		}

		if (zone === 'buttons') {
			if (key === 'left') { if (buttonIndex > 0) setButtonIndex(buttonIndex - 1); else if (!reordering) setZone('nav'); }
			else if (key === 'right') setButtonIndex(clamp(buttonIndex + 1, 0, buttons.length - 1));
			else if (key === 'down' && count > 0) setZone('content');
			return true;
		}

		if (onOptions) {
			const option = options[optionAt];
			if (key === 'up') setOptionIndex(clamp(optionAt - 1, 0, count - 1));
			else if (key === 'down') setOptionIndex(clamp(optionAt + 1, 0, count - 1));
			else if ((key === 'left' || key === 'right') && option && option.stepper && !option.disabled) onAdjust(option.id, key === 'left' ? -1 : 1);
			else if (key === 'left') setZone('nav');
			return true;
		}

		switch (key) {
			case 'left':
				if (gridIndex % COLUMNS === 0) { if (!reordering) setZone('nav'); }
				else setGridIndex(gridIndex - 1);
				break;
			case 'right': setGridIndex(clamp(gridIndex + 1, 0, count - 1)); break;
			case 'up':
				if (gridIndex >= COLUMNS) setGridIndex(gridIndex - COLUMNS);
				else if (buttons.length) setZone('buttons');
				break;
			case 'down': if (gridIndex + COLUMNS < count) setGridIndex(gridIndex + COLUMNS); break;
		}
		return true;
	}, [zone, tabIndex, tabs.length, gridIndex, optionAt, buttonIndex, count, tab, onOptions, options, buttons.length, reordering, held, items, onReorder, onAdjust, onClose]);

	useKeys(handleKey, active);

	const hint = zone === 'nav' ? tr('↑ ↓ choose · → open · Back closes Settings', '↑ ↓ выбор · → открыть · «Назад» закрывает настройки')
		: onOptions ? tr('OK changes · ← → adjust · Back returns to the list', 'OK меняет · ← → подстраивают · «Назад» к списку')
		: held ? tr('Arrows move it · OK puts it down · Back cancels', 'Стрелки двигают · OK кладёт на место · «Назад» отменяет')
		: reordering ? tr('OK picks a tile up · hold OK or choose Done to finish', 'OK берёт плитку · удерживайте OK или выберите «Готово»')
		: tr('OK shows or hides · hold OK to re-order', 'OK показывает или скрывает · удерживайте OK для порядка');
	const focusedOption = zone === 'content' && onOptions ? options[optionAt] : undefined;
	const footer = focusedOption ? (focusedOption.disabled || focusedOption.description) : '';

	return (
		<div class="screen screen--settings">
			<div class="set">
				<nav class="set__nav">
					<h1 class="set__heading">{tr('Settings', 'Настройки')}</h1>
					{tabs.map((t, i) => {
						const icon = ICONS[t.icon];
						return (
							<div
								key={t.id}
								class={['navitem', i === tabIndex && 'navitem--current', zone === 'nav' && i === tabIndex && 'navitem--focused', reordering && i !== tabIndex && 'navitem--locked'].filter(Boolean).join(' ')}
								onMouseEnter={() => { if (!reordering) { setZone('nav'); selectTab(i); } }}
								onClick={enter}
							>
								<span class="navitem__icon" style={{background: icon.color}}>
									<svg viewBox="0 0 24 24"><path d={icon.path} /></svg>
								</span>
								<span class="navitem__label">{t.label}</span>
							</div>
						);
					})}
				</nav>
				<section class="set__main">
					<h2 class="set__title">{current ? current.label : ''}</h2>
					<p class="set__hint">{hint}</p>
					{tab && (
						<div class="set__buttons">
							{buttons.map((b, i) => (
								<div
									key={b}
									class={['pill', b === 'arrange' && reordering && 'pill--active', zone === 'buttons' && i === buttonIndex && 'pill--focused'].filter(Boolean).join(' ')}
									onMouseEnter={() => { setZone('buttons'); setButtonIndex(i); }}
									onClick={() => pressButton(b)}
								>
									{b === 'refresh' ? tr('Refresh list', 'Обновить список') : reordering ? tr('Done', 'Готово') : tab.arrange}
								</div>
							))}
						</div>
					)}
					{tab && (
						<Grid
							items={items}
							mode={reordering ? 'reorder' : 'select'}
							checkedKeys={keysOf(tab.shown)}
							liftedIndex={held ? held.index : -1}
							focused={zone === 'content'}
							selected={gridIndex}
							onSelect={(i) => { if (!held) { setZone('content'); setGridIndex(i); } }}
							onActivate={(i) => { setZone('content'); activateGridItem(i); }}
						/>
					)}
					{tab && reordering && count === 0 && <p class="set__empty">{tr('Nothing on the home screen yet.', 'На главном экране пока ничего нет.')}</p>}
					{onOptions && (
						<OptionList
							options={options}
							focused={zone === 'content'}
							selected={optionAt}
							onSelect={(i) => { setZone('content'); setOptionIndex(i); }}
							onActivate={(i) => { setZone('content'); setOptionIndex(i); onOption(options[i].id); }}
						/>
					)}
					<p class={`set__footer${focusedOption && focusedOption.disabled ? ' set__footer--warn' : ''}`}>{footer}</p>
				</section>
			</div>
		</div>
	);
}
