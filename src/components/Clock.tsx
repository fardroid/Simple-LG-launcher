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
	return (
		<div class={classes} style={style}>
			<Frost plate={plate} />
			<div class="clock__time" style={{fontSize: `${size}px`, fontWeight: config.weight, letterSpacing: `${(size * 0.03).toFixed(1)}px`}}>
				{time}
				{period && (
					<span class="clock__period" style={{fontSize: `${Math.round(size * 0.27)}px`, marginLeft: `${Math.round(size * 0.12)}px`}}>{period}</span>
				)}
			</div>
			{config.showDate && (
				<div class="clock__date" style={{fontSize: `${dateSize}px`, marginLeft: `${Math.round(size * 0.2)}px`, marginTop: `${Math.max(0, digitsTop(config.font, size) - Math.round(dateSize * 0.24))}px`}}>
					<div class="clock__day">{lines.day}</div>
					<div>{lines.date}</div>
				</div>
			)}
		</div>
	);
}

export const Clock = memoProps(ClockView);
