// The language of the screens. Russian and English are written where they are used, as a pair: tr('Today', 'Сегодня').
// Every other language is a dictionary in ./locales, keyed by the English text; a missing entry falls back to English.
// "auto" follows the TV's own language (English when the TV speaks a language the launcher does not have).

import {es} from './locales/es';
import {fr} from './locales/fr';
import {de} from './locales/de';
import {it} from './locales/it';
import {pt} from './locales/pt';
import {tr as trTR} from './locales/tr';
import {uk} from './locales/uk';
import {zh} from './locales/zh';
import {ja} from './locales/ja';
import {ko} from './locales/ko';
import {pl} from './locales/pl';

export type Lang = 'en' | 'ru' | 'es' | 'fr' | 'de' | 'it' | 'pt' | 'tr' | 'uk' | 'zh' | 'ja' | 'ko' | 'pl';
export type LangSetting = 'auto' | Lang;

/** How a language counts: 'one' = no plural forms, 'slavic' = 1 / 2-4 / 5+ (21 like 1), 'pl' = the same but only 1 itself is singular
 *  (21 dni), 'fr' = 0 and 1 are singular, 'en' = only 1 is. */
type PluralRule = 'none' | 'slavic' | 'pl' | 'fr' | 'en';

interface LangInfo {
	/** The name of the language in itself (so it can be found whatever the screen shows). */
	name: string;
	/** The BCP 47 tag for Intl and for the weather service's place search. */
	tag: string;
	rule: PluralRule;
}

export const LANG_INFO: Record<Lang, LangInfo> = {
	en: {name: 'English', tag: 'en', rule: 'en'},
	ru: {name: 'Русский', tag: 'ru', rule: 'slavic'},
	es: {name: 'Español', tag: 'es', rule: 'en'},
	fr: {name: 'Français', tag: 'fr', rule: 'fr'},
	de: {name: 'Deutsch', tag: 'de', rule: 'en'},
	it: {name: 'Italiano', tag: 'it', rule: 'en'},
	pt: {name: 'Português', tag: 'pt', rule: 'fr'},
	tr: {name: 'Türkçe', tag: 'tr', rule: 'en'},
	uk: {name: 'Українська', tag: 'uk', rule: 'slavic'},
	zh: {name: '简体中文', tag: 'zh-CN', rule: 'none'},
	ja: {name: '日本語', tag: 'ja', rule: 'none'},
	ko: {name: '한국어', tag: 'ko', rule: 'none'},
	pl: {name: 'Polski', tag: 'pl', rule: 'pl'}
};

export const LANG_SETTINGS: readonly LangSetting[] = ['auto', 'en', 'ru', 'es', 'fr', 'de', 'it', 'pt', 'tr', 'uk', 'zh', 'ja', 'ko', 'pl'];

const DICTIONARIES: Partial<Record<Lang, Record<string, string>>> = {es, fr, de, it, pt, tr: trTR, uk, zh, ja, ko, pl};

/** The launcher's language for a language tag such as "pt-BR" or "zh-Hans-CN" (English when it has none). */
export function fromTag (tag: string): Lang {
	const code = String(tag || '').toLowerCase().split(/[-_]/)[0];
	return code in LANG_INFO ? code as Lang : 'en';
}

function system (): Lang {
	try {
		const langs = (navigator.languages && navigator.languages.length ? navigator.languages : [navigator.language]) || [];
		return fromTag(String(langs[0] || ''));
	} catch {
		return 'en';
	}
}

let current: Lang = system();
let dictionary: Record<string, string> | undefined = DICTIONARIES[current];

/** Switches the language (from the settings, before the screens draw). */
export function setLang (setting: LangSetting): void {
	current = setting === 'auto' ? system() : setting;
	dictionary = DICTIONARIES[current];
}

export function lang (): Lang {
	return current;
}

/** The BCP 47 tag of the current language. */
export function langTag (): string {
	return LANG_INFO[current].tag;
}

/**
 * The text in the current language. The dictionaries are keyed by the English text; when the same English words
 * mean different things in two places, `key` tells them apart in the dictionaries (the screen still shows `en`).
 */
export function tr (en: string, ru: string, key?: string): string {
	if (current === 'en') return en;
	if (current === 'ru') return ru;
	const found = dictionary && dictionary[key || en];
	return found === undefined ? en : found;
}

/** Fills {0}, {1}… in a text: fmt(tr('Uninstall {0}?', …), title). */
export function fmt (text: string, ...values: Array<string | number>): string {
	return text.replace(/\{(\d+)\}/g, (whole, i) => (values[+i] === undefined ? whole : String(values[+i])));
}

/** The right Russian form of a counted noun: plural(2, ['день', 'дня', 'дней']) → "дня". */
export function plural (n: number, forms: [string, string, string]): string {
	const a = Math.abs(n) % 100, b = a % 10;
	if (a > 10 && a < 20) return forms[2];
	if (b > 1 && b < 5) return forms[1];
	return b === 1 ? forms[0] : forms[2];
}

/** Which of a language's noun forms a number takes (0 = the first). */
export function formIndex (rule: PluralRule, n: number): number {
	const x = Math.abs(n);
	if (rule === 'none') return 0;
	if (rule === 'slavic') {
		const a = x % 100, b = a % 10;
		return a > 10 && a < 20 ? 2 : b > 1 && b < 5 ? 1 : b === 1 ? 0 : 2;
	}
	if (rule === 'pl') {
		const a = x % 100, b = a % 10;
		return x === 1 ? 0 : b > 1 && b < 5 && (a < 12 || a > 14) ? 1 : 2;
	}
	if (rule === 'fr') return x < 2 ? 0 : 1;
	return x === 1 ? 0 : 1;
}

/**
 * A number with its noun: count(3, ['day', 'days'], ['день', 'дня', 'дней']) → "3 days" / "3 дня".
 * The other languages keep the noun's forms in their dictionary under the English plural, joined with "|".
 */
export function count (n: number, en: [string, string], ru: [string, string, string]): string {
	if (current === 'ru') return `${n} ${plural(n, ru)}`;
	if (current === 'en') return `${n} ${Math.abs(n) === 1 ? en[0] : en[1]}`;
	const forms = ((dictionary && dictionary[en[1]]) || en[1]).split('|');
	const rule = LANG_INFO[current].rule;
	return `${n}${rule === 'none' ? '' : ' '}${forms[Math.min(formIndex(rule, n), forms.length - 1)]}`;
}

/** The name of a language setting, in that language (so it can be found whatever the screen shows). */
export function langName (setting: LangSetting): string {
	return setting === 'auto' ? tr('Same as the TV', 'Как на ТВ') : LANG_INFO[setting].name;
}
