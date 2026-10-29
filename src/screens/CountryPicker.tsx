import {useCallback, useState} from 'preact/hooks';
import {PickerIcon} from '../components/Glyph';
import {useKeys, clamp, type NavKey} from '../hooks/useKeys';
import {COUNTRIES, countryName, editionFor} from '../lib/feeds';
import {fmt, lang, langTag, LANG_INFO, tr, type Lang} from '../lib/i18n';

interface Props {
	/** the country in use */
	current: string;
	onChoose: (country: string) => void;
	onClose: () => void;
}

const VISIBLE_ROWS = 9;

/** Choose the country of Google News: every country with an edition, by its name in the screen's language. Under each, the
 *  language its headlines come in (the launcher's when the country has an edition in it). */
export function CountryPicker ({current, onChoose, onClose}: Props) {
	const tag = langTag();
	const rows = COUNTRIES.map((code) => ({code, name: countryName(code, tag)})).sort((a, b) => a.name.localeCompare(b.name, tag));
	const [index, setIndex] = useState(() => Math.max(0, rows.findIndex((r) => r.code === current)));

	const handleKey = useCallback((key: NavKey): boolean => {
		if (key === 'up') setIndex((i) => clamp(i - 1, 0, rows.length - 1));
		else if (key === 'down') setIndex((i) => clamp(i + 1, 0, rows.length - 1));
		else if (key === 'enter') onChoose(rows[index].code);
		else if (key === 'back') onClose();
		return true;
	}, [index, rows.length]);
	useKeys(handleKey, true);

	const start = clamp(index - Math.floor(VISIBLE_ROWS / 2), 0, Math.max(0, rows.length - VISIBLE_ROWS));
	const visible = rows.slice(start, start + VISIBLE_ROWS);
	/** The language of a country's headlines: by the TV's own names where it has them, else as the language calls itself. */
	const language = (code: string): string => {
		const edition = editionFor(code, lang());
		try {
			const Names = (Intl as unknown as {DisplayNames?: new (l: string[], o: {type: string}) => {of: (c: string) => string | undefined}}).DisplayNames;
			const name = Names ? new Names([tag], {type: 'language'}).of(String(edition[0])) : '';
			if (name && name !== edition[0]) return name;
		} catch { /* an older TV */ }
		const own = LANG_INFO[edition[0] as Lang];
		return own ? own.name : edition[1];
	};

	return (
		<div class="screen screen--picker">
			<div class="settings">
				<h1 class="settings__title">{tr('Country of the news', 'Страна новостей')}</h1>
				<p class="settings__hint">{tr('OK chooses a country · Back returns to Settings', 'OK выбирает страну · Назад — к настройкам')}</p>
				<div class="picker__path">{fmt(tr('Google News editions: {0}', 'Редакций Google Новостей: {0}'), rows.length)}</div>
				<div class="picker">
					{visible.map((row, n) => {
						const i = start + n;
						return (
							<div key={row.code} class={`picker__row${i === index ? ' picker__row--focused' : ''}`} onMouseEnter={() => setIndex(i)} onClick={() => onChoose(row.code)}>
								<PickerIcon kind="pin" />
								<div class="picker__text">
									<div class="picker__label">{row.name}{row.code === current ? ' ✓' : ''}</div>
									<div class="picker__detail">{language(row.code)}</div>
								</div>
							</div>
						);
					})}
				</div>
			</div>
		</div>
	);
}
