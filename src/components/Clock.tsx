import {useLayoutEffect, useRef} from 'preact/hooks';
import {dateLines, timeParts, type TimeFormat} from '../lib/clock';
import type {ClockConfig} from '../lib/storage';
import {colorInfo} from '../lib/clockcolor';
import {digitsTop, fontFamily} from '../lib/clockfont';
import {plateVars, type PlateLook} from '../lib/plates';
import {Frost} from './Frost';
import {memoProps} from '../lib/memoprops';

/** The clock card: the time (with a small AM/PM beside it in 12-hour mode) and, to its right, the day and date.
 *  Size, thickness, colour and what is shown come from the settings; its place is the top of the left column (see Home). */
function ClockView ({now, format, config, plate}: {now: Date; format: TimeFormat; config: ClockConfig; plate?: PlateLook}) {
	const {time, period} = timeParts(now, format);
	const size = config.size;
	const info = colorInfo(config.color);
	const style: Record<string, string> = {fontFamily: fontFamily(config.font), '--clock-accent': info.accent};
	Object.assign(style, plateVars(plate));
	if (info.gradient) style['--clock-gradient'] = info.gradient; else style.color = info.color as string;
	const classes = ['card', 'clock', info.gradient && 'clock--gradient', info.dark && 'clock--dark'].filter(Boolean).join(' ');
	const lines = dateLines(now);
	const dateSize = Math.round(32 * config.dateSize / 100);
	// Measured again whenever the clock redraws (once a minute), and once the web fonts have loaded:
	//  - the time's width is that of its digits, rarely a whole number of pixels: rounded up, so the date beside it starts on a whole
	//    pixel (text on half pixels is soft through the TV's 2x scale-up of its 1080p picture);
	//  - the digits are centred in the card by what is really drawn: each font puts its digits at another height in its line, so the
	//    time and the date are moved up or down together by whole pixels (--clock-shift; the card keeps its height).
	const timeEl = useRef<HTMLDivElement>(null);
	const baseEl = useRef<HTMLSpanElement>(null);
	const fit = useRef<() => void>(() => undefined);
	fit.current = () => {
		const el = timeEl.current;
		const card = el && (el.parentElement as HTMLElement | null);
		if (!el || !card) return;
		el.style.width = '';
		el.style.width = `${Math.ceil(el.getBoundingClientRect().width)}px`;
		card.style.removeProperty('--clock-shift');
		const base = baseEl.current;
		const ink = base ? digitInk(time, `${config.weight} ${size}px ${fontFamily(config.font)}`) : null;
		if (!base || !ink) return;
		const box = card.getBoundingClientRect();
		const timeBox = el.getBoundingClientRect();
		const cs = window.getComputedStyle(card);
		const top = parseFloat(cs.paddingTop) || 0, bottom = parseFloat(cs.paddingBottom) || 0;
		if (Math.abs(box.height - top - bottom - timeBox.height) > 1) return;     // the date is taller than the time: it sets the height
		const baseline = base.getBoundingClientRect().bottom;
		const above = baseline - ink.ascent - box.top, below = box.bottom - baseline - ink.descent;
		const shift = Math.round((below - above) / 2);
		if (shift) card.style.setProperty('--clock-shift', `${shift}px`);
	};
	useLayoutEffect(() => fit.current());
	useLayoutEffect(() => {
		const fonts = (document as unknown as {fonts?: {ready?: Promise<unknown>}}).fonts;
		if (fonts && fonts.ready) fonts.ready.then(() => fit.current(), () => undefined);
	}, []);
	return (
		<div class={classes} style={style}>
			<Frost plate={plate} />
			<div class="clock__time" ref={timeEl} style={{fontSize: `${size}px`, fontWeight: config.weight, letterSpacing: `${Math.round(size * 0.03)}px`}}>
				{time}
				<span class="clock__base" ref={baseEl} />
				{period && (
					<span class="clock__period" style={{fontSize: `${Math.round(size * 0.27)}px`, marginLeft: `${Math.round(size * 0.12)}px`}}>{period}</span>
				)}
			</div>
			{config.showDate && (
				<div class="clock__date" style={{fontSize: `${dateSize}px`, lineHeight: `${Math.round(dateSize * 1.2)}px`, marginLeft: `${Math.round(size * 0.2)}px`, marginTop: `${Math.max(0, digitsTop(config.font, size) - Math.round(dateSize * 0.24))}px`}}>
					<div class="clock__day">{lines.day}</div>
					<div>{lines.date}</div>
				</div>
			)}
		</div>
	);
}

export const Clock = memoProps(ClockView);

let inkCanvas: HTMLCanvasElement | null = null;

/** How far the drawn digits reach above and below the baseline, in px (null where the TV's canvas can't tell: older web views). */
function digitInk (text: string, font: string): {ascent: number; descent: number} | null {
	try {
		inkCanvas = inkCanvas || document.createElement('canvas');
		const context = inkCanvas.getContext('2d');
		if (!context) return null;
		context.font = font;
		const m = context.measureText(text);
		if (typeof m.actualBoundingBoxAscent !== 'number' || !(m.actualBoundingBoxAscent > 0)) return null;
		return {ascent: m.actualBoundingBoxAscent, descent: Math.max(0, m.actualBoundingBoxDescent || 0)};
	} catch { return null; }
}
