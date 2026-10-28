import {useLayoutEffect, useRef} from 'preact/hooks';
import {plateVars, type PlateLook} from '../lib/plates';
import {Frost} from './Frost';
import {memoProps} from '../lib/memoprops';
import {tr} from '../lib/i18n';
import type {Quote} from '../lib/quote';

interface Props {
	quote: Quote | null;
	/** nothing has been read yet */
	loading: boolean;
	/** the card's place on the screen: its height (fixed under another card, the whole column when alone) */
	height: number;
	/** text size, % of the standard */
	size: number;
	plate?: PlateLook;
}

/** The biggest the text of a quote gets, at the standard text size, px. */
const MAX_FONT = 44;
const MIN_FONT = 16;

/** The quote of the day: the text as big as the card allows (a short saying is set large, a long one small), the author under it.
 *  The size is found by measuring: from the largest down, two pixels at a time, until it fits. */
function QuoteCardView ({quote, loading, height, size, plate}: Props) {
	const body = useRef<HTMLDivElement>(null);
	const textEl = useRef<HTMLDivElement>(null);
	const k = size / 100;
	const text = quote ? quote.text : '';
	useLayoutEffect(() => {
		const box = body.current, el = textEl.current;
		if (!box || !el) return;
		if (!text) { el.style.fontSize = ''; return; }        // the waiting text keeps the card's own size
		let font = Math.round(Math.min(MAX_FONT * k, height * 0.34));
		const fits = () => box.scrollHeight <= box.clientHeight + 1;
		el.style.fontSize = `${font}px`;
		el.classList.remove('quote__text--clamped');
		while (!fits() && font > MIN_FONT) {
			font -= 2;
			el.style.fontSize = `${font}px`;
		}
		// still too long at the smallest size: the end is cut with an ellipsis
		if (!fits()) el.classList.add('quote__text--clamped');
	}, [text, height, k]);
	return (
		<div class="card quote" style={{'--k': String(k), height: `${height}px`, ...plateVars(plate)}}>
			<Frost plate={plate} />
			<div class="quote__body" ref={body}>
				{quote
					? <div class="quote__text" ref={textEl}>{quote.text}</div>
					: <div class="quote__text quote__text--empty" ref={textEl}>{loading ? tr('Loading the quote of the day…', 'Загружаю фразу дня…') : tr('The quote of the day has not loaded yet.', 'Фраза дня пока не загрузилась.')}</div>}
				{quote && <div class="quote__by">{quote.author} · Wikiquote</div>}
			</div>
		</div>
	);
}

export const QuoteCard = memoProps(QuoteCardView);
