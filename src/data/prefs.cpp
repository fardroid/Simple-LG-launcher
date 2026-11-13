#include "data/prefs.h"

#include "data/feeds.h"
#include "data/homemode.h"
#include "i18n/i18n.h"
#include "ui/themes.inc"

#include <algorithm>
#include <memory>

const std::vector<int> INTERVAL_VALUES = {10, 30, 60, 300, 900, 1800, 3600};
const std::vector<std::string> CLOCK_FONT_IDS = {"inter", "tv", "poppins", "oswald", "mono", "serif"};
const std::vector<std::string> CLOCK_COLOR_IDS = {"white", "warm", "blue", "amber", "grey", "charcoal", "black", "ember", "ocean", "dusk"};

namespace {

using Get = std::function<bool(const UserConfig &)>;
template <class T> using Getter = std::function<T(const UserConfig &)>;
template <class T> using Setter = std::function<UserConfig(UserConfig, T)>;

int delta (Press p) { return p == Press::left ? -1 : 1; }

std::vector<Pref> group (const std::string &heading, std::vector<Pref> rows) {
	for (auto &p : rows) p.group = heading;
	return rows;
}
Pref only (Get when, Pref p) { p.when = when; return p; }

std::string mode_short (const std::string &mode) {
	return mode.empty() ? tr("Unknown", "Неизвестно") : mode == "default" ? tr("Home app", "Штатно") : mode == "mount" ? tr("Swap", "Подмена") : mode == "helper" ? tr("Helper", "Помощник") : tr("Off", "Выкл.");
}

Pref toggle (const char *id, const char *tab, std::string label, std::string description, Getter<bool> get, Setter<bool> set) {
	Pref p;
	p.id = id; p.tab = tab; p.label = label; p.description = description; p.kind = "toggle";
	p.on = get;
	p.change = [get, set] (const UserConfig &c, Press press) { return press == Press::ok ? set(c, !get(c)) : c; };   // arrows leave switches alone
	return p;
}

template <class T>
Pref choice (const char *id, const char *tab, std::string label, std::string description, std::vector<T> values, Getter<T> get, Setter<T> set, std::function<std::string(T)> text) {
	Pref p;
	p.id = id; p.tab = tab; p.label = label; p.description = description; p.kind = "choice";
	p.text = [get, text] (const UserConfig &c, const SystemInfo &) { return text(get(c)); };
	p.change = [values, get, set] (const UserConfig &c, Press press) { return set(c, step_value(values, get(c), delta(press), press == Press::ok)); };
	return p;
}

Pref range (const char *id, const char *tab, std::string label, std::string description, const Range &r, Getter<int> get, Setter<int> set, std::function<std::string(int)> text) {
	return choice<int>(id, tab, label, description, range_values(r), get, set, text);
}

Pref action (const char *id, const char *tab, std::string label, std::string description, std::function<std::string(const UserConfig &, const SystemInfo &)> text, bool opens = true) {
	Pref p;
	p.id = id; p.tab = tab; p.label = label; p.description = description; p.kind = "action"; p.opens = opens;
	p.text = text;
	return p;
}

std::string pct (int v) { return std::to_string(v) + "%"; }
std::string px (int v) { return std::to_string(v) + " px"; }

std::string base_name (const std::string &path) {
	std::string p = path;
	while (p.size() > 1 && p.back() == '/') p.pop_back();
	size_t slash = p.rfind('/');
	return slash == std::string::npos ? p : p.substr(slash + 1);
}

std::string labels_of (const UserConfig &c) { return !c.showLabels ? "none" : c.labelsSelectedOnly ? "selected" : "all"; }
std::string labels_text (std::string v) { return v == "none" ? tr("None", "Нет") : v == "selected" ? tr("Selected only", "Только у выбранной") : tr("All", "У всех"); }
std::string shift_text (std::string v) { return v == "on" ? tr("On", "Вкл.") : v == "off" ? tr("Off", "Выкл.") : tr("Auto", "Авто"); }
std::string dim_text (std::string v) { return v == "night" ? tr("At night", "Ночью") : v == "always" ? tr("Always", "Всегда") : tr("Off", "Выкл."); }
std::string plate_text (std::string v) { return v == "light" ? tr("Light", "Светлые") : v == "dark" ? tr("Dark", "Тёмные") : tr("Auto", "Авто"); }
std::string theme_text (std::string v) { const ThemeColors &t = theme_colors(v); return tr(t.en, t.ru); }
std::vector<std::string> theme_list () {
	std::vector<std::string> out;
	for (const auto &t : THEMES) out.push_back(t.id);
	return out;
}
std::string format_text (std::string v) { return v == "12h" ? tr("12-hour (AM/PM)", "12-часовой (AM/PM)") : v == "24h" ? tr("24-hour", "24-часовой") : tr("Same as the TV", "Как на ТВ"); }
std::string weight_text (int v) {
	return v == 300 ? tr("Light", "Лёгкий", "Light (weight)") : v == 400 ? tr("Regular", "Обычный") : v == 500 ? tr("Medium", "Средний") : v == 600 ? tr("Bold", "Жирный") : tr("Thin", "Тонкий");
}

using SV = std::vector<std::string>;

std::vector<Pref> weather_prefs () {
	return group(tr("Weather", "Погода"), {
		toggle("weatherOn", "widgets", tr("Show the weather", "Показывать погоду"), tr("The temperature and the forecast, on their own card.", "Температура и погода под датой."),
			[] (const UserConfig &c) { return c.weather.on; }, [] (UserConfig c, bool v) { c.weather.on = v && c.weather.city.has_value(); return c; }),
		action("weatherCity", "widgets", tr("City", "Город"), tr("Pick a city from the list, or search for any place by name. Needs the internet.", "Выберите город из списка или найдите любое место по названию. Нужен интернет."),
			[] (const UserConfig &c, const SystemInfo &) { return c.weather.city ? c.weather.city->name : tr("Not chosen", "Не выбран"); }),
		choice<std::string>("weatherUnit", "widgets", tr("Temperature unit", "Единицы температуры"), tr("Degrees Celsius or Fahrenheit.", "Градусы Цельсия или Фаренгейта."),
			SV{"c", "f"}, [] (const UserConfig &c) { return c.weather.unit; }, [] (UserConfig c, std::string v) { c.weather.unit = v; return c; },
			[] (std::string v) { return v == "f" ? tr("Fahrenheit (°F)", "Фаренгейт (°F)") : tr("Celsius (°C)", "Цельсий (°C)"); })
	});
}

std::vector<Pref> calendar_prefs () {
	return group(tr("Calendar", "Календарь"), {
		toggle("calendarOn", "widgets", tr("Show the calendar", "Показывать календарь"), tr("The day's events in the top right of the home screen.", "События дня в правом верхнем углу главного экрана."),
			[] (const UserConfig &c) { return c.calendar.on; }, [] (UserConfig c, bool v) { c.calendar.on = v && !c.calendar.sources.empty(); return c; }),
		action("calendarUrl", "widgets", tr("Calendars", "Календари"), tr("iCal links from Google Calendar, .ics files or a whole folder. Several calendars can be shown at once.", "Ссылки в формате iCal из Google Календаря, файлы .ics или целая папка. Можно подключить несколько календарей сразу."),
			[] (const UserConfig &c, const SystemInfo &) { return !c.calendar.sources.empty() ? count((long)c.calendar.sources.size(), "calendar", "calendars", "календарь", "календаря", "календарей") : tr("None", "Не задан", "None (calendars)"); })
	});
}

std::vector<Pref> news_prefs () {
	return group(tr("News", "Новости"), {
		toggle("newsOn", "widgets", tr("Show the news", "Показывать новости"), tr("Headlines on their own card. OK on a headline shows it with a QR code to read it on your phone. The TV reads most news sites through the Homebrew Channel (root).", "Заголовки на отдельной карточке. OK на заголовке показывает его с QR-кодом, чтобы дочитать на телефоне. Большинство новостных сайтов ТВ читает через Homebrew Channel (нужен рут)."),
			[] (const UserConfig &c) { return c.news.on; }, [] (UserConfig c, bool v) { c.news.on = v && (c.news.google || !c.news.feeds.empty()); return c; }),
		toggle("newsGoogle", "widgets", tr("Google News", "Google Новости"), tr("The top stories of the chosen country, in the launcher's language when the country has an edition in it.", "Главные новости выбранной страны — на языке лаунчера, если у страны есть такая редакция."),
			[] (const UserConfig &c) { return c.news.google; }, [] (UserConfig c, bool v) { c.news.google = v; c.news.on = c.news.on && (v || !c.news.feeds.empty()); return c; }),
		only([] (const UserConfig &c) { return c.news.google; },
			action("newsCountry", "widgets", tr("Country", "Страна"), tr("Whose top stories Google News shows. At first, the TV's country, or the country of the launcher's language.", "Чьи главные новости показывает Google Новости. Сначала — страна ТВ или страна языка лаунчера."),
				[] (const UserConfig &c, const SystemInfo &info) { return country_name(country_of(c.news.country, info.tv_tag, lang()), lang_tag()); })),
		action("newsFeeds", "widgets", tr("Your feeds", "Свои ленты"), tr("Addresses of RSS or Atom feeds of any site, shown together with Google News or instead of it.", "Адреса RSS- или Atom-лент любых сайтов. Показываются вместе с Google Новостями или вместо них."),
			[] (const UserConfig &c, const SystemInfo &) { return !c.news.feeds.empty() ? count((long)c.news.feeds.size(), "feed", "feeds", "лента", "ленты", "лент") : tr("None", "Нет", "None (feeds)"); })
	});
}

std::vector<Pref> quote_prefs () {
	return group(tr("Quote of the day", "Фраза дня"), {
		toggle("quoteOn", "widgets", tr("Show the quote of the day", "Показывать фразу дня"), tr("A quote from Wikiquote in the launcher's language, a new one every day.", "Цитата из Викицитатника на языке лаунчера, каждый день новая."),
			[] (const UserConfig &c) { return c.quote.on; }, [] (UserConfig c, bool v) { c.quote.on = v; return c; })
	});
}

void append (std::vector<Pref> &out, const std::vector<Pref> &more) { out.insert(out.end(), more.begin(), more.end()); }

std::vector<Pref> base_prefs () {
	std::vector<Pref> out;
	Get slideshow = [] (const UserConfig &c) { return c.wallpaper.mode == "folder" || c.wallpaper.mode == "pictures"; };
	// ----- Appearance -----
	out.push_back(choice<std::string>("theme", "look", tr("Colour theme", "Цветовая тема"), tr("The background when there is no picture, the colours of the text, the cards and the app tiles, and of these screens. OLED black leaves unused pixels off.", "Фон без картинки, цвета текста, виджетов, плиток приложений и этих экранов. Чёрная тема не зажигает лишние пиксели OLED."),
		theme_list(), [] (const UserConfig &c) { return c.theme; }, [] (UserConfig c, std::string v) { c.theme = v; return c; }, theme_text));
	append(out, group(tr("App row", "Ряд приложений"), {
		range("iconScale", "look", tr("Icon size", "Размер значков"), tr("How big the app tiles are.", "Насколько крупные плитки приложений."),
			ranges::iconScale, [] (const UserConfig &c) { return c.iconScale; }, [] (UserConfig c, int v) { c.iconScale = v; return c; }, pct),
		choice<std::string>("showLabels", "look", tr("App names", "Названия приложений"), tr("A name under each tile, only under the selected one, or none (only the icons).", "Подпись под каждой плиткой, только под выбранной или никакой (только значки)."),
			SV{"all", "selected", "none"}, labels_of, [] (UserConfig c, std::string v) { c.showLabels = v != "none"; c.labelsSelectedOnly = v == "selected"; return c; }, labels_text),
		range("sideMargin", "look", tr("Side margin", "Поля по бокам"), tr("Space left and right of the row.", "Отступ слева и справа от ряда."),
			ranges::sideMargin, [] (const UserConfig &c) { return c.sideMargin; }, [] (UserConfig c, int v) { c.sideMargin = v; return c; }, px),
		range("bottomMargin", "look", tr("Bottom margin", "Поле снизу"), tr("Space below the row.", "Отступ под рядом."),
			ranges::bottomMargin, [] (const UserConfig &c) { return c.bottomMargin; }, [] (UserConfig c, int v) { c.bottomMargin = v; return c; }, px)
	}));
	append(out, group(tr("Widgets", "Виджеты"), {
		choice<std::string>("clockFont", "look", tr("Font", "Шрифт"), tr("For the clock, the date, the weather and the calendar. \"TV font\" is built into the TV; the others come with the launcher.", "Для часов, даты, погоды и календаря. «Шрифт ТВ» встроен в телевизор, остальные идут с лаунчером."),
			CLOCK_FONT_IDS, [] (const UserConfig &c) { return c.clock.font; }, [] (UserConfig c, std::string v) { c.clock.font = v; return c; }, font_label),
		range("cardSize", "look", tr("Text size", "Размер текста"), tr("All the text on the weather and calendar cards.", "Весь текст на виджетах погоды и календаря."),
			ranges::cardSize, [] (const UserConfig &c) { return c.cardSize; }, [] (UserConfig c, int v) { c.cardSize = v; return c; }, pct),
		choice<std::string>("plates", "look", tr("Card colour", "Цвет виджетов"), tr("Auto: all the cards turn light or dark together, depending on the picture behind them, so the text stays readable on each.", "Авто: все виджеты вместе становятся светлыми или тёмными в зависимости от картинки за ними, чтобы текст хорошо читался на каждом."),
			SV{"auto", "light", "dark"}, [] (const UserConfig &c) { return c.plates; }, [] (UserConfig c, std::string v) { c.plates = v; return c; }, plate_text),
		only([] (const UserConfig &c) { return c.wallpaper.mode != "gradient"; },
			toggle("glass", "look", tr("Frosted glass", "Матовое стекло"), tr("The cards blur the picture behind them.", "Виджеты размывают картинку под собой."),
				[] (const UserConfig &c) { return c.glass; }, [] (UserConfig c, bool v) { c.glass = v; return c; })),
		only([] (const UserConfig &c) { return c.wallpaper.mode != "gradient" && c.glass; },
			range("frost", "look", tr("Frost", "Матовость"), tr("How much the glass blurs the picture behind it: 100% is the most, 30% lets most of the picture through.", "Насколько стекло размывает картинку под собой: 100% — сильнее всего, на 30% картинка видна чётче всего."),
				ranges::frost, [] (const UserConfig &c) { return c.frost; }, [] (UserConfig c, int v) { c.frost = v; return c; }, pct)),
		toggle("shadows", "look", tr("Shadows", "Тени"), tr("A soft shadow under the clock and the cards. Without it the TV has a little less to draw.", "Мягкая тень под часами и виджетами. Без неё ТВ рисует экран чуть легче."),
			[] (const UserConfig &c) { return c.shadows; }, [] (UserConfig c, bool v) { c.shadows = v; return c; }),
		choice<std::string>("pixelShift", "look", tr("Pixel shift (OLED)", "Сдвиг пикселей (OLED)"), tr("The clock, the cards and the apps move a few pixels once a minute, so an OLED screen does not keep the same picture on the same pixels for hours. Auto: on when the TV is an OLED (by its model name).", "Часы, виджеты и приложения раз в минуту сдвигаются на несколько пикселей, чтобы OLED-экран часами не держал одно и то же изображение на одних пикселях. Авто: включается, если ТВ — OLED (по названию модели)."),
			SV{"auto", "on", "off"}, [] (const UserConfig &c) { return c.pixelShift; }, [] (UserConfig c, std::string v) { c.pixelShift = v; return c; }, shift_text)
	}));

	// ----- Widgets: the clock, the weather and the calendar -----
	append(out, group(tr("Clock", "Часы"), {
		choice<std::string>("clockFormat", "widgets", tr("Format", "Формат"), tr("12-hour, 24-hour, or the same as the TV.", "12 или 24 часа, или как на ТВ."),
			SV{"24h", "12h", "tv"}, [] (const UserConfig &c) { return c.clockFormat; }, [] (UserConfig c, std::string v) { c.clockFormat = v; return c; }, format_text),
		range("clockSize", "widgets", tr("Size", "Размер"), tr("The height of the digits.", "Высота цифр."),
			ranges::clockSize, [] (const UserConfig &c) { return c.clock.size; }, [] (UserConfig c, int v) { c.clock.size = v; return c; }, px),
		choice<int>("clockWeight", "widgets", tr("Weight", "Толщина"), tr("From thin to bold digits.", "От тонких до жирных цифр."),
			{200, 300, 400, 500, 600}, [] (const UserConfig &c) { return c.clock.weight; }, [] (UserConfig c, int v) { c.clock.weight = v; return c; }, weight_text),
		choice<std::string>("clockColor", "widgets", tr("Colour", "Цвет"), tr("White is hard to read on a light picture: pick a dark colour or a gradient for light wallpapers.", "Белый плохо читается на светлой картинке: для светлых обоев выберите тёмный цвет или градиент."),
			CLOCK_COLOR_IDS, [] (const UserConfig &c) { return c.clock.color; }, [] (UserConfig c, std::string v) { c.clock.color = v; return c; }, color_label)
	}));
	append(out, group(tr("Date", "Дата"), {
		toggle("showDate", "widgets", tr("Show the date", "Показывать дату"), tr("The weekday and the date beside the time.", "День недели и дата рядом со временем."),
			[] (const UserConfig &c) { return c.clock.showDate; }, [] (UserConfig c, bool v) { c.clock.showDate = v; return c; }),
		only([] (const UserConfig &c) { return c.clock.showDate; },
			range("dateSize", "widgets", tr("Date size", "Размер даты"), tr("The size of the weekday and the date.", "Размер дня недели и даты."),
				ranges::dateSize, [] (const UserConfig &c) { return c.clock.dateSize; }, [] (UserConfig c, int v) { c.clock.dateSize = v; return c; }, pct))
	}));

	// ----- Wallpaper -----
	out.push_back(action("wallpaper", "wallpaper", tr("Picture", "Картинка"), tr("One picture, or a folder of pictures as a slideshow, from a USB drive or the TV's storage.", "Одна картинка или папка с картинками для слайд-шоу, с USB-накопителя или из памяти ТВ."),
		[] (const UserConfig &c, const SystemInfo &) {
			return c.wallpaper.mode == "gradient" ? tr("None", "Нет") : c.wallpaper.mode == "pictures" ? count((long)c.wallpaper.paths.size(), "picture", "pictures", "картинка", "картинки", "картинок") : base_name(c.wallpaper.path);
		}));
	append(out, group(tr("Slideshow", "Слайд-шоу"), {
		only(slideshow, choice<int>("interval", "wallpaper", tr("Change every", "Менять каждые"), tr("How long each picture stays on screen.", "Сколько каждая картинка остаётся на экране."),
			INTERVAL_VALUES, [] (const UserConfig &c) { return c.wallpaper.intervalSec; }, [] (UserConfig c, int v) { c.wallpaper.intervalSec = v; return c; }, interval_text)),
		only(slideshow, toggle("shuffle", "wallpaper", tr("Shuffle", "Перемешивать"), tr("Random order. Off: in alphabetical order.", "Случайный порядок. Если выключено, по алфавиту."),
			[] (const UserConfig &c) { return c.wallpaper.shuffle; }, [] (UserConfig c, bool v) { c.wallpaper.shuffle = v; return c; }))
	}));
	Get picture = [] (const UserConfig &c) { return c.wallpaper.mode != "gradient"; };
	append(out, group(tr("Brightness", "Яркость"), {
		only(picture, choice<std::string>("dim", "wallpaper", tr("Dim the picture", "Затемнять картинку"), tr("Darkens the wallpaper, so it is easier on the eyes and the text stands out. At night: from sunset to sunrise in the weather's city (it darkens gradually over the dusk); with no city chosen, from 19:00 to 7:00.", "Делает обои темнее: глазам спокойнее, текст заметнее. Ночью: от заката до рассвета в городе погоды (темнеет постепенно, пока идут сумерки); если город не выбран, с 19:00 до 7:00."),
			SV{"off", "night", "always"}, [] (const UserConfig &c) { return c.wallpaper.dim; }, [] (UserConfig c, std::string v) { c.wallpaper.dim = v; return c; }, dim_text)),
		only([] (const UserConfig &c) { return c.wallpaper.mode != "gradient" && c.wallpaper.dim != "off"; },
			range("dimStrength", "wallpaper", tr("Dimming", "Затемнение"), tr("How much darker the picture gets.", "Насколько картинка становится темнее."),
				ranges::dimStrength, [] (const UserConfig &c) { return c.wallpaper.dimStrength; }, [] (UserConfig c, int v) { c.wallpaper.dimStrength = v; return c; }, pct))
	}));

	// ----- Home screen -----
	out.push_back(choice<std::string>("language", "home", tr("Language", "Язык"), tr("The language of the launcher. \"Same as the TV\" follows the TV's own language.", "Язык лаунчера. «Как на ТВ» следует языку самого телевизора."),
		LANG_SETTINGS, [] (const UserConfig &c) { return c.language; }, [] (UserConfig c, std::string v) { c.language = v; return c; }, lang_name));
	out.push_back(action("resetLook", "home", tr("Reset look and clock", "Сбросить вид и часы"), tr("Sizes, margins, cards and the clock go back to how they were at first. Apps, wallpaper, weather and calendars stay.", "Размеры, поля, карточки и часы вернутся к начальным. Приложения, обои, погода и календари останутся."),
		[] (const UserConfig &, const SystemInfo &) { return std::string(); }, false));
	append(out, group(tr("Opening the launcher", "Запуск лаунчера"), {
		action("homeMethod", "home", tr("How the launcher opens", "Как открывается лаунчер"), tr("Whether the Home button, Back out of the last app and waking the TV open the launcher, and in which way. Each way with its pros and cons.", "Открывают ли лаунчер кнопка Home, выход из последнего приложения и включение ТВ, и каким способом. У каждого способа свои плюсы и минусы."),
			[] (const UserConfig &, const SystemInfo &info) { return mode_short(info.home_mode); }),
		action("homeCleanup", "home", tr("Remove everything from the TV", "Убрать всё с ТВ"), tr("Switches off whichever way opens the launcher and takes off the TV what it put there: the start-up links, the guards, keeping the launcher loaded and the copies of its scripts. The Home button opens LG Home again.",
			"Выключает способ открытия лаунчера и убирает с ТВ всё, что для него ставилось: автозапуск, стражи, удержание лаунчера в памяти и копии его скриптов. Кнопка Home снова открывает LG Home."),
			[] (const UserConfig &, const SystemInfo &) { return std::string(); }, false)
	}));
	Pref about;
	about.id = "about"; about.tab = "home"; about.label = "Simple LG launcher"; about.kind = "info";
	about.description = tr("A native home screen for LG TVs. github.com/fardroid/Simple-LG-launcher", "Нативный главный экран для телевизоров LG. github.com/fardroid/Simple-LG-launcher");
	about.text = [] (const UserConfig &, const SystemInfo &info) { return info.version; };
	Pref check = toggle("checkUpdates", "home", tr("Check for new versions", "Проверять новые версии"),
		tr("Once a day the launcher looks on GitHub for a newer version. It is downloaded and installed only when you ask.", "Раз в день лаунчер ищет на GitHub новую версию. Скачивается и ставится она, только когда вы попросите."),
		[] (const UserConfig &c) { return c.checkUpdates; }, [] (UserConfig c, bool v) { c.checkUpdates = v; return c; });
	// under the setting: a look by hand, then the new version (download it, then install it)
	using Step = UpdateState::Step;
	Pref update = action("update", "home", "", tr("The new version is downloaded from GitHub, checked, and installed the way the Homebrew Channel installs it. The launcher then starts again.",
		"Новая версия скачивается с GitHub, проверяется и ставится так же, как это делает Homebrew Channel. Потом лаунчер запускается заново."),
		[] (const UserConfig &, const SystemInfo &info) {
			const UpdateState &u = info.update;
			if (u.checking) return std::string();
			if (u.step == Step::downloading) return std::to_string((int)(u.progress * 100)) + "%";
			if (u.step == Step::ready) return tr("Install", "Установить");
			if (u.step == Step::available) return tr("Download", "Скачать");
			if (u.step == Step::failed || u.unreachable) return tr("Try again", "Ещё раз");
			return tr("Check", "Проверить");
		}, false);
	update.shown = [] (const UserConfig &c, const SystemInfo &) { return c.checkUpdates; };
	update.label_of = [] (const SystemInfo &info) {
		const UpdateState &u = info.update;
		const std::string &v = u.info.version;
		if (u.checking) return tr("Looking for a new version…", "Ищу новую версию…");
		if (u.step == Step::downloading) return fmt(tr("Downloading version {0}…", "Скачиваю версию {0}…"), {v});
		if (u.step == Step::ready) return fmt(tr("Version {0} is ready to install", "Версия {0} готова к установке"), {v});
		if (u.step == Step::failed) return fmt(tr("Version {0} could not be downloaded", "Не удалось скачать версию {0}"), {v});
		if (u.step == Step::available) return fmt(tr("Version {0} is out", "Вышла версия {0}"), {v});
		if (u.unreachable) return tr("Could not reach GitHub", "Не удалось связаться с GitHub");
		if (u.latest) return tr("This is the latest version", "Установлена последняя версия");
		return tr("Check for a new version", "Проверить наличие новой версии");
	};
	update.progress_of = [] (const SystemInfo &info) { return info.update.step == Step::downloading ? (float)info.update.progress : -1.f; };
	append(out, group(tr("About", "О лаунчере"), {about, check, update}));
	// the last row of all
	Pref restart = action("restartTv", "home", tr("Restart the TV", "Перезагрузить ТВ"), tr("A full restart of the TV, not standby. Takes about a minute.", "Полная перезагрузка телевизора, а не переход в режим ожидания. Занимает около минуты."),
		[] (const UserConfig &, const SystemInfo &) { return std::string(); }, false);
	restart.icon = "restart";
	out.push_back(restart);
	return out;
}

std::string built_lang;
std::vector<Pref> built;

}  // namespace

