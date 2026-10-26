import {useCallback, useEffect, useRef, useState} from 'preact/hooks';
import {Grid} from '../components/Grid';
import {OptionList, type OptionItem} from '../components/OptionList';
import type {TileModel} from '../components/Tile';
import {useKeys, clamp, type NavKey} from '../hooks/useKeys';
import {useOkPress} from '../hooks/useOkPress';
import {moveItem} from '../lib/order';
import {LAYOUT} from '../theme/tokens';
import {tr} from '../lib/i18n';

export interface SettingsTab {
	id: string;
	label: string;
	/** the re-order button, e.g. "Arrange apps" (grid tabs) */
	arrange: string;
	/** everything available */
	items: TileModel[];
	/** what's on the home screen, in home-screen order */
	shown: TileModel[];
	/** a "Refresh list" button next to the re-order button */
	refresh?: boolean;
	/** present on option tabs (a list of settings instead of a grid of tiles) */
	options?: OptionItem[];
}

interface Props {
	tabs: SettingsTab[];
	active: boolean;
	onToggle: (tabId: string, key: string) => void;
	onReorder: (tabId: string, keys: string[]) => void;
	onOption: (optionId: string) => void;
	/** the "Refresh list" button on the Apps tab */
	onRefresh?: () => void;
	/** ← or → on a stepper option: -1 or +1 */
	onAdjust: (optionId: string, direction: -1 | 1) => void;
	onClose: () => void;
}

/** Focus is on the tabs row (a tab, or the re-order button) or in the tab's content. */
type Zone = 'tabs' | 'refresh' | 'button' | 'content';

/** A tile that has been picked up in re-order mode, and the order to restore on Back. */
interface Held {
	index: number;
	original: string[];
}

const hints = () => ({
	select: tr('OK adds or removes · Hold OK to re-order · Back returns home', 'OK добавляет или убирает · Удерживайте OK для порядка · Назад — на главный экран'),
	reorder: tr('OK picks an item up · Hold OK or choose Done to finish', 'OK берёт элемент · Удерживайте OK или выберите «Готово», чтобы закончить'),
	holding: tr('Arrows move it · OK puts it down · Back cancels', 'Стрелки двигают · OK кладёт на место · Назад отменяет'),
	options: tr('OK changes a setting · ← → change numbers · Back returns home', 'OK меняет настройку · ← → меняют числа · Назад — на главный экран')
});

/** Settings: pick which apps and sources appear on the home row and their order, plus the
 *  wallpaper and other options.
 *
 *  Tabs: grid tabs (Apps, Sources) and option tabs (Look, Clock, Wallpaper, Options).
 *  Grid tabs, select mode: everything available; OK adds/removes an item.
 *  Grid tabs, re-order mode (hold OK, or the button): only what's on the home screen, in order;
 *  OK picks an item up, arrows move it, OK puts it down. Hold OK or press Done to leave. */
