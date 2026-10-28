// OLED care (Settings → Home screen → Pixel shift): the clock, the cards and the apps move a few pixels once a minute
// round a small loop, so no pixel shows the same edge of a digit or an icon for hours. The idea and the loop are
// Hearth's (rudra-avatar/Hearth, from iulyax27's pull request #10). The step comes from the clock the home screen
// already redraws once a minute, so it costs no timer and no extra redraw. Whole pixels only: a fractional move
// would blur every edge through the TV's 2x scale-up of its 1080p picture.

const STEPS: readonly {x: number; y: number}[] = [
	{x: 0, y: 0}, {x: 4, y: 2}, {x: 8, y: 0}, {x: 4, y: -2}, {x: 0, y: -4}, {x: -4, y: -2}, {x: -8, y: 0}, {x: -4, y: 2}
];

/** The offset in px (at 1080p) for this minute. */
export function shiftAt (now: Date): {x: number; y: number} {
	const minute = Math.floor(now.getTime() / 60000);
	return STEPS[((minute % STEPS.length) + STEPS.length) % STEPS.length];
}
