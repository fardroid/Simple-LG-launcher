import {useCallback, useEffect, useRef, useState} from 'preact/hooks';
import {CheckMark, PickerIcon} from '../components/Glyph';
import {failureText, whySuffix} from '../lib/failure';
import {useKeys, clamp, type NavKey} from '../hooks/useKeys';
import {
	baseName, INTERNAL_ROOT, joinPath, listDir, listRoots, parentPath,
	type DirEntry, type StorageRoot
} from '../lib/wallpaper';
import {count as countOf, fmt, tr} from '../lib/i18n';
import {cachedThumb, loadThumb} from '../lib/thumbs';

/** What the user picked. */
export type WallpaperChoice =
	| {mode: 'gradient'}
	| {mode: 'image'; path: string}
	| {mode: 'folder'; path: string}
	| {mode: 'pictures'; paths: string[]};

interface Props {
	onChoose: (choice: WallpaperChoice) => void;
	onClose: () => void;
	/** the pictures chosen now (one picture, or the ticked ones), ticked when the gallery opens */
	chosen?: string[];
}

type RowKind = 'gradient' | 'root' | 'folder' | 'picture' | 'slideshow' | 'info' | 'retry';

interface PickerRow {
	key: string;
	kind: RowKind;
	label: string;
	detail?: string;
	path?: string;
}

/** the gallery: tiles across, and the height of a row of tiles with its name and the gap under it (global.css .gallery) */
const COLUMNS = 5;
const ROW_GAP = 28;
const ROW_STEP = 158 + 74 + ROW_GAP;
const VIEWPORT_HEIGHT = 760;

const hints = () => ({
	roots: tr('Choose where the pictures are · Back returns to Settings', 'Выберите, где лежат картинки · Назад — к настройкам'),
	folder: tr('OK ticks pictures for a slideshow or opens a folder · Back goes up a level', 'OK отмечает картинки для слайд-шоу или открывает папку · Назад — на уровень вверх')
});

/** The tile's preview: the picture itself for a picture, otherwise an icon. */
function Preview ({row, thumb}: {row: PickerRow; thumb: string | null | undefined}) {
	if (row.kind === 'picture' && thumb) return <img class="gallery__image" src={thumb} alt="" />;
	if (row.kind === 'gradient') return <div class="gallery__image gallery__image--default" />;
	const kind = row.kind === 'retry' ? 'info' : row.kind === 'root' ? (row.path === INTERNAL_ROOT.path ? 'folder' : 'drive') : row.kind;
	return <div class={`gallery__icon${row.kind === 'picture' && thumb === undefined ? ' gallery__icon--waiting' : ''}`}><PickerIcon kind={kind} /></div>;
}

/** Long paths keep their end (the current folder) and lose the start. */
function shortPath (path: string): string {
	return path.length > 90 ? `…${path.slice(path.length - 89)}` : path;
}

function pictureCount (entries: DirEntry[]): number {
	return entries.filter((e) => !e.isDir).length;
}

function buildRows (path: string | null, roots: StorageRoot[], entries: DirEntry[], failed: boolean): PickerRow[] {
	if (failed) {
		const rows: PickerRow[] = [{key: 'retry', kind: 'retry', label: tr('Try again', 'Повторить'), detail: tr('Read the storage again', 'Прочитать память снова')}];
		if (path === null) rows.unshift({key: 'gradient', kind: 'gradient', label: tr('Default background', 'Фон по умолчанию'), detail: tr('Black or a calm gradient, depending on the theme', 'Чёрный или спокойный градиент, в зависимости от темы')});
		return rows;
	}
	if (path === null) {
		const rows: PickerRow[] = [{key: 'gradient', kind: 'gradient', label: tr('Default background', 'Фон по умолчанию'), detail: tr('Black or a calm gradient, depending on the theme', 'Чёрный или спокойный градиент, в зависимости от темы')}];
		roots.forEach((r) => rows.push({key: `root:${r.path}`, kind: 'root', label: r.label, detail: r.path, path: r.path}));
		if (!roots.some((r) => r.path !== INTERNAL_ROOT.path)) {
			rows.push({key: 'no-usb', kind: 'info', label: tr('No USB drive found', 'USB-накопитель не найден'), detail: tr('Plug one in and open this screen again', 'Подключите его и откройте этот экран снова')});
		}
		return rows;
	}
	const rows: PickerRow[] = [];
	const count = pictureCount(entries);
	if (count > 0) {
		rows.push({
			key: 'slideshow', kind: 'slideshow', label: tr('Slideshow', 'Слайд-шоу'),
			detail: fmt(tr('{0} in {1}', '{0} в папке {1}'), countOf(count, ['picture', 'pictures'], ['картинка', 'картинки', 'картинок']), baseName(path))
		});
	}
	entries.forEach((e) => rows.push({
		key: `${e.isDir ? 'dir' : 'pic'}:${e.name}`,
		kind: e.isDir ? 'folder' : 'picture',
		label: e.name,
		path: joinPath(path, e.name)
	}));
	if (!rows.length) rows.push({key: 'empty', kind: 'info', label: tr('Nothing here', 'Здесь пусто'), detail: tr('This folder has no pictures or folders', 'В этой папке нет картинок и папок')});
	return rows;
}

