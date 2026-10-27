import {useEffect, useLayoutEffect, useRef, useState} from 'preact/hooks';
import {LAYOUT} from '../theme/tokens';

export interface OptionItem {
	id: string;
	label: string;
	/** shown at the foot of the screen while the row is focused */
	description: string;
	/** the heading of its group; rows of one group follow each other */
	group?: string;
	/** info rows only show a fact */
	kind?: 'toggle' | 'choice' | 'action' | 'info';
	/** opens another screen: a chevron points onwards */
	opens?: boolean;
	/** on/off options: shown as a switch */
	value?: boolean;
	/** other options: the current choice, shown as text */
	valueText?: string;
	/** ← → change it; shown with arrows while focused */
	stepper?: boolean;
	/** greyed out: why this can't be used on this TV */
	disabled?: string;
}

interface Props {
	options: OptionItem[];
	focused: boolean;
	selected: number;
	onSelect: (index: number) => void;
	onActivate: (index: number) => void;
}

/** Room kept free below the list, in px (the description of the focused row sits there). */
const BOTTOM_SPACE = 170;
/** Used when the screen cannot be measured. */
const FALLBACK_HEIGHT = 700;

/** The rows in their groups: [heading, rows with their index in `options`]. */
function grouped (options: OptionItem[]): {heading: string; rows: {o: OptionItem; i: number}[]}[] {
	const out: {heading: string; rows: {o: OptionItem; i: number}[]}[] = [];
	options.forEach((o, i) => {
		const heading = o.group || '';
		const last = out[out.length - 1];
		if (last && last.heading === heading) last.rows.push({o, i});
		else out.push({heading, rows: [{o, i}]});
	});
	return out;
}

/** The settings of a section, in rounded groups. Every row is drawn; the list sits in a window as tall as the space
 *  really left on screen (measured, not assumed) and slides so the focused row is always in view. */
export function OptionList ({options, focused, selected, onSelect, onActivate}: Props) {
	const viewRef = useRef<HTMLDivElement>(null);
	const trackRef = useRef<HTMLDivElement>(null);
	const [height, setHeight] = useState(FALLBACK_HEIGHT);
	const [shift, setShift] = useState(0);
	const shiftRef = useRef(0);

	// The window reaches from the top of the list to the space kept for the description.
	useLayoutEffect(() => {
		const measure = () => {
			const view = viewRef.current;
			if (!view) return;
			const screenHeight = Math.min(window.innerHeight || LAYOUT.screenHeight, LAYOUT.screenHeight);
			const room = Math.round(screenHeight - view.getBoundingClientRect().top - BOTTOM_SPACE);
			if (room >= 120) setHeight((h) => (h === room ? h : room));
		};
		measure();
		window.addEventListener('resize', measure);
		return () => window.removeEventListener('resize', measure);
	}, []);

	// Slide the list just far enough to keep the focused row (and, for the first row of a group, its heading) in view.
	useLayoutEffect(() => {
		const track = trackRef.current;
		if (!track) return;
		const row = track.querySelector(`[data-row="${Math.max(0, Math.min(selected, options.length - 1))}"]`) as HTMLElement | null;
		if (!row) return;
		// offsets are all from the track (the nearest positioned element)
		const box = row.parentElement as HTMLElement;
		const group = box.parentElement as HTMLElement;
		const first = box.firstElementChild === row;
		const top = (first ? group.offsetTop : row.offsetTop) - 8;
		const bottom = row.offsetTop + row.offsetHeight + 16;
		let next = shiftRef.current;
		if (top < next) next = Math.max(0, top);
		else if (bottom > next + height) next = bottom - height;
		if (next !== shiftRef.current) { shiftRef.current = next; setShift(next); }
	}, [selected, options, height]);

	// Another section starts at the top.
	const first = options.length ? options[0].id : '';
	useEffect(() => { shiftRef.current = 0; setShift(0); }, [first]);

	return (
		<div class="options" ref={viewRef} style={{height: `${height}px`}}>
			<div class="options__track" ref={trackRef} style={{transform: `translateY(${-shift}px)`}}>
				{grouped(options).map((g, n) => (
					<div class="ogroup" key={g.heading + n}>
						{g.heading && <div class="ogroup__title">{g.heading}</div>}
						<div class="ogroup__box">
							{g.rows.map(({o, i}) => {
								const on = focused && i === selected;
								return (
									<div
										key={o.id}
										data-row={i}
										class={['orow', on && 'orow--focused', o.disabled && 'orow--disabled', o.kind === 'info' && 'orow--info'].filter(Boolean).join(' ')}
										onMouseMove={() => { if (i !== selected || !focused) onSelect(i); }}
										onClick={() => onActivate(i)}
									>
										<div class="orow__label">{o.label}</div>
										{o.value !== undefined
											? <div class={`switch${o.value ? ' switch--on' : ''}`} role="switch" aria-checked={o.value}><div class="switch__knob" /></div>
											: <div class="orow__value">
												{o.stepper && <span class="orow__step">‹</span>}
												<span class="orow__text">{o.valueText}</span>
												{o.stepper && <span class="orow__step">›</span>}
												{o.opens && <span class="orow__go">›</span>}
											</div>}
									</div>
								);
							})}
						</div>
					</div>
				))}
			</div>
		</div>
	);
}