std::string interval_text (int seconds) {
	if (seconds < 60) return count(seconds, "second", "seconds", "секунда", "секунды", "секунд");
	if (seconds < 3600) return count(seconds / 60, "minute", "minutes", "минута", "минуты", "минут");
	return count(seconds / 3600, "hour", "hours", "час", "часа", "часов");
}

std::string font_label (const std::string &id) {
	if (id == "tv") return tr("TV font", "Шрифт ТВ");
	if (id == "poppins") return tr("Poppins (rounded)", "Poppins (округлый)");
	if (id == "oswald") return tr("Oswald (narrow)", "Oswald (узкий)");
	if (id == "mono") return tr("Roboto Mono (digital)", "Roboto Mono (цифровой)");
	if (id == "serif") return tr("Cormorant (elegant serif)", "Cormorant (изящный с засечками)");
	return tr("Inter (default)", "Inter (по умолчанию)");
}

std::string color_label (const std::string &id) {
	if (id == "warm") return tr("Warm white", "Тёплый белый");
	if (id == "blue") return tr("Ice blue", "Ледяной синий");
	if (id == "amber") return tr("Amber", "Янтарный");
	if (id == "grey") return tr("Grey", "Серый");
	if (id == "charcoal") return tr("Dark grey", "Тёмно-серый");
	if (id == "black") return tr("Black", "Чёрный");
	if (id == "ember") return tr("Gradient: Ember (red to gold)", "Градиент: Ember (красный в золото)");
	if (id == "ocean") return tr("Gradient: Ocean (blue to teal)", "Градиент: Ocean (синий в бирюзовый)");
	if (id == "dusk") return tr("Gradient: Dusk (violet to coral)", "Градиент: Dusk (фиолетовый в коралловый)");
	return tr("White", "Белый");
}

