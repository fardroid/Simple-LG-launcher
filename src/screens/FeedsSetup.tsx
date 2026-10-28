import {useCallback, useEffect, useRef, useState} from 'preact/hooks';
import {PickerIcon} from '../components/Glyph';
import {useKeys, clamp, type NavKey} from '../hooks/useKeys';
import {normalizeFeedUrl, validFeedUrl} from '../lib/feeds';
import {MAX_FEEDS} from '../lib/storage';
import {fmt, tr} from '../lib/i18n';

interface Props {
	/** the feeds now in use */
	feeds: string[];
	onChoose: (feeds: string[]) => void;
	onClose: () => void;
}

interface SetupRow {
	key: 'type' | 'clear' | 'feed' | 'info';
	label: string;
	detail?: string;
	feed?: string;
}

/** A short name for a feed in the list: its site and the end of its address. */
function shortName (url: string): string {
	const m = /^https?:\/\/([^\/]+)(.*)$/i.exec(url);
	if (!m) return url;
	const rest = m[2].length > 24 ? '…' + m[2].slice(-22) : m[2];
	return m[1] + rest;
}

/** The user's own news feeds: add an address typed with the TV's on-screen keyboard; OK on a feed removes it. */
export function FeedsSetup ({feeds, onChoose, onClose}: Props) {
	const [typing, setTyping] = useState(false);
	const [index, setIndex] = useState(0);
	const [note, setNote] = useState('');
	const input = useRef<HTMLInputElement>(null);

	const rows: SetupRow[] = [{key: 'type', label: tr('Add a feed…', 'Добавить ленту…'), detail: tr('The address of an RSS or Atom feed, typed with the on-screen keyboard', 'Адрес RSS- или Atom-ленты. Печатается экранной клавиатурой')}];
	if (feeds.length) rows.push({key: 'clear', label: tr('Remove all feeds', 'Убрать все ленты'), detail: tr('Only Google News stays, if it is on', 'Останутся только Google Новости, если они включены')});
	if (note) rows.push({key: 'info', label: note});
	feeds.forEach((f) => rows.push({key: 'feed', label: shortName(f), detail: tr('OK removes it', 'OK убирает её'), feed: f}));

	useEffect(() => { if (typing && input.current) input.current.focus(); }, [typing]);

	const add = (raw: string): boolean => {
		const url = normalizeFeedUrl(raw);
		if (!validFeedUrl(url)) { setNote(tr('That does not look like a link. It should start with https://', 'Это не похоже на ссылку. Она должна начинаться с https://')); return false; }
		if (feeds.indexOf(url) !== -1) { setNote(tr('This feed is already on the list', 'Эта лента уже добавлена')); return false; }
		if (feeds.length >= MAX_FEEDS) { setNote(fmt(tr('No more than {0} feeds', 'Не больше {0} лент'), MAX_FEEDS)); return false; }
		setNote('');
		onChoose(feeds.concat(url));
		return true;
	};

	const activate = (i: number) => {
		const row = rows[i];
		if (!row) return;
		setIndex(i);
		if (row.key === 'type') setTyping(true);
		else if (row.key === 'clear') onChoose([]);
		else if (row.key === 'feed' && row.feed) { onChoose(feeds.filter((f) => f !== row.feed)); setIndex((n) => Math.max(0, n - 1)); }
	};

	const handleKey = useCallback((key: NavKey): boolean => {
		if (key === 'up') setIndex((i) => clamp(i - 1, 0, Math.max(0, rows.length - 1)));
		else if (key === 'down') setIndex((i) => clamp(i + 1, 0, Math.max(0, rows.length - 1)));
		else if (key === 'enter') activate(index);
		else if (key === 'back') onClose();
		return true;
	}, [rows.length, index, feeds]);

	useKeys(handleKey, !typing);

	const onFieldKey = (e: KeyboardEvent) => {
		if (e.keyCode === 13) {
			e.preventDefault();
			const value = (e.target as HTMLInputElement).value;
			if (value.trim().length > 0 && add(value)) { setTyping(false); return; }
			if (!value.trim()) setTyping(false);
		} else if (e.keyCode === 461 || e.keyCode === 27) { e.preventDefault(); setTyping(false); }
	};

	return (
		<div class="screen screen--picker">
			<div class="settings">
				<h1 class="settings__title">{tr('Your feeds', 'Свои ленты')}</h1>
				<p class="settings__hint">{typing ? tr('Type the link and press OK · Back cancels', 'Введите ссылку и нажмите OK · Назад отменяет') : tr('OK on a feed removes it · Back returns to Settings', 'OK на ленте убирает её · Назад возвращает в настройки')}</p>
				<div class="picker__path">{feeds.length ? fmt(tr('Added: {0}', 'Добавлено: {0}'), feeds.length) : tr('No feeds of your own yet', 'Своих лент пока нет')}</div>
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
								<PickerIcon kind={row.key === 'type' ? 'search' : row.key === 'info' ? 'info' : row.key === 'feed' ? 'picture' : 'folder'} />
								<div class="picker__text">
									{editing
										? <input ref={input} class="picker__input" type="text" placeholder="https://example.com/rss.xml" maxLength={600} onKeyDown={onFieldKey} />
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