export function Settings ({tabs, active, onToggle, onReorder, onOption, onRefresh, onAdjust, onClose}: Props) {
	const [tabIndex, setTabIndex] = useState(0);
	const [zone, setZone] = useState<Zone>('tabs');
	const scrollRef = useRef<HTMLDivElement>(null);
	const trackRef = useRef<HTMLDivElement>(null);
	const [tabScroll, setTabScroll] = useState({shift: 0, left: false, right: false});
	const [gridIndex, setGridIndex] = useState(0);
	const [optionIndex, setOptionIndex] = useState(0);
	const [reordering, setReordering] = useState(false);
	const [held, setHeld] = useState<Held | null>(null);

	const tabCount = tabs.length;
	const current: SettingsTab | undefined = tabs[tabIndex];
	const options: OptionItem[] = (current && current.options) || [];
	const onOptions = !!(current && current.options);
	const tab: SettingsTab | undefined = onOptions ? undefined : current;     // grid tab, if any
	const hasRefresh = !!(tab && tab.refresh && onRefresh);
	const items = tab ? (reordering ? tab.shown : tab.items) : [];
	const count = onOptions ? options.length : items.length;
	const cols = LAYOUT.gridColumns;
	const keysOf = (list: TileModel[]) => list.map((t) => t.key);

	const selectTab = (i: number) => {
		if (reordering) return;          // tab is locked while re-ordering
		setTabIndex(i);
		setGridIndex(0);
		setOptionIndex(0);
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
		if (next && (zone === 'tabs' || target.length === 0)) setZone('button');
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

	const onPress = () => {
		if (zone === 'refresh') { if (onRefresh) onRefresh(); return; }
		if (zone === 'button') { setMode(!reordering); return; }
		if (zone === 'tabs') { if (count > 0) setZone('content'); return; }
		if (onOptions) { if (options[optionIndex]) onOption(options[optionIndex].id); return; }
		activateGridItem(gridIndex);
	};

	const onLongPress = () => { if (tab) setMode(!reordering); };

	useOkPress({onPress, onLongPress}, active);

	const handleKey = useCallback((key: NavKey): boolean => {
		if (key === 'enter') return true;           // handled by useOkPress (press vs hold)

		if (key === 'back') {
			if (held && tab) { onReorder(tab.id, held.original); setGridIndex(held.index); setHeld(null); }
			else if (reordering) setMode(false);
			else onClose();
			return true;
		}

		if (held) {
			if (key === 'left') moveHeld(held.index - 1);
			else if (key === 'right') moveHeld(held.index + 1);
			else if (key === 'up') moveHeld(held.index - cols);
			else if (key === 'down') moveHeld(held.index + cols);
			return true;
		}

		if (zone === 'tabs') {
			if (key === 'left') selectTab(clamp(tabIndex - 1, 0, tabCount - 1));
			else if (key === 'right') selectTab(clamp(tabIndex + 1, 0, tabCount - 1));
			else if (key === 'down' && count > 0) setZone('content');
			return true;
		}
		if (zone === 'refresh') {
			if (key === 'left') setZone('tabs');
			else if (key === 'right') setZone('button');
			else if (key === 'down' && count > 0) setZone('content');
			return true;
		}
		if (zone === 'button') {
			if (key === 'left' && !reordering) setZone(hasRefresh ? 'refresh' : 'tabs');
			else if (key === 'down' && count > 0) setZone('content');
			return true;
		}

		if (onOptions) {
			if (key === 'up') { if (optionIndex === 0) setZone('tabs'); else setOptionIndex(optionIndex - 1); }
			else if (key === 'down') setOptionIndex(clamp(optionIndex + 1, 0, count - 1));
			else if ((key === 'left' || key === 'right') && options[optionIndex] && options[optionIndex].stepper) {
				onAdjust(options[optionIndex].id, key === 'left' ? -1 : 1);
			}
			return true;
		}

		switch (key) {
			case 'left': setGridIndex((i) => clamp(i - 1, 0, count - 1)); break;
			case 'right': setGridIndex((i) => clamp(i + 1, 0, count - 1)); break;
			case 'up':
				if (gridIndex >= cols) setGridIndex((i) => i - cols);
				// Spatial: the button sits top-right, so leave the grid towards whichever is above.
				else setZone(reordering || gridIndex % cols >= cols / 2 ? 'button' : 'tabs');
				break;
			case 'down': setGridIndex((i) => (i + cols < count ? i + cols : i)); break;
		}
		return true;
	}, [zone, tabIndex, tabCount, gridIndex, optionIndex, count, tab, onOptions, options, reordering, held, items, hasRefresh, onReorder, onAdjust, onClose]);

	useKeys(handleKey, active);

	// The row of tabs is wider than the screen: it slides to keep the chosen tab in the middle, and the buttons beside it stay put.
	useEffect(() => {
		const scroll = scrollRef.current, track = trackRef.current;
		if (!scroll || !track || !track.children[tabIndex]) return;
		const el = track.children[tabIndex] as HTMLElement;
		const room = Math.max(0, track.scrollWidth - scroll.clientWidth);
		const shift = clamp(el.offsetLeft + el.offsetWidth / 2 - scroll.clientWidth / 2, 0, room);
		setTabScroll({shift, left: shift > 1, right: shift < room - 1});
	}, [tabIndex, tabCount, hasRefresh, reordering]);

	const HINT = hints();
	const hint = onOptions ? HINT.options : held ? HINT.holding : reordering ? HINT.reorder : HINT.select;
	const labels = tabs.map((t) => t.label);
	const buttonClass = ['settings__button', reordering && 'settings__button--active', zone === 'button' && 'settings__button--focused'].filter(Boolean).join(' ');

	return (
		<div class="screen screen--settings">
			<div class="settings">
				<h1 class="settings__title">{tr('Settings', 'Настройки')}</h1>
				<p class="settings__hint">{hint}</p>
				<div class="settings__tabs">
					<div class={`tabs__scroll${tabScroll.left ? ' tabs__scroll--left' : ''}${tabScroll.right ? ' tabs__scroll--right' : ''}`} ref={scrollRef}>
					<div class="tabs__track" ref={trackRef} style={{transform: `translateX(${-tabScroll.shift}px)`}}>
					{labels.map((label, i) => (
						<div
							key={label}
							class={['tab', i === tabIndex && 'tab--active', reordering && i !== tabIndex && 'tab--locked', zone === 'tabs' && i === tabIndex && 'tab--focused'].filter(Boolean).join(' ')}
							onMouseEnter={() => { if (!reordering) { setZone('tabs'); selectTab(i); } }}
						>
							{label}
						</div>
					))}
					</div>
					</div>
					{hasRefresh && !reordering && (
						<div
							class={['settings__button', 'settings__button--refresh', zone === 'refresh' && 'settings__button--focused'].join(' ')}
							onMouseEnter={() => setZone('refresh')}
							onClick={() => { if (onRefresh) onRefresh(); }}
						>
							{tr('Refresh list', 'Обновить список')}
						</div>
					)}
					{tab && (
						<div
							class={buttonClass}
							onMouseEnter={() => setZone('button')}
							onClick={() => setMode(!reordering)}
						>
							{reordering ? tr('Done', 'Готово') : tab.arrange}
						</div>
					)}
				</div>
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
				{onOptions && (
					<OptionList
						options={options}
						focused={zone === 'content'}
						selected={optionIndex}
						onSelect={(i) => { setZone('content'); setOptionIndex(i); }}
						onActivate={(i) => { setZone('content'); onOption(options[i].id); }}
					/>
				)}
				{reordering && count === 0 && <p class="settings__hint">{tr('Nothing on the home screen yet.', 'На главном экране пока ничего нет.')}</p>}
			</div>
		</div>
	);
}