/** Gallery for choosing the wallpaper, in tiles like the apps in Settings, each picture with its preview.
 *  Top level: the default gradient, USB drives, internal storage. Inside a folder: sub-folders,
 *  pictures, and (when it has pictures) a tile that plays the whole folder as a slideshow.
 *  OK ticks pictures, in any folders, as the apps are ticked in Settings; leaving the gallery shows them (one picture, or a
 *  slideshow of them). The default background and a folder's slideshow are chosen at once. */
export function Picker ({onChoose, onClose, chosen = []}: Props) {
	const [ticked, setTicked] = useState<string[]>(chosen);
	const [roots, setRoots] = useState<StorageRoot[]>([]);
	const [path, setPath] = useState<string | null>(null);
	const [entries, setEntries] = useState<DirEntry[]>([]);
	const [index, setIndex] = useState(0);
	const [loading, setLoading] = useState(true);
	const [error, setError] = useState('');
	const [attempt, setAttempt] = useState(0);
	const remembered = useRef<Record<string, number>>({});
	// the previews come in one at a time, the nearest to the highlight first; each one that arrives draws the gallery again
	const [, setThumbsMade] = useState(0);
	const at = useRef(index);
	at.current = index;

	const rows = buildRows(path, roots, entries, !loading && error !== '');
	const pathKey = path === null ? '' : path;

	// Roots load once; a folder's contents load whenever the path changes.
	useEffect(() => {
		let cancelled = false;
		if (path === null) {
			setLoading(true);
			setError('');
			listRoots().then(
				(r) => { if (!cancelled) { setRoots(r); setLoading(false); } },
				(e) => { if (!cancelled) { setRoots([]); setError(failureText(e, tr('read the storage', 'прочитать память'))); setLoading(false); } }
			);
		} else {
			setLoading(true);
			setError('');
			listDir(path).then(
				(list) => { if (!cancelled) { setEntries(list); setLoading(false); } },
				() => { if (!cancelled) { setEntries([]); setError(tr('Could not read the folder', 'Не удалось прочитать папку') + (whySuffix() || '')); setLoading(false); } }
			);
		}
		return () => { cancelled = true; };
	}, [path, attempt]);

	// Previews for the pictures of the folder shown, one after another, always the one nearest the highlight next
	useEffect(() => {
		if (loading || path === null) return undefined;
		let cancelled = false;
		const offset = rows.length - entries.length;          // the slideshow tile comes before the entries
		const todo = entries.map((e, i) => ({i: i + offset, path: joinPath(path, e.name), pic: !e.isDir})).filter((t) => t.pic && cachedThumb(t.path) === undefined);
		const next = (): void => {
			if (cancelled || !todo.length) return;
			let best = 0;
			todo.forEach((t, k) => { if (Math.abs(t.i - at.current) < Math.abs(todo[best].i - at.current)) best = k; });
			const t = todo.splice(best, 1)[0];
			loadThumb(t.path).then(() => { if (!cancelled) { setThumbsMade((n) => n + 1); next(); } });
		};
		next();
		return () => { cancelled = true; };
	}, [loading, path, entries]);

	const goTo = (next: string | null, restoreIndex: number) => {
		remembered.current[pathKey] = index;
		setEntries([]);
		setIndex(restoreIndex);
		setPath(next);
	};

	/** Leaving the gallery: the ticks are the new wallpaper when they changed (none left: the default background). */
	const leave = () => {
		const same = ticked.length === chosen.length && ticked.every((p) => chosen.indexOf(p) !== -1);
		if (same) onClose();
		else onChoose(ticked.length ? {mode: 'pictures', paths: ticked} : {mode: 'gradient'});
	};

	const goUp = () => {
		if (path === null) { leave(); return; }
		const atRoot = roots.some((r) => r.path === path);
		const target = atRoot ? null : parentPath(path);
		const restore = remembered.current[target === null ? '' : target] || 0;
		remembered.current[pathKey] = 0;
		setEntries([]);
		setIndex(restore);
		setPath(target);
	};

	const activate = (i: number) => {
		const row = rows[i];
		if (!row || loading) return;
		switch (row.kind) {
			case 'gradient': onChoose({mode: 'gradient'}); break;
			case 'retry': setIndex(0); setAttempt((n) => n + 1); break;
			case 'root':
			case 'folder': if (row.path) goTo(row.path, 0); break;
			case 'picture': {
				const p = row.path;
				if (p) setTicked((list) => (list.indexOf(p) === -1 ? list.concat(p) : list.filter((x) => x !== p)));
				break;
			}
			case 'slideshow': if (path) onChoose({mode: 'folder', path}); break;
			default: break;
		}
	};

	const handleKey = useCallback((key: NavKey): boolean => {
		const last = Math.max(0, rows.length - 1);
		switch (key) {
			case 'left': setIndex((i) => clamp(i - 1, 0, last)); break;
			case 'right': setIndex((i) => clamp(i + 1, 0, last)); break;
			case 'up': setIndex((i) => (i >= COLUMNS ? i - COLUMNS : i)); break;
			// down from the row above the last one goes to the last tile when there is none right under the highlight
			case 'down': setIndex((i) => (Math.floor(i / COLUMNS) < Math.floor(last / COLUMNS) ? Math.min(last, i + COLUMNS) : i)); break;
			case 'enter': activate(index); break;
			case 'back': goUp(); break;
			default: break;
		}
		return true;
	}, [rows.length, index, path, roots, loading, entries, ticked]);

	useKeys(handleKey, true);

	// the highlighted row of tiles stays in the middle of the gallery (clamped at the ends), as in the grid of apps in Settings
	const lines = Math.ceil(rows.length / COLUMNS);
	const total = lines * ROW_STEP - ROW_GAP;
	const offset = total <= VIEWPORT_HEIGHT ? 0 : clamp(Math.floor(index / COLUMNS) * ROW_STEP - (VIEWPORT_HEIGHT - ROW_STEP) / 2, 0, total - VIEWPORT_HEIGHT);

	return (
		<div class="screen screen--picker screen--gallery">
			<div class="settings">
				<h1 class="settings__title">{tr('Wallpaper', 'Выбор обоев', 'Choose wallpaper')}</h1>
				<p class="settings__hint">{path === null ? hints().roots : hints().folder}</p>
				<div class="picker__path">{path === null ? tr('Locations', 'Расположения') : shortPath(path)}</div>
				{loading && <div class="picker__status">{tr('Loading…', 'Загрузка…')}</div>}
				{!loading && error && <div class="picker__status picker__status--error">{error}</div>}
				{!loading && (
					<div class="gallery">
						<div class="gallery__track" style={{transform: `translate3d(0, ${-Math.round(offset)}px, 0)`}}>
							{rows.map((row, i) => (
								<div
									key={row.key}
									class={`gallery__tile${i === index ? ' gallery__tile--focused' : ''}${row.kind === 'info' ? ' gallery__tile--info' : ''}`}
									onMouseEnter={() => setIndex(i)}
									onClick={() => { setIndex(i); activate(i); }}
								>
									<div class="gallery__frame">
										<div class="gallery__box">
											<Preview row={row} thumb={row.kind === 'picture' && row.path ? cachedThumb(row.path) : undefined} />
											{row.kind === 'picture' && row.path && ticked.indexOf(row.path) !== -1 && <div class="tile__badge"><CheckMark /></div>}
										</div>
										<div class="gallery__ring" />
									</div>
									<div class="gallery__label">{row.label}</div>
									{row.detail && <div class="gallery__detail">{row.detail}</div>}
								</div>
							))}
						</div>
					</div>
				)}
			</div>
		</div>
	);
}
