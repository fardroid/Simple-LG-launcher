import {useCallback, useEffect, useRef, useState} from 'preact/hooks';
import {PickerIcon} from '../components/Glyph';
import {useKeys, clamp, type NavKey} from '../hooks/useKeys';
import {exec, shellQuote} from '../lib/apps';
import {normalizeUrl, validSource} from '../lib/calendar';
import {isWebOS} from '../lib/luna';
import {INTERNAL_ROOT} from '../lib/wallpaper';
import {fmt, tr} from '../lib/i18n';

/** A text file with one calendar per line (an address, a file or a folder), so nothing has to be typed with the remote. */
export const CALENDAR_FILE = INTERNAL_ROOT.path + '/calendar.txt';
/** The folder whose .ics files are all shown. */
export const CALENDAR_FOLDER = INTERNAL_ROOT.path + '/cals/';
const MAX_SOURCES = 8;

interface Props {
	/** the calendars now in use */
	sources: string[];
	/** reading files and folders needs the Homebrew Channel (root) */
	canReadFiles: boolean;
	onChoose: (sources: string[]) => void;
	onClose: () => void;
}

interface SetupRow {
	key: 'type' | 'file' | 'folder' | 'clear' | 'source' | 'info';
	label: string;
	detail?: string;
}

/** A short name for a source in the list. */
function shortName (src: string): string {
	if (src.charAt(0) === '/') return src;
	const m = /^https?:\/\/([^\/]+)/i.exec(src);
	return (m ? m[1] : src) + ' …' + src.slice(-8);
}

/** Choose the calendars: add an address typed with the TV's on-screen keyboard, read a list from a file, or use a folder. */
export function CalendarSetup ({sources, canReadFiles, onChoose, onClose}: Props) {
	const [typing, setTyping] = useState(false);
	const [index, setIndex] = useState(0);
	const [note, setNote] = useState('');
	const input = useRef<HTMLInputElement>(null);

	const rows: SetupRow[] = [{key: 'type', label: tr('Add a link…', 'Добавить ссылку…'), detail: tr('The secret address in iCal format from Google Calendar, typed with the on-screen keyboard', 'Закрытый адрес в формате iCal из Google Календаря. Печатается экранной клавиатурой')}];
	if (canReadFiles) {
		rows.push({key: 'folder', label: tr('Add a folder of calendars', 'Добавить папку с календарями'), detail: fmt(tr('Every .ics file in {0}', 'Все файлы .ics из {0}'), CALENDAR_FOLDER)});
		rows.push({key: 'file', label: tr('Read the list from a file', 'Взять список из файла'), detail: fmt(tr('One calendar per line, in {0}', 'По одному календарю в строке, в файле {0}'), CALENDAR_FILE)});
	}
	if (sources.length) rows.push({key: 'clear', label: tr('Remove all calendars', 'Убрать все календари'), detail: tr('The calendar card leaves the home screen', 'Календарь на главном экране исчезнет')});
	if (note) rows.push({key: 'info', label: note});
	sources.forEach((s) => rows.push({key: 'source', label: shortName(s)}));

	useEffect(() => { if (typing && input.current) input.current.focus(); }, [typing]);

	const add = (raw: string): boolean => {
		const src = raw.charAt(0) === '/' ? raw.trim() : normalizeUrl(raw);
		if (!validSource(src)) { setNote(tr('That does not look like a link. It should start with https://', 'Это не похоже на ссылку. Она должна начинаться с https://')); return false; }
		if (sources.indexOf(src) !== -1) { setNote(tr('This calendar is already on the list', 'Этот календарь уже подключён')); return false; }
		if (sources.length >= MAX_SOURCES) { setNote(fmt(tr('No more than {0} calendars', 'Не больше {0} календарей'), MAX_SOURCES)); return false; }
		onChoose(sources.concat(src));
		return true;
	};

	const readFile = () => {
		if (!isWebOS()) return;
		setNote(tr('Reading the file…', 'Читаю файл…'));
		exec('head -c 4000 ' + shellQuote(CALENDAR_FILE), 8000).then(
			(text) => {
				const lines = text.split('\n').map((l) => l.trim()).filter((l) => l && l.charAt(0) !== '#');
				const list = lines.map((l) => (l.charAt(0) === '/' ? l : normalizeUrl(l))).filter((l) => validSource(l)).slice(0, MAX_SOURCES);
				if (!list.length) setNote(fmt(tr('{0} has no links in it, or is empty', 'В файле {0} нет ссылок или он пуст'), CALENDAR_FILE));
				else onChoose(list);
			},
			() => setNote(tr('Could not read the file. This needs a rooted TV with the Homebrew Channel', 'Не удалось прочитать файл. Нужен рутованный ТВ с Homebrew Channel'))
		);
	};

	const activate = (i: number) => {
		const row = rows[i];
		if (!row) return;
		setIndex(i);
		if (row.key === 'type') setTyping(true);
		else if (row.key === 'file') readFile();
		else if (row.key === 'folder') add(CALENDAR_FOLDER);
		else if (row.key === 'clear') onChoose([]);
		else if (row.key === 'source') onChoose(sources.filter((s) => shortName(s) !== row.label));
	};

	const handleKey = useCallback((key: NavKey): boolean => {
		if (key === 'up') setIndex((i) => clamp(i - 1, 0, Math.max(0, rows.length - 1)));
		else if (key === 'down') setIndex((i) => clamp(i + 1, 0, Math.max(0, rows.length - 1)));
		else if (key === 'enter') activate(index);
		else if (key === 'back') onClose();
		return true;
	}, [rows.length, index, sources, canReadFiles]);

	useKeys(handleKey, !typing);

	const onFieldKey = (e: KeyboardEvent) => {
		if (e.keyCode === 13) {
			e.preventDefault();
			const value = (e.target as HTMLInputElement).value;
			if (value.trim().length > 0 && add(value)) return;
			setTyping(false);
		} else if (e.keyCode === 461 || e.keyCode === 27) { e.preventDefault(); setTyping(false); }
	};

	return (
		<div class="screen screen--picker">
			<div class="settings">
				<h1 class="settings__title">{tr('Calendars', 'Календари')}</h1>
				<p class="settings__hint">{typing ? tr('Type the link and press OK · Back cancels', 'Введите ссылку и нажмите OK · Назад отменяет') : tr('OK on a calendar removes it · Back returns to Settings', 'OK на подключённом календаре убирает его · Назад возвращает в настройки')}</p>
				<div class="picker__path">{sources.length ? fmt(tr('Connected: {0}', 'Подключено: {0}'), sources.length) : tr('No calendars yet', 'Календарей пока нет')}</div>
				<div class="picker">
					{rows.map((row, i) => {
						const editing = typing && row.key === 'type' && i === index;
						return (
							<div
								key={row.key + i}
								class={`picker__row${i === index ? ' picker__row--focused' : ''}${row.key === 'info' ? ' picker__row--info' : ''}`}
								onMouseEnter={() => { if (!typing) setIndex(i); }}
								onClick={() => activate(i)}
							>
								<PickerIcon kind={row.key === 'type' ? 'search' : row.key === 'info' ? 'info' : row.key === 'file' ? 'drive' : row.key === 'source' ? 'picture' : 'folder'} />
								<div class="picker__text">
									{editing
										? <input ref={input} class="picker__input" type="text" placeholder="https://calendar.google.com/calendar/ical/…" maxLength={600} onKeyDown={onFieldKey} />
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
