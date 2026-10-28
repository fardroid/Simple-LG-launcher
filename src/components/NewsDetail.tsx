import {useMemo} from 'preact/hooks';
import qrcode from 'qrcode-generator';
import {tr} from '../lib/i18n';
import type {FeedItem} from '../lib/feeds';
import {ageText} from './NewsCard';

/** The squares of a QR code of `text` as one SVG path, and its size in squares (null when it cannot be made: too long). */
function qrPath (text: string): {d: string; n: number} | null {
	try {
		const qr = qrcode(0, 'M');
		qr.addData(text);
		qr.make();
		const n = qr.getModuleCount();
		let d = '';
		for (let y = 0; y < n; y++) for (let x = 0; x < n; x++) if (qr.isDark(y, x)) d += `M${x} ${y}h1v1h-1z`;
		return {d, n};
	} catch { return null; }
}

interface Props {
	item: FeedItem;
	now: Date;
}

/** A headline over the home screen: its outlet and age, the headline, the description from the feed, and a QR code of its link
 *  to read the article on a phone. OK or Back closes it (Home handles the keys). */
export function NewsDetail ({item, now}: Props) {
	const code = useMemo(() => (item.link ? qrPath(item.link) : null), [item.link]);
	const age = ageText(item.at, now.getTime());
	return (
		<div class="news-detail">
			<div class="news-detail__card">
				<div class="news-detail__text">
					<div class="news-detail__meta">{[item.source, age].filter((x) => !!x).join(' · ')}</div>
					<div class="news-detail__title">{item.title}</div>
					{item.summary && <div class="news-detail__summary">{item.summary}</div>}
					<div class="news-detail__hint">{tr('OK or Back closes', 'OK или «Назад» — закрыть')}</div>
				</div>
				{code && (
					<div class="news-detail__qr">
						<svg viewBox={`-2 -2 ${code.n + 4} ${code.n + 4}`} shape-rendering="crispEdges" aria-hidden="true">
							<rect x="-2" y="-2" width={code.n + 4} height={code.n + 4} fill="#fff" />
							<path d={code.d} fill="#000" />
						</svg>
						<div class="news-detail__scan">{tr('Scan to read it on your phone', 'Наведите камеру телефона, чтобы дочитать')}</div>
					</div>
				)}
			</div>
		</div>
	);
}
