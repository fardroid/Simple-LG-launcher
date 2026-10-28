import {plateVars, type PlateLook} from '../lib/plates';
import {Frost} from './Frost';
import {memoProps} from '../lib/memoprops';
import {fmt, tr} from '../lib/i18n';
import {ageOf, headlinesPerPage, type FeedItem} from '../lib/feeds';

interface Props {
	items: FeedItem[] | null;
	/** nothing has been read yet */
	loading: boolean;
	/** the page shown (0 = the newest headlines) */
	page: number;
	/** the headline the remote is on (index in `items`), when the card is focused */
	selected: number;
	focused: boolean;
	/** the card's height on the screen, px */
	height: number;
	/** text size, % of the standard */
	size: number;
	now: Date;
	plate?: PlateLook;
	/** a headline was clicked */
	onPick: (index: number) => void;
}

/** "12 min ago", "3 h ago" */
export function ageText (at: number, now: number): string {
	const age = ageOf(at, now);
	if (!age) return '';
	if (age.unit === 'min') return fmt(tr('{0} min ago', '{0} мин назад'), age.n);
	if (age.unit === 'h') return fmt(tr('{0} h ago', '{0} ч назад'), age.n);
	return fmt(tr('{0} d ago', '{0} дн. назад'), age.n);
}

/** The news: a page of headlines, each with its outlet and age. ← → turn the pages, ↑ ↓ choose a headline, OK opens it. */
function NewsCardView ({items, loading, page, selected, focused, height, size, now, plate, onPick}: Props) {
	const k = size / 100;
	const per = headlinesPerPage(height, k);
	const list = items || [];
	const pages = Math.max(1, Math.ceil(list.length / per));
	const p = Math.min(page, pages - 1);
	const shown = list.slice(p * per, p * per + per);
	const nowMs = now.getTime();
	return (
		<div class={`card news${focused ? ' news--focus' : ''}`} style={{'--k': String(k), ...plateVars(plate)}}>
			<Frost plate={plate} />
			<div class="news__head">
				<div class="news__title">{tr('News', 'Новости')}</div>
				{pages > 1 && <div class="news__pages">{p + 1} / {pages}</div>}
			</div>
			<div class="news__list" key={'p' + p}>
				{shown.map((item, i) => {
					const index = p * per + i;
					const age = ageText(item.at, nowMs);
					return (
						<div key={item.link || item.title} class={`news__item${focused && index === selected ? ' news__item--sel' : ''}`} onClick={() => onPick(index)}>
							<div class="news__meta">{[item.source, age].filter((x) => !!x).join(' · ')}</div>
							<div class="news__headline">{item.title}</div>
						</div>
					);
				})}
				{!list.length && <div class="news__empty">{loading ? tr('Loading the news…', 'Загружаю новости…') : items ? tr('No fresh headlines.', 'Свежих новостей нет.') : tr('The news has not loaded yet.', 'Новости пока не загрузились.')}</div>}
			</div>
		</div>
	);
}

export const NewsCard = memoProps(NewsCardView);
