import {plateVars, type PlateLook} from '../lib/plates';
import {Frost} from './Frost';
import {memoProps} from '../lib/memoprops';
import type {WeatherView} from '../lib/weather';
import {WeatherGlyph} from './Glyph';

/** The curve of today's temperature, one point every 3 hours, with a dashed line at the present moment. It is drawn at the
 *  card's own width (816 px inside the wide card), so its text keeps its size in a narrow column. */
const TOP = 46;
const BOTTOM = 108;
const EDGE = 50;

function Curve ({view, now, W}: {view: WeatherView; now: Date; W: number}) {
	const hours = view.hours;
	if (hours.length < 2) return null;
	const temps = hours.map((h) => h.tempC);
	const lo = Math.min.apply(null, temps);
	const hi = Math.max.apply(null, temps);
	const span = Math.max(hi - lo, 3);
	const points = hours.map((h, i) => ({
		x: EDGE + (W - 2 * EDGE) * i / (hours.length - 1),
		y: BOTTOM - (h.tempC - lo) / span * (BOTTOM - TOP),
		h
	}));
	let path = `M${points[0].x} ${points[0].y}`;
	for (let i = 1; i < points.length; i++) {
		const a = points[i - 1], b = points[i], cx = (a.x + b.x) / 2;
		path += ` C${cx} ${a.y} ${cx} ${b.y} ${b.x} ${b.y}`;
	}
	const first = hours[0].hour, last = hours[hours.length - 1].hour;
	const hourNow = now.getHours() + now.getMinutes() / 60;
	const nowX = EDGE + (W - 2 * EDGE) * Math.min(1, Math.max(0, (hourNow - first) / (last - first)));
	const isToday = view.day === 0;
	return (
		<svg class="wx-curve" viewBox={`0 0 ${W} 150`} aria-hidden="true">
			<defs>
				<linearGradient id="wxfade" x1="0" y1="0" x2="0" y2="1">
					<stop offset="0" stop-color="#ffc36b" stop-opacity="0.34" />
					<stop offset="1" stop-color="#ffc36b" stop-opacity="0" />
				</linearGradient>
			</defs>
			<path d={`${path} L${points[points.length - 1].x} 118 L${points[0].x} 118Z`} fill="url(#wxfade)" opacity="0.5" />
			<path d={path} fill="none" stroke="#ffc36b" stroke-width="3.5" stroke-linecap="round" />
			{isToday && <line x1={nowX} x2={nowX} y1="22" y2="120" stroke="#ffffff" stroke-width="2" stroke-dasharray="4 5" opacity="0.55" />}
			{points.map((p) => (
				<g key={p.h.hour}>
					<circle cx={p.x} cy={p.y} r="6" fill="#0d1534" stroke="#ffc36b" stroke-width="3.5" />
					<text class="wx-curve__temp" x={p.x} y={p.y - 14} text-anchor="middle">{p.h.temp}</text>
					<text class="wx-curve__hour" x={p.x} y="144" text-anchor="middle">{p.h.label}</text>
				</g>
			))}
		</svg>
	);
}

interface Props {
	view: WeatherView;
	/** text size, % of the standard */
	size: number;
	now: Date;
	plate?: PlateLook;
	/** the TV remote is on this card: ← → choose the day */
	focused: boolean;
	/** a day was clicked (index in the forecast) */
	onPick: (day: number) => void;
	/** the card's width on the screen, px */
	width?: number;
}

/** Weather: now, the chosen day by hours, and the next days. ← → (or a click) choose which day the curve is for. */
function WeatherCardView ({view, size, now, plate, focused, onPick, width = 880}: Props) {
	return (
		<div class={`card weather${focused ? ' weather--focus' : ''}`} style={{'--k': String(size / 100), ...plateVars(plate)}}>
			<Frost plate={plate} />
			<div class="weather__now">
				<WeatherGlyph icon={view.icon} class="weather__now-icon" />
				<div class="weather__temp">{view.temp}</div>
				<div class="weather__text">
					<div class="weather__main">{view.text}</div>
					{view.detail && <div class="weather__detail">{view.detail}</div>}
				</div>
				<div class="weather__place">
					{view.city && <div class="weather__city">{view.city}</div>}
				</div>
			</div>
			<div class="weather__hours"><Curve view={view} now={now} W={width - 64} /></div>
			<div class="weather__days">
				{view.days.map((day, i) => (
					<div key={i} class={`wday${day.today ? ' wday--today' : ''}${day.selected ? ' wday--sel' : ''}`} onClick={() => onPick(i)}>
						<div class="wday__name">{day.label}</div>
						<WeatherGlyph icon={day.icon} class="wday__icon" />
						<div class="wday__high">{day.high}</div>
						<div class="wday__low">{day.low}</div>
					</div>
				))}
			</div>
		</div>
	);
}

export const WeatherCard = memoProps(WeatherCardView);
