import {useEffect, useState} from 'preact/hooks';
import {TIMING} from '../config/constants';

/** Current time. It is checked every second, but the screen is only redrawn when the minute changes (nothing shows
 *  seconds): redrawing the whole home screen every second kept the TV's processor busy and made the remote react late. */
export function useClock (paused = false): Date {
	const [now, setNow] = useState(() => new Date());
	useEffect(() => {
		if (paused) return undefined;
		const update = () => {
			const next = new Date();
			setNow((prev) => (next.getMinutes() === prev.getMinutes() && next.getHours() === prev.getHours() && next.getDate() === prev.getDate() ? prev : next));
		};
		update();
		const id = window.setInterval(update, TIMING.clockTickMs);
		return () => window.clearInterval(id);
	}, [paused]);
	return now;
}
