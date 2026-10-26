import {useCallback, useEffect, useRef, useState} from 'preact/hooks';
import {PickerIcon} from '../components/Glyph';
import {useKeys, clamp, type NavKey} from '../hooks/useKeys';
import {presetCities} from '../lib/cities';
import {searchCities} from '../lib/weathernet';
import type {City} from '../lib/weather';
import {tr} from '../lib/i18n';

interface Props {
	/** the search may go through the TV's shell when the web view cannot reach the service */
	allowShell: boolean;
	onChoose: (city: City) => void;
	onClose: () => void;
}

interface CityRow {
	key: string;
	kind: 'search' | 'back' | 'city' | 'info';
	label: string;
	detail?: string;
	city?: City;
}

const VISIBLE_ROWS = 9;

/** Choose the city for the weather: a short list to pick from, or a search by name (typed with the TV's
 *  on-screen keyboard). OK on a city chooses it; Back goes up. */
export function CityPicker ({allowShell, onChoose, onClose}: Props) {
	const [results, setResults] = useState<City[] | null>(null);       // null: showing the list; otherwise search results
	const [typing, setTyping] = useState(false);
	const [busy, setBusy] = useState(false);
	const [failed, setFailed] = useState(false);
	const [index, setIndex] = useState(0);
	const input = useRef<HTMLInputElement>(null);

	const rows: CityRow[] = [];
	if (results === null) {
		rows.push({key: 'search', kind: 'search', label: tr('Search for a city…', 'Найти город…'), detail: tr('Type the name with the on-screen keyboard', 'Введите название на экранной клавиатуре')});
		presetCities().forEach((c) => rows.push({key: c.name + c.lat, kind: 'city', label: c.name, detail: c.detail, city: c}));
	} else {
		rows.push({key: 'search', kind: 'search', label: tr('Search again…', 'Искать снова…')});
		rows.push({key: 'back', kind: 'back', label: tr('Back to the list', 'Назад к списку')});
		if (busy) rows.push({key: 'busy', kind: 'info', label: tr('Searching…', 'Поиск…')});
		else if (failed) rows.push({key: 'failed', kind: 'info', label: tr('The search did not work', 'Не удалось выполнить поиск'), detail: tr('Check the TV\'s internet connection and try again', 'Проверьте подключение ТВ к интернету и повторите')});
		else if (results.length === 0) rows.push({key: 'none', kind: 'info', label: tr('Nothing found', 'Ничего не найдено'), detail: tr('Try another spelling, or just the city name', 'Попробуйте другое написание или только название города')});
		results.forEach((c, i) => rows.push({key: 'r' + i, kind: 'city', label: c.name, detail: c.detail, city: c}));
	}

	useEffect(() => { if (typing && input.current) input.current.focus(); }, [typing]);

	const runSearch = (text: string) => {
		setTyping(false);
		if (input.current) input.current.blur();
		if (text.trim().length < 2) return;
		setResults([]);
		setBusy(true);
		setFailed(false);
		setIndex(0);
		searchCities(text, allowShell).then(
			(list) => { setResults(list); setBusy(false); setIndex(list.length ? (results === null ? 2 : 2) : 0); },
			() => { setResults([]); setBusy(false); setFailed(true); }
		);
	};

	const activate = (i: number) => {
		const row = rows[i];
		if (!row) return;
		if (row.kind === 'search') { setIndex(i); setTyping(true); }
		else if (row.kind === 'back') { setResults(null); setIndex(0); }
		else if (row.kind === 'city' && row.city) onChoose(row.city);
	};

	const handleKey = useCallback((key: NavKey): boolean => {
		switch (key) {
			case 'up': setIndex((i) => clamp(i - 1, 0, Math.max(0, rows.length - 1))); break;
			case 'down': setIndex((i) => clamp(i + 1, 0, Math.max(0, rows.length - 1))); break;
			case 'enter': activate(index); break;
			case 'back': if (results !== null) { setResults(null); setIndex(0); } else onClose(); break;
			default: break;
		}
		return true;
	}, [rows.length, index, results, busy, failed]);

	useKeys(handleKey, !typing);

	// The text field: Enter searches, Back leaves it.
	const onFieldKey = (e: KeyboardEvent) => {
		if (e.keyCode === 13) { e.preventDefault(); runSearch((e.target as HTMLInputElement).value); }
		else if (e.keyCode === 461 || e.keyCode === 27) { e.preventDefault(); setTyping(false); }
	};

	const start = clamp(index - Math.floor(VISIBLE_ROWS / 2), 0, Math.max(0, rows.length - VISIBLE_ROWS));
	const visible = rows.slice(start, start + VISIBLE_ROWS);

	return (
		<div class="screen screen--picker">
			<div class="settings">
				<h1 class="settings__title">{tr('Choose a city', 'Выберите город')}</h1>
				<p class="settings__hint">{typing ? tr('Type a city and press OK · Back cancels', 'Введите город и нажмите OK · Назад отменяет') : tr('OK chooses a city · Back returns to Settings', 'OK выбирает город · Назад — к настройкам')}</p>
				<div class="picker__path">{results === null ? tr('Popular cities', 'Популярные города') : tr('Search results', 'Результаты поиска')}</div>
				<div class="picker">
					{visible.map((row, n) => {
						const i = start + n;
						const editing = typing && row.kind === 'search' && i === index;
						return (
							<div
								key={row.key}
								class={`picker__row${i === index ? ' picker__row--focused' : ''}${row.kind === 'info' ? ' picker__row--info' : ''}`}
								onMouseEnter={() => { if (!typing) setIndex(i); }}
								onClick={() => { setIndex(i); activate(i); }}
							>
								<PickerIcon kind={row.kind === 'search' ? 'search' : row.kind === 'info' ? 'info' : row.kind === 'back' ? 'folder' : 'pin'} />
								<div class="picker__text">
									{editing
										? <input ref={input} class="picker__input" type="text" placeholder={tr('City name', 'Название города')} maxLength={60} onKeyDown={onFieldKey} />
										: <div class="picker__label">{row.label}</div>}
									{!editing && row.detail && <div class="picker__detail">{row.detail}</div>}
								</div>
							</div>
						);
					})}
				</div>
			</div>
		</div>
	);
}