UserConfig reset_look (const UserConfig &c) {
	UserConfig d, out = c;
	out.theme = d.theme;
	out.iconScale = d.iconScale;
	out.showLabels = true;
	out.labelsSelectedOnly = false;
	out.sideMargin = d.sideMargin;
	out.bottomMargin = d.bottomMargin;
	out.cardSize = d.cardSize;
	out.plates = "auto";
	out.glass = true;
	out.frost = d.frost;
	out.shadows = true;
	out.clockFormat = "24h";
	out.clock = ClockConfig();
	return out;
}

const std::vector<Pref> &prefs () {
	if (built.empty() || built_lang != lang()) {
		built_lang = lang();
		built = base_prefs();
		append(built, weather_prefs());
		append(built, calendar_prefs());
		append(built, news_prefs());
		append(built, quote_prefs());
	}
	return built;
}

const Pref *find_pref (const std::string &id) {
	for (auto &p : prefs()) if (p.id == id) return &p;
	return nullptr;
}

std::vector<OptionItem> options_for (const std::string &tab, const UserConfig &c, const SystemInfo &info) {
	std::vector<OptionItem> out;
	for (auto &p : prefs()) {
		if (p.tab != tab || (p.when && !p.when(c)) || (p.shown && !p.shown(c, info))) continue;
		OptionItem o;
		o.id = p.id; o.label = p.label; o.description = p.description; o.group = p.group; o.kind = p.kind; o.opens = p.opens; o.icon = p.icon;
		if (p.kind == "toggle" && p.on) o.value = p.on(c) ? 1 : 0;
		if (p.kind != "toggle" && p.text) o.valueText = p.text(c, info);
		o.stepper = p.kind == "choice";
		if (p.label_of) o.label = p.label_of(info);
		if (p.progress_of) o.progress = p.progress_of(info);
		// the rows that run commands as root, greyed out without it (with why in place of the description)
		if (std::find(ROOT_OPTIONS.begin(), ROOT_OPTIONS.end(), p.id) != ROOT_OPTIONS.end()) o.disabled = root_reason((Root)info.root, info.root_detail);
		// a new version: looked for and downloaded without root, but installed only with it (the Homebrew Channel can install it)
		using Step = UpdateState::Step;
		if (p.id == "update" && (Root)info.root == Root::no && info.update.step != Step::none && !info.update.checking) {
			o.disabled = tr("Installing it here needs a rooted TV. It can be updated in the Homebrew Channel.", "Чтобы поставить её отсюда, нужен рутованный ТВ. Обновить можно в Homebrew Channel.");
			o.valueText.clear();
		}
		out.push_back(o);
	}
	return out;
}

bool same_config (const UserConfig &a, const UserConfig &b) { return config_json(a) == config_json(b); }

UserConfig apply_pref (const UserConfig &c, const std::string &id, Press press, bool *changed) {
	const Pref *p = find_pref(id);
	UserConfig next = p && p->change ? p->change(c, press) : c;
	if (changed) *changed = !same_config(c, next);
	return next;
}
