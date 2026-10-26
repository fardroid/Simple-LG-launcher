// Inline SVG line icons used by the Misc row and the settings badge.

export type GlyphName = 'house' | 'gear' | 'tv';

/** Small icons for the wallpaper picker. */
export function PickerIcon ({kind}: {kind: 'folder' | 'picture' | 'drive' | 'slideshow' | 'default' | 'info' | 'search' | 'pin'}) {
	return (
		<svg class="picker__icon" viewBox="0 0 24 24" aria-hidden="true">
			{kind === 'folder' && <path d="M3 7.5h6.2l2 2.3H21v9.7H3z" />}
			{kind === 'picture' && <><rect x="3.5" y="5" width="17" height="14" rx="2" /><circle cx="9" cy="10" r="1.6" /><path d="M4 17l5-4.5 3.5 3L16 12l4 4.5" /></>}
			{kind === 'drive' && <><rect x="7.5" y="3.5" width="9" height="17" rx="2" /><path d="M10.5 7v3M13.5 7v3" /></>}
			{kind === 'slideshow' && <><rect x="6" y="7" width="14" height="11" rx="2" /><path d="M3.5 9.5v8.5a2 2 0 0 0 2 2H16" /></>}
			{kind === 'default' && <><rect x="3.5" y="5" width="17" height="14" rx="2" /><path d="M3.5 15l17-7" /></>}
			{kind === 'info' && <><circle cx="12" cy="12" r="8.5" /><path d="M12 11v5M12 8v.2" /></>}
			{kind === 'search' && <><circle cx="10.5" cy="10.5" r="6" /><path d="M15 15l5 5" /></>}
			{kind === 'pin' && <><path d="M12 21s6.5-6.1 6.5-11A6.5 6.5 0 0 0 5.5 10C5.5 14.9 12 21 12 21z" /><circle cx="12" cy="10" r="2.3" /></>}
		</svg>
	);
}

export function Glyph ({name}: {name: GlyphName}) {
	return (
		<svg class="tile__glyph" viewBox="0 0 24 24" aria-hidden="true">
			{name === 'house'
				? <path d="M3 11.5 12 4l9 7.5M5.5 9.8V20h13V9.8M10 20v-6h4v6" />
				: name === 'tv'
				? <>
					<rect x="2.8" y="4.5" width="18.4" height="12.2" rx="1.8" />
					<path d="M8 20.2h8M12 16.7v3.5M7 8.6h4.6M14.6 8.6H17M7 12.6H9.2M12.2 12.6H17" />
					<circle cx="13.1" cy="8.6" r="1.1" />
					<circle cx="10.7" cy="12.6" r="1.1" />
				</>
				: <>
					<circle cx="12" cy="12" r="3.2" />
					<path d="M12 2.8v2.4M12 18.8v2.4M2.8 12h2.4M18.8 12h2.4M5.5 5.5l1.7 1.7M16.8 16.8l1.7 1.7M5.5 18.5l1.7-1.7M16.8 7.2l1.7-1.7" />
				</>}
		</svg>
	);
}

export function CheckMark () {
	return (
		<svg viewBox="0 0 24 24" aria-hidden="true">
			<path d="M5 12.5l4.5 4.5L19 7.5" />
		</svg>
	);
}

export type WeatherIconName = 'sun' | 'moon' | 'partly' | 'cmoon' | 'cloud' | 'fog' | 'rain' | 'snow' | 'storm';

/** A weather picture (line art, drawn in the text colour; the sun and the moon get their own tint). */
export function WeatherGlyph ({icon, class: extra}: {icon: WeatherIconName; class?: string}) {
	const cloud = 'M7 18.5h9.5a3.7 3.7 0 0 0 .4-7.4 5.3 5.3 0 0 0-10.2 1.3A3.1 3.1 0 0 0 7 18.5z';
	return (
		<svg class={`wx-icon wx-icon--${icon}${extra ? ' ' + extra : ''}`} viewBox="0 0 24 24" aria-hidden="true">
			{icon === 'sun' && <><circle cx="12" cy="12" r="4" /><path d="M12 2.5v2.5M12 19v2.5M2.5 12H5M19 12h2.5M5.3 5.3l1.8 1.8M16.9 16.9l1.8 1.8M5.3 18.7l1.8-1.8M16.9 7.1l1.8-1.8" /></>}
			{icon === 'moon' && <path d="M19.5 14.5A8 8 0 0 1 9.5 4.5a8 8 0 1 0 10 10z" />}
			{icon === 'partly' && <><path d="M8 4.5v1.6M3.3 9H4.9M4.7 5.7l1.1 1.1M11.3 5.7l-1.1 1.1" /><circle cx="8" cy="9" r="2.6" /><path d="M9 19h8.3a3.4 3.4 0 0 0 .3-6.8 4.8 4.8 0 0 0-9.2 1.1A2.9 2.9 0 0 0 9 19z" /></>}
			{icon === 'cmoon' && <><path d="M11.5 3.8A4.6 4.6 0 1 0 15 11a4.6 4.6 0 0 1-3.5-7.2z" /><path d="M9 19.5h8.3a3.4 3.4 0 0 0 .3-6.8 4.8 4.8 0 0 0-9.2 1.1A2.9 2.9 0 0 0 9 19.5z" /></>}
			{icon === 'cloud' && <path d={cloud} />}
			{icon === 'fog' && <><path d="M7 14.5h9.5a3.4 3.4 0 0 0 .3-6.8 4.9 4.9 0 0 0-9.4 1.2A2.8 2.8 0 0 0 7 14.5z" /><path d="M5 18h14M8 21h8" /></>}
			{icon === 'rain' && <><path d="M7 14.5h9.5a3.4 3.4 0 0 0 .3-6.8 4.9 4.9 0 0 0-9.4 1.2A2.8 2.8 0 0 0 7 14.5z" /><path d="M8.5 17.5l-1 2.5M12.5 17.5l-1 2.5M16.5 17.5l-1 2.5" /></>}
			{icon === 'snow' && <><path d="M7 14.5h9.5a3.4 3.4 0 0 0 .3-6.8 4.9 4.9 0 0 0-9.4 1.2A2.8 2.8 0 0 0 7 14.5z" /><path d="M8.5 18v.2M12.5 19.5v.2M16.5 18v.2" /></>}
			{icon === 'storm' && <><path d="M7 14.5h9.5a3.4 3.4 0 0 0 .3-6.8 4.9 4.9 0 0 0-9.4 1.2A2.8 2.8 0 0 0 7 14.5z" /><path d="M12.5 14.5l-2 3.2h3l-2 3.3" /></>}
		</svg>
	);
}
