// The language of the screens: English or Russian. "auto" follows the TV's own language (Russian when the TV is set to Russian,
// English otherwise). Every text is written where it is used, as a pair: tr('Today', 'Сегодня').

export type Lang = 'en' | 'ru';
export type LangSetting = 'auto' | Lang;

export const LANG_SETTINGS: readonly LangSetting[] = ['auto', 'en', 'ru'];

function system (): Lang {
	try {
		const langs = (navigator.languages && navigator.languages.length ? navigator.languages : [navigator.language]) || [];
		return String(langs[0] || '').toLowerCase().indexOf('ru') === 0 ? 'ru' : 'en';
	} catch {
		return 'en';
	}
}

let current: Lang = system();

/** Switches the language (from the settings, before the screens draw). */
export function setLang (setting: LangSetting): void {
	current = setting === 'auto' ? system() : setting;
}

export function lang (): Lang {
	return current;
}

/** The text in the current language. */
export function tr (en: string, ru: string): string {
	return current === 'ru' ? ru : en;
}

/** The right Russian form of a counted noun: plural(2, ['день', 'дня', 'дней']) → "дня". */
export function plural (n: number, forms: [string, string, string]): string {
	const a = Math.abs(n) % 100, b = a % 10;
	if (a > 10 && a < 20) return forms[2];
	if (b > 1 && b < 5) return forms[1];
	return b === 1 ? forms[0] : forms[2];
}

/** A number with its noun: count(3, ['day', 'days'], ['день', 'дня', 'дней']) → "3 days" / "3 дня". */
export function count (n: number, en: [string, string], ru: [string, string, string]): string {
	return `${n} ${current === 'ru' ? plural(n, ru) : Math.abs(n) === 1 ? en[0] : en[1]}`;
}

/** The name of a language setting, in that language (so it can be found whatever the screen shows). */
export function langName (setting: LangSetting): string {
	return setting === 'en' ? 'English' : setting === 'ru' ? 'Русский' : tr('Same as the TV', 'Как на ТВ');
}
