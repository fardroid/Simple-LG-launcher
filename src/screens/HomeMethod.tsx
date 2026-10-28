import {useCallback, useEffect, useState} from 'preact/hooks';
import {CheckMark} from '../components/Glyph';
import {useKeys, clamp, type NavKey} from '../hooks/useKeys';
import {HOME_MODES, modeNote, modeText, type HomeMode, type HomeModeInfo} from '../lib/homemode';
import {tr} from '../lib/i18n';

interface Props {
	/** what is on now and what this TV can do; null while it is being read (or could not be) */
	info: HomeModeInfo | null;
	/** the way being switched to, while the switch runs */
	busy: HomeMode | null;
	onPick: (mode: HomeMode) => void;
	onClose: () => void;
}

/** Settings → Home screen → How the launcher opens: the four ways, the one that is on ticked, and for the focused one
 *  how it works, its pros and cons. OK on another way asks once more ("press OK again") before switching, so a stray
 *  press never changes what the Home button does. */
export function HomeMethod ({info, busy, onPick, onClose}: Props) {
	const current = info ? info.mode : null;
	const [index, setIndex] = useState(() => Math.max(0, HOME_MODES.indexOf(current || 'default')));
	const [armed, setArmed] = useState<HomeMode | null>(null);
	useEffect(() => { if (current) setIndex(HOME_MODES.indexOf(current)); }, [current]);
	useEffect(() => setArmed(null), [index, current]);

	const focusedMode = HOME_MODES[index];
	const text = modeText(focusedMode);
	const note = modeNote(focusedMode, info);

	const activate = (i: number) => {
		const mode = HOME_MODES[i];
		if (busy || !info || mode === current || modeNote(mode, info).blocked) return;
		if (armed !== mode) { setArmed(mode); return; }
		setArmed(null);
		onPick(mode);
	};

	const handleKey = useCallback((key: NavKey): boolean => {
		switch (key) {
			case 'up': setIndex((i) => clamp(i - 1, 0, HOME_MODES.length - 1)); break;
			case 'down': setIndex((i) => clamp(i + 1, 0, HOME_MODES.length - 1)); break;
			case 'enter': activate(index); break;
			case 'back': if (!busy) onClose(); break;
			default: break;
		}
		return true;
	}, [index, armed, busy, info, current]);

	useKeys(handleKey, true);

	let action = tr('OK switches to this way', 'OK — включить этот способ');
	if (!info) action = tr('Reading the TV\'s settings…', 'Читаю настройки ТВ…');
	else if (busy) action = busy === focusedMode ? tr('Switching… The screen may blink.', 'Переключаю… Экран может мигнуть.') : tr('Switching…', 'Переключаю…');
	else if (focusedMode === current) action = tr('This way is on now', 'Этот способ включён сейчас');
	else if (note.blocked) action = note.text;
	else if (armed === focusedMode) action = tr('Press OK again to switch', 'Нажмите OK ещё раз, чтобы переключить');

	return (
		<div class="screen screen--picker">
			<div class="settings homemethod">
				<h1 class="settings__title">{tr('How the launcher opens', 'Как открывается лаунчер')}</h1>
				<p class="settings__hint">{tr('What the Home button, Back out of the last app and waking the TV open. One way at a time.', 'Что открывают кнопка Home, выход из последнего приложения и включение ТВ. Работает один способ за раз.')}</p>
				<div class="homemethod__body">
					<div class="picker homemethod__list">
						{HOME_MODES.map((mode, i) => {
							const t = modeText(mode);
							const n = modeNote(mode, info);
							return (
								<div
									key={mode}
									class={`picker__row${i === index ? ' picker__row--focused' : ''}${n.blocked ? ' picker__row--disabled' : ''}`}
									onMouseEnter={() => setIndex(i)}
									onClick={() => { setIndex(i); activate(i); }}
								>
									<div class="homemethod__tick">{mode === current && <CheckMark />}</div>
									<div class="picker__text">
										<div class="picker__label">{t.name}</div>
										<div class="picker__detail">{mode === current ? tr('On now', 'Включено сейчас') : n.text || (busy === mode ? tr('Switching…', 'Переключаю…') : '')}</div>
									</div>
								</div>
							);
						})}
					</div>
					<div class="homemethod__panel">
						<p class="homemethod__how">{text.how}</p>
						<ul class="homemethod__list homemethod__list--pro">{text.pros.map((p) => <li key={p}>{p}</li>)}</ul>
						<ul class="homemethod__list homemethod__list--con">{text.cons.map((c) => <li key={c}>{c}</li>)}</ul>
						{note.text && !note.blocked && <p class="homemethod__note">{note.text}</p>}
						<p class={`homemethod__action${armed === focusedMode ? ' homemethod__action--armed' : ''}`}>{action}</p>
					</div>
				</div>
			</div>
		</div>
	);
}
