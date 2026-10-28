import {useCallback, useEffect, useMemo, useRef, useState} from 'preact/hooks';
import {Home, type Appearance} from './screens/Home';
import {Settings, type SettingsTab} from './screens/Settings';
import {Picker, type WallpaperChoice} from './screens/Picker';
import {CityPicker} from './screens/CityPicker';
import {HomeMethod} from './screens/HomeMethod';
import {detectOled, knownOled} from './lib/panel';
import {applySplashMode, noteSplashMode} from './lib/splash';
import {readWallpaperCache, readWallpaperLook, wallpaperCacheId} from './lib/wallpaper';
import {reloadIfUpdated} from './lib/update';
import {readHomeMode, switchHomeMode, modeText, type HomeMode} from './lib/homemode';
import {Toast, type ToastMessage} from './components/Toast';
import {Wallpaper} from './components/Wallpaper';
import type {TileModel} from './components/Tile';
import {cacheApps, cachedApps, listApps, probeRoot, type AppEntry} from './lib/apps';
import {lastFailureDetail, rootState, whySuffix} from './lib/failure';
import {keepKnownIcons, sameApps} from './lib/appcache';
import type {TimeFormat} from './lib/clock';
import {count, fmt, lang, setLang, tr} from './lib/i18n';
import {listInputs, mockInputs, type InputSource} from './lib/inputs';
import {launchApp, launchLgHome, launchSystemSettings} from './lib/launch';
import {isWebOS} from './lib/luna';
import {hidePointerOnForeground} from './lib/pointer';
import {closeLgHome, takeoverActive} from './lib/system';
import {applyHomeHook, getHookStatus, hookMessage, installHomeHook, logLine, markAliveSoon, reapplyHomeHook, removeHomeHook, stockHome} from './lib/homehook';
import {applyPref, disabledReason, findPref, optionsFor, resetLook, type Press, type SystemInfo} from './lib/prefs';
import {hasStoredConfig, loadConfig, parseConfig, saveConfig, toggleId, DEFAULT_WALLPAPER, type UserConfig} from './lib/storage';
import {AS_HOME, forHome, isKeepAlive, readConfigFile, saveConfigFile} from './lib/ashome';
import {applyTheme} from './theme/tokens';
import {useTimeFormat} from './hooks/useTimeFormat';
import {AppMenu} from './components/AppMenu';
import {hiddenAfterUpgrade, toggleHidden, visibleApps} from './lib/apporder';
import {uninstallApp, uninstallVerdict, UNINSTALL_LOG} from './lib/uninstall';
import {appTile, inputTile, pickInOrder} from './lib/tiles';
import {moveKey, movesFor, orderTiles, orderUpdate, reslot, type MoveAction} from './lib/tileorder';
import {APP_ID, TIMING, WALLPAPER} from './config/constants';
import {WEATHER_ENABLED} from './config/features';
import {fontFamily} from './lib/clockfont';
import {fetchWeather} from './lib/weathernet';
import {fetchCalendars} from './lib/calendarload';
import {CAL_FRESH_MS, calendarIsFresh, readCalendarCache, usableCalendar, writeCalendarCache, type CalendarData} from './lib/calendar';
import {CalendarSetup} from './screens/CalendarSetup';
import {plateLooks, plateRects} from './lib/plates';
import {boardLayout, boardTop} from './lib/boardlayout';
import {countryOf, googleNewsUrl, newsIsFresh, NEWS_FRESH_MS, readNewsCache, tvLanguageTag, usableNews, writeNewsCache, type NewsData} from './lib/feeds';
import {fetchNews} from './lib/feedsload';
import {readQuoteCache, shownQuote, todaysQuote, writeQuoteCache, type Quote} from './lib/quote';
import {fetchQuote} from './lib/quoteload';
import {FeedsSetup} from './screens/FeedsSetup';
import {CountryPicker} from './screens/CountryPicker';
import {FRESH_MS, isFresh, readWeatherCache, usableCached, weatherView, writeWeatherCache, type City, type WeatherData} from './lib/weather';

type Screen = 'home' | 'settings' | 'picker' | 'cities' | 'calendarUrl' | 'homeMethod' | 'feeds' | 'country';

const TAB = {apps: 'apps', sources: 'sources', look: 'look', widgets: 'widgets', wallpaper: 'wallpaper', home: 'home'} as const;
const MISC = {lgHome: 'lg-home', settings: 'settings', system: 'system-settings'} as const;

/** The three tiles that always end the row. */
const miscTiles = (): TileModel[] => [
	{key: MISC.lgHome, label: 'LG Home', glyph: 'house'},
	{key: MISC.system, label: tr('TV settings', 'Настройки ТВ'), glyph: 'tv'},
	{key: MISC.settings, label: tr('Launcher settings', 'Настройки лаунчера'), glyph: 'gear'}
];

export function App () {
	const [screen, setScreen] = useState<Screen>('home');
	const [config, setConfig] = useState<UserConfig>(() => forHome(loadConfig()));
	// every text below is worded in the chosen language
	setLang(config.language);
	const language = lang();
	// The settings kept on the TV are the latest of both copies of the launcher (each writes them when a setting changes). The copy
	// running as the TV's Home always starts from them; the normal one only when it has none of its own yet.
	useEffect(() => {
		if (!AS_HOME && hasStoredConfig()) return;
		readConfigFile().then((raw) => {
			if (!raw) return;
			const next = forHome(parseConfig(raw));
			setConfig((current) => (JSON.stringify(current) === JSON.stringify(next) ? current : next));
			saveConfig(next);
		});
	}, []);
	// The normal launcher keeps the TV's copy up to date from the start (it is otherwise written when a setting changes).
	useEffect(() => { if (!AS_HOME && hasStoredConfig()) saveConfigFile(config); }, []);
	const [apps, setApps] = useState<AppEntry[]>(cachedApps);      // last known list: the row is drawn at once
	const [asleep, setAsleep] = useState(document.hidden);
	const [wakes, setWakes] = useState(0);                    // counts each return to the front (Home pressed)
	const [wallpaperOn, setWallpaperOn] = useState(!document.hidden);
	const wasHidden = useRef(document.hidden);
	const [menu, setMenu] = useState<{key: string; title: string; app: AppEntry | null} | null>(null);   // the tile whose Move / Hide / Uninstall menu is open
	const [focus, setFocus] = useState<{key: string; seq: number} | undefined>(undefined);               // asks Home to follow a moved tile
	const replaceRef = useRef(config.replaceHome);
	const [allInputs, setInputs] = useState<InputSource[]>([]);
	// the launcher itself may be in the TV's input list (see Options): it is not offered as a source
	const inputs = useMemo(() => allInputs.filter((i) => i.appId !== APP_ID), [allInputs]);
	const [weather, setWeather] = useState<WeatherData | null>(() => (WEATHER_ENABLED ? readWeatherCache() : null));
	const weatherRef = useRef(weather);
	weatherRef.current = weather;
	// the look saved with the cached picture, so the plates open with their glass instead of waiting for the wallpaper to start
	const [savedLook] = useState(() => readWallpaperLook(wallpaperCacheId(config.wallpaper.mode, config.wallpaper.path), config.frost));
	const [wallpaperFrost, setWallpaperFrost] = useState<string | null>(savedLook ? savedLook.frost : null);      // the blurred wallpaper for the frosted glass of the plates
	const [wallpaperMap, setWallpaperMap] = useState<number[] | null>(savedLook ? savedLook.map : null);       // brightness of the wallpaper picture (null: the theme's own background)
	const [calendar, setCalendar] = useState<CalendarData | null>(readCalendarCache);
	const calendarRef = useRef(calendar);
	calendarRef.current = calendar;
	const [news, setNews] = useState<NewsData | null>(readNewsCache);
	const newsRef = useRef(news);
	newsRef.current = news;
	const [quote, setQuote] = useState<Quote | null>(readQuoteCache);
	const quoteRef = useRef(quote);
	quoteRef.current = quote;
	const [toast, setToast] = useState<ToastMessage | null>(null);
	const toastTimer = useRef(0);
	const closedAt = useRef(0);
	const [systemInfo, setSystemInfo] = useState<SystemInfo>({homeHook: null, takeover: AS_HOME ? true : null});
	const tvFormat = useTimeFormat();

	// With the hand-over helper on, note in its log how long this launcher took to show its first screen.
	useEffect(() => {
		if (!config.replaceHome || !isWebOS()) return undefined;
		const frame = window.requestAnimationFrame(() => window.requestAnimationFrame(() => {
			const began = window.performance && window.performance.timing ? window.performance.timing.navigationStart : 0;
			markAliveSoon(began ? `launcher: first screen ${Date.now() - began} ms after the page started (hidden: ${document.hidden})` : undefined);
		}));
		return () => window.cancelAnimationFrame(frame);
	}, []);

	// Just after the TV starts the background helper may not be running yet (it is only started at boot when the
	// "at boot" link is on), and until it is, the Home button shows LG Home for a moment. So right after the first
	// screen is up, one light call makes sure it is running, tried again a few times if the Homebrew service is not
	// up yet. (The heavier check, which can rewrite the helper's files, still waits as before.)
	useEffect(() => {
		if (!config.replaceHome || !isWebOS()) return undefined;
		const delays = [1200, 3000, 6000, 12000, 25000, 50000];
		let cancelled = false;
		let timer = 0;
		const attempt = (n: number) => applyHomeHook().then((result) => {
			if (cancelled || result !== 'failed' || n + 1 >= delays.length) return;
			timer = window.setTimeout(() => attempt(n + 1), delays[n + 1]);
		});
		timer = window.setTimeout(() => attempt(0), delays[0]);
		return () => { cancelled = true; window.clearTimeout(timer); };
	}, []);

	// Colours follow the theme setting.
	useEffect(() => applyTheme(config.theme), [config.theme]);

	const showToast = useCallback((text: string, kind: ToastMessage['kind'] = 'info') => {
		window.clearTimeout(toastTimer.current);
		setToast({text, kind});
		toastTimer.current = window.setTimeout(() => setToast(null), TIMING.toastMs);
	}, []);

	// Start in remote (5-way) mode: hide the Magic Remote pointer whenever we come to the
	// front. Shaking the remote still brings it back.
	useEffect(() => hidePointerOnForeground(), []);

	// Load apps at start and whenever the Homebrew option changes. With a saved list on screen the real one is
	// fetched a few seconds later, off the critical path. It is fetched again each time the launcher comes back to
	// the front (at most every 10 s), so a newly installed app shows up without restarting the launcher.
	//
	// The saved list is the full Homebrew list (with its icons), and it is only ever replaced by a newer full list.
	// Just after the TV starts the Homebrew service is often not up yet: the saved list then stays on screen, and
	// the read is tried again a little later, quietly. The small built-in catalogue is used only when there is no
	// saved list at all (the very first start).
	const hadCache = useRef(apps.length > 0);
	const haveFull = useRef(apps.length > 0);           // the list on screen is a full Homebrew list
	const appsRef = useRef(apps);
	appsRef.current = apps;
	const lastLoad = useRef(0);
	const seenWakes = useRef(0);
	const [reloads, setReloads] = useState(0);        // the "Refresh list" button in Settings → Apps
	const manualLoad = useRef(false);
	useEffect(() => {
		const woke = wakes !== seenWakes.current;
		seenWakes.current = wakes;
		const manual = manualLoad.current;               // asked for with the button: now, and say what happened
		manualLoad.current = false;
		if (woke && !manual && Date.now() - lastLoad.current < 10000) return undefined;
		let cancelled = false;
		let timer = 0;
		const retryAfter = [15000, 45000, 120000, 300000];      // quiet retries when the Homebrew service does not answer
		let told = false;
		const load = (attempt: number) => {
			if (cancelled) return;
			const keepSaved = config.useHomebrew && haveFull.current;
			const retry = () => { if (!cancelled && config.useHomebrew && attempt < retryAfter.length) timer = window.setTimeout(() => { if (document.hidden) return; load(attempt + 1); }, retryAfter[attempt]); };
			listApps(config.useHomebrew, keepSaved)
				.then(({apps: fresh, source}) => {
					if (cancelled) return;
					if (source === 'homebrew') {
						const list = keepKnownIcons(fresh, appsRef.current);
						if (manual) {
							const added = list.filter((a) => !appsRef.current.some((p) => p.id === a.id)).length;
							showToast(added > 0 ? fmt(tr('Found: {0}', 'Найдено: {0}'), count(added, ['new app', 'new apps'], ['новое приложение', 'новых приложения', 'новых приложений'])) : tr('The app list is up to date', 'Список приложений актуален'));
						}
						lastLoad.current = Date.now();       // counted only once a full list has been read, so a failed load is tried again
						haveFull.current = true;
						setApps((previous) => (sameApps(previous, list) ? previous : list));
						cacheApps(list);
						// settings from an older version listed the apps to show: now the real list is known, keep those apps hidden
						if (list.length > 0) setConfig((c) => { if (c.hiddenIds !== null) return c; const next = {...c, hiddenIds: hiddenAfterUpgrade(list, c.appIds)}; saveConfig(next); return next; });
						return;
					}
					// only the built-in catalogue was available (nothing saved yet, or Homebrew is switched off)
					setApps((previous) => (sameApps(previous, fresh) ? previous : fresh));
					if (config.useHomebrew) {
						lastLoad.current = 0;
						if ((manual || !told) && rootState() !== 'no') { told = true; showToast(tr('Using the built-in app list', 'Используется встроенный список приложений') + (whySuffix() || tr('. The Homebrew Channel is not available yet', '. Homebrew Channel пока недоступен')), 'error'); }
						retry();
					} else {
						lastLoad.current = Date.now();
						if (manual) showToast(tr('The live app list is off. Turn it on in Settings → Options to see new apps', 'Живой список приложений выключен. Включите его в Настройки → Параметры, чтобы видеть новые приложения'), 'error');
					}
				})
				.catch(() => {
					if (cancelled) return;
					// the saved full list stays on screen; try again later without a word
					if (config.useHomebrew && haveFull.current) retry();
					else if (manual || !told) { told = true; showToast(tr('Could not read the app list', 'Не удалось прочитать список приложений') + whySuffix(), 'error'); }
				});
		};
		// Not needed to draw the home screen, and it is a lot of work for the TV (a shell command and the icons), so
		// it waits until the launcher has been up for a few seconds (at once only when there is no saved list to show).
		timer = window.setTimeout(() => { if (woke && document.hidden) return; load(0); }, hadCache.current || woke ? TIMING.backgroundWorkMs : 0);
		hadCache.current = false;
		return () => { cancelled = true; window.clearTimeout(timer); };
	}, [config.useHomebrew, wakes, reloads, showToast]);

	// In the background the launcher lets go of its wallpaper (the big bitmap) and stops its clock, but it
	// keeps its screen: if the TV ever reports "hidden" wrongly, the launcher must still be visible,
	// not a black screen. The state is also re-read every second as a safety net.
	replaceRef.current = config.replaceHome;
	useEffect(() => {
		const sync = (why: string) => {
			const hidden = document.hidden;
			if (wasHidden.current && !hidden) setWakes((n) => n + 1);
			wasHidden.current = hidden;
			setAsleep((was) => {
				if (was !== hidden && replaceRef.current) {
					// the helper checks for the "alive" mark after opening the launcher
					if (hidden) logLine(`launcher: hidden (${why})`);
					else markAliveSoon(`launcher: visible (${why})`);
				}
				return hidden;
			});
			if (hidden) { setScreen('home'); setMenu(null); }
			// updated while loaded: the new version is loaded in the background, or at the latest when the launcher shows again
			if (why === 'event') reloadIfUpdated();
		};
		const onVisibility = () => sync('event');
		document.addEventListener('visibilitychange', onVisibility);
		const poll = window.setInterval(() => sync('check'), 1000);
		// Home pressed while the launcher is already in front: the TV relaunches it without hiding it first.
		const onRelaunch = () => {
			reloadIfUpdated();
			// Running as the TV's Home, the TV relaunches it now and then with {"keepAlive": true} only to keep it loaded:
			// that is not the Home key, so whatever is open (Settings) stays open
			const ps = (window as unknown as {PalmSystem?: {launchParams?: string}}).PalmSystem;
			if (isKeepAlive(ps && ps.launchParams)) return;
			setScreen('home'); setMenu(null); setWakes((n) => n + 1);
			// Home pressed while in front: the launcher never became hidden, so tell the helper it is here
			// (otherwise its check could think the launcher failed to show and open it again and again)
			if (replaceRef.current) markAliveSoon('launcher: relaunched while in front');
		};
		document.addEventListener('webOSRelaunch', onRelaunch);
		return () => { document.removeEventListener('visibilitychange', onVisibility); document.removeEventListener('webOSRelaunch', onRelaunch); window.clearInterval(poll); };
	}, []);

	// The wallpaper stays loaded for a while after the launcher goes to the background, so pressing Home
	// again finds it ready (no fade-in). After that it is released to give the memory back.
	useEffect(() => {
		if (!asleep) { setWallpaperOn(true); return undefined; }
		const timer = window.setTimeout(() => setWallpaperOn(false), WALLPAPER.releaseAfterMs);
		return () => window.clearTimeout(timer);
	}, [asleep]);

	// Inputs are only read when they're needed: some are on the home row, or Settings is open.
	const needInputs = config.sourceIds.length > 0 || screen === 'settings';
	useEffect(() => {
		if (!needInputs) return undefined;
		const refresh = () => {
			if (!isWebOS()) { setInputs(mockInputs()); return; }
			listInputs().then(setInputs).catch(() => showToast(tr('Could not read the inputs', 'Не удалось прочитать входы'), 'error'));
		};
		refresh();
		const onVisibility = () => { if (!document.hidden) refresh(); };
		document.addEventListener('visibilitychange', onVisibility);
		return () => document.removeEventListener('visibilitychange', onVisibility);
	}, [needInputs, showToast]);

	// Which app the TV's Home button opens: read when Settings opens, so the Options row is right.
	useEffect(() => {
		// note the real size of the screen in the helper's log: this is what to look at if a list ever looks wrong on some TV
		if (screen === 'settings' && isWebOS()) {
			const chrome = /Chrome\/([0-9.]+)/.exec(navigator.userAgent);
			logLine(`launcher: settings opened, window ${window.innerWidth}x${window.innerHeight}, pixel ratio ${window.devicePixelRatio}, screen ${window.screen.width}x${window.screen.height}, web view ${chrome ? chrome[1] : 'unknown'}`);
		}
		// One tiny command first: can this TV run commands at all? If not, the rows that need that are greyed out
		// with the reason, and the other status reads (which would only fail) are skipped.
		if (screen === 'settings') {
			let live = true;
			probeRoot().then((root) => {
				const rootDetail = lastFailureDetail();
				if (!live) return undefined;
				if (root === 'no') { setSystemInfo({homeHook: null, takeover: AS_HOME ? true : null, root, rootDetail}); return undefined; }
				return Promise.all([getHookStatus(), AS_HOME ? Promise.resolve(true) : takeoverActive(), readHomeMode()]).then(([homeHook, takeover, homeMode]) => { if (live) setSystemInfo({homeHook, takeover, homeMode, root, rootDetail}); });
			});
			return () => { live = false; };
		}
		return undefined;
	}, [screen]);

	// Shortly after we come to the front (not at the very first moment, while the TV is still switching
	// apps): make sure the hand-over helper is running (if it is switched on), and end LG's home app to
	// free memory. If we are pushed to the back again within seconds, LG Home has come back in front of
	// us, so "Close LG Home" turns itself off (not needed when the helper is on: it hands straight back).
	useEffect(() => {
		if ((!config.closeHome && !config.replaceHome) || !isWebOS()) return undefined;
		let timer = 0;
		let retryTimer = 0;
		const arm = () => {
			window.clearTimeout(timer);
			window.clearTimeout(retryTimer);
			if (document.hidden) return;
			timer = window.setTimeout(() => {
				closedAt.current = Date.now();
				if (config.replaceHome) {
					// The Homebrew service is often not up yet just after the TV starts: then nothing is changed
					// (the setting stays on) and it is tried again a little later.
					const tryHelper = (attempt: number) => reapplyHomeHook(config.hookBoot).then(({result, detail, fatal, unreachable}) => {
						if (unreachable) {
							if (attempt < 4) retryTimer = window.setTimeout(() => { if (!document.hidden) tryHelper(attempt + 1); }, [10000, 30000, 90000, 240000][attempt]);
							return;
						}
						if (!fatal) return;
						setConfig((c) => { const next = {...c, replaceHome: false}; saveConfig(next); return next; });
						removeHomeHook();
						showToast(hookMessage(result, detail), 'error');
					});
					tryHelper(0);
				}
				closeLgHome().catch(() => undefined);      // the Homebrew service may not be up yet: nothing to do then
			}, TIMING.backgroundWorkMs);
		};
		const onVisibility = () => {
			if (!config.replaceHome && document.hidden && Date.now() - closedAt.current < 2500) {
				setConfig((c) => { const next = {...c, closeHome: false}; saveConfig(next); return next; });
				try { localStorage.setItem('customlauncher-homekill-off', '1'); } catch { /* ignore */ }
			}
			arm();
		};
		arm();
		document.addEventListener('visibilitychange', onVisibility);
		return () => { window.clearTimeout(timer); window.clearTimeout(retryTimer); document.removeEventListener('visibilitychange', onVisibility); };
	}, [config.closeHome, config.replaceHome, config.hookBoot, showToast]);

	// The weather line: read from the saved copy at once (nothing is fetched to draw the home screen), refreshed in
	// the background a few seconds after the launcher appears, and then every 30 minutes while it is in front.
	const wx = config.weather;
	const wxCity: City | null = WEATHER_ENABLED && wx.on ? wx.city : null;
	const wxLat = wxCity ? wxCity.lat : 0;
	const wxLon = wxCity ? wxCity.lon : 0;
	useEffect(() => {
		if (!wxCity) return undefined;
		const city = wxCity;
		const allowShell = config.useHomebrew && isWebOS();
		const backoff = [120000, 600000, 1800000];
		let cancelled = false;
		let timer = 0;
		let failures = 0;
		const run = () => {
			if (cancelled || document.hidden) return;        // coming back to the front starts this over
			fetchWeather(city, allowShell).then(
				(data) => {
					if (cancelled) return;
					failures = 0;
					writeWeatherCache(data);
					setWeather(data);
					timer = window.setTimeout(run, FRESH_MS);
				},
				() => {
					if (cancelled) return;
					if (failures === 0 && !usableCached(weatherRef.current, city.lat, city.lon, Date.now())) showToast(tr('Could not get the weather. Check the TV\'s internet connection. It will try again', 'Не удалось получить погоду. Проверьте подключение ТВ к интернету. Попытка повторится'), 'error');
					timer = window.setTimeout(run, backoff[Math.min(failures, backoff.length - 1)]);
					failures++;
				}
			);
		};
		const now = Date.now();
		const known = weatherRef.current;
		const wait = isFresh(known, city.lat, city.lon, now) && known
			? Math.max(0, FRESH_MS - (now - known.at))
			: (usableCached(known, city.lat, city.lon, now) ? TIMING.backgroundWorkMs : 1500);
		timer = window.setTimeout(run, wait);
		return () => { cancelled = true; window.clearTimeout(timer); };
	}, [wxCity !== null, wxLat, wxLon, wakes, config.useHomebrew, showToast]);

	const weatherAt = useMemo(() => {
		if (!wxCity) return null;
		const data = usableCached(weather, wxCity.lat, wxCity.lon, Date.now());
		return data ? (day: number) => weatherView(data, wx.unit, wx.showCity ? wxCity.name : '', day) : null;
	}, [weather, wxCity, wx.unit, wx.showCity, language]);

	// The calendar: the same idea as the weather. The saved copy is drawn at once; the feed is read a few seconds after
	// the launcher appears and then every 15 minutes while it is in front.
	const calSources = config.calendar.on ? config.calendar.sources : [];
	const calKey = calSources.join('\n');
	const [calendarLoading, setCalendarLoading] = useState(false);
	useEffect(() => {
		if (!calKey) return undefined;
		const allowShell = config.useHomebrew && isWebOS();
		const backoff = [120000, 600000, 1800000];
		let cancelled = false;
		let timer = 0;
		let failures = 0;
		const run = () => {
			if (cancelled || document.hidden) return;
			setCalendarLoading(calendarRef.current === null || calendarRef.current.key !== calKey);
			fetchCalendars(calSources, calKey, allowShell, calendarRef.current && calendarRef.current.key === calKey ? calendarRef.current : null).then(
				(data) => {
					if (cancelled) return;
					failures = 0;
					writeCalendarCache(data);
					setCalendar(data);
					setCalendarLoading(false);
					timer = window.setTimeout(run, CAL_FRESH_MS);
				},
				() => {
					if (cancelled) return;
					setCalendarLoading(false);
					if (failures === 0 && !usableCalendar(calendarRef.current, calKey, Date.now())) showToast(tr('Could not load the calendar. Check the link and the internet. It will try again', 'Не удалось загрузить календарь. Проверьте ссылку и интернет. Попробую ещё раз'), 'error');
					timer = window.setTimeout(run, backoff[Math.min(failures, backoff.length - 1)]);
					failures++;
				}
			);
		};
		const now = Date.now();
		const known = calendarRef.current;
		const wait = calendarIsFresh(known, calKey, now) && known
			? Math.max(0, CAL_FRESH_MS - (now - known.at))
			: (usableCalendar(known, calKey, now) ? TIMING.backgroundWorkMs : 1500);
		timer = window.setTimeout(run, wait);
		return () => { cancelled = true; window.clearTimeout(timer); };
	}, [calKey, wakes, config.useHomebrew, showToast]);

	const calendarCard = useMemo(() => {
		if (!calKey) return null;
		return {data: usableCalendar(calendar, calKey, Date.now()), loading: calendarLoading};
	}, [calKey, calendar, calendarLoading, language]);

	// The news: the same idea as the calendar. The saved copy is drawn at once; the feeds are read a few seconds after the launcher
	// appears and then every 25 minutes while it is in front. Google News follows the chosen country and the launcher's language.
	const newsSources = useMemo(() => {
		if (!config.news.on) return [] as string[];
		const google = config.news.google ? [googleNewsUrl(countryOf(config.news.country, tvLanguageTag()), language)] : [];
		return google.concat(config.news.feeds);
	}, [config.news, language]);
	const newsKey = newsSources.join('\n');
	const [newsLoading, setNewsLoading] = useState(false);
	useEffect(() => {
		if (!newsKey) return undefined;
		const allowShell = config.useHomebrew && isWebOS();
		const backoff = [120000, 600000, 1800000];
		let cancelled = false;
		let timer = 0;
		let failures = 0;
		const run = () => {
			if (cancelled || document.hidden) return;
			setNewsLoading(!usableNews(newsRef.current, newsKey));
			fetchNews(newsSources, newsKey, allowShell).then(
				(data) => {
					if (cancelled) return;
					failures = 0;
					writeNewsCache(data);
					setNews(data);
					setNewsLoading(false);
					timer = window.setTimeout(run, NEWS_FRESH_MS);
				},
				() => {
					if (cancelled) return;
					setNewsLoading(false);
					if (failures === 0 && !usableNews(newsRef.current, newsKey)) showToast(allowShell ? tr('Could not load the news. Check the feeds and the internet. It will try again', 'Не удалось загрузить новости. Проверьте ленты и интернет. Попробую ещё раз') : tr('Could not load the news. Most news sites can only be read through the Homebrew Channel (root)', 'Не удалось загрузить новости. Большинство новостных сайтов можно читать только через Homebrew Channel (нужен рут)'), 'error');
					timer = window.setTimeout(run, backoff[Math.min(failures, backoff.length - 1)]);
					failures++;
				}
			);
		};
		const now = Date.now();
		const known = newsRef.current;
		const wait = newsIsFresh(known, newsKey, now) && known
			? Math.max(0, NEWS_FRESH_MS - (now - known.at))
			: (usableNews(known, newsKey) ? TIMING.backgroundWorkMs : 1500);
		timer = window.setTimeout(run, wait);
		return () => { cancelled = true; window.clearTimeout(timer); };
	}, [newsKey, wakes, config.useHomebrew, showToast]);

	const newsCard = useMemo(() => {
		if (!newsKey) return null;
		const data = usableNews(news, newsKey);
		return {items: data ? data.items : null, loading: newsLoading};
	}, [newsKey, news, newsLoading]);

	// The quote of the day: today's is fetched once (yesterday's stays until it is in), in the launcher's language. Wikimedia lets
	// the web view read it, so this needs no root; the shell is the fallback for TVs that cannot verify the certificate.
	const quoteOn = config.quote.on;
	useEffect(() => {
		if (!quoteOn) return undefined;
		const allowShell = config.useHomebrew && isWebOS();
		let cancelled = false;
		let timer = 0;
		let failures = 0;
		const backoff = [120000, 600000, 1800000];
		const run = () => {
			if (cancelled || document.hidden) return;
			const now = Date.now();
			if (todaysQuote(quoteRef.current, language, now)) {
				// look again just after midnight
				const next = new Date(now);
				next.setHours(24, 1, 0, 0);
				timer = window.setTimeout(run, next.getTime() - now);
				return;
			}
			fetchQuote(language, allowShell, now).then(
				(q) => { if (cancelled) return; failures = 0; writeQuoteCache(q); setQuote(q); timer = window.setTimeout(run, 60000); },
				() => { if (cancelled) return; timer = window.setTimeout(run, backoff[Math.min(failures, backoff.length - 1)]); failures++; }
			);
		};
		timer = window.setTimeout(run, todaysQuote(quoteRef.current, language, Date.now()) ? 1000 : TIMING.backgroundWorkMs);
		return () => { cancelled = true; window.clearTimeout(timer); };
	}, [quoteOn, language, wakes, config.useHomebrew]);

	const quoteCard = useMemo(() => (quoteOn ? {quote: shownQuote(quote, language), loading: !shownQuote(quote, language)} : null), [quoteOn, quote, language]);

	// Tell the user once if the safety switch-off fired.
	useEffect(() => {
		const onVisibility = () => {
			if (document.hidden) return;
			try {
				if (localStorage.getItem('customlauncher-homekill-off')) {
					localStorage.removeItem('customlauncher-homekill-off');
					showToast(tr('"Close LG Home" was switched off: LG Home kept coming back', '«Закрывать LG Home» выключено: LG Home постоянно возвращался на экран'), 'error');
				}
			} catch { /* ignore */ }
		};
		onVisibility();
		document.addEventListener('visibilitychange', onVisibility);
		return () => document.removeEventListener('visibilitychange', onVisibility);
	}, [showToast]);

	const updateConfig = (raw: UserConfig) => { const next = forHome(raw); setConfig(next); saveConfig(next); saveConfigFile(next); };

	// ----- derived lists -----
	const shownApps = useMemo(
		() => visibleApps(apps, config.appIds, config.hiddenIds),
		[apps, config.appIds, config.hiddenIds]
	);
	const shownInputs = useMemo(() => pickInOrder(inputs, config.sourceIds, (s) => s.appId), [inputs, config.sourceIds]);

	// apps and inputs share one order (Move to start / left / right / end); LG Home and Settings always end the row
	const orderedTiles: TileModel[] = useMemo(
		() => orderTiles(shownApps.map(appTile).concat(shownInputs.map(inputTile)), (t) => t.key, config.tileOrder),
		[shownApps, shownInputs, config.tileOrder]
	);
	// Running as the TV's Home there is no other LG Home to open.
	const homeItems: TileModel[] = useMemo(() => orderedTiles.concat(AS_HOME ? miscTiles().filter((t) => t.key !== MISC.lgHome) : miscTiles()), [orderedTiles, language]);

	const timeFormat: TimeFormat = config.clockFormat === 'tv' ? tvFormat : config.clockFormat;
	// the font chosen in Settings → Widgets is the one of the weather and calendar cards too (the clock sets its own)
	useEffect(() => { document.documentElement.style.setProperty('--widget-font', fontFamily(config.clock.font)); }, [config.clock.font]);

	const tabs: SettingsTab[] = useMemo(() => [
		{id: TAB.apps, icon: 'apps', label: tr('Apps', 'Приложения'), arrange: tr('Arrange', 'Порядок'), refresh: true, items: apps.map(appTile), shown: shownApps.map(appTile)},
		{id: TAB.sources, icon: 'inputs', label: tr('Inputs', 'Входы'), arrange: tr('Arrange', 'Порядок'), items: inputs.map(inputTile), shown: shownInputs.map(inputTile)},
		{id: TAB.look, icon: 'look', label: tr('Appearance', 'Оформление'), arrange: '', items: [], shown: [], options: optionsFor('look', config)},
		{id: TAB.widgets, icon: 'widgets', label: tr('Widgets', 'Виджеты'), arrange: '', items: [], shown: [], options: optionsFor('widgets', config)},
		{id: TAB.wallpaper, icon: 'wallpaper', label: tr('Wallpaper', 'Обои'), arrange: '', items: [], shown: [], options: optionsFor('wallpaper', config)},
		{id: TAB.home, icon: 'home', label: tr('Home screen', 'Главный экран'), arrange: '', items: [], shown: [], options: optionsFor('home', config, systemInfo)}
	] as SettingsTab[], [apps, inputs, shownApps, shownInputs, config, systemInfo, language]);

	const boardY = boardTop({iconScale: config.iconScale, bottomMargin: config.bottomMargin, showLabels: config.showLabels, rows: 1}, homeItems.length);
	const layout = useMemo(() => boardLayout({
		weather: !!weatherAt, calendar: !!calendarCard, news: !!newsCard, quote: !!quoteCard,
		clock: {size: config.clock.size, showDate: config.clock.showDate, dateSize: config.clock.dateSize, period: timeFormat === '12h' && config.clock.showPeriod}
	}, boardY), [weatherAt !== null, calendarCard !== null, newsCard !== null, quoteCard !== null, config.clock.size, config.clock.showDate, config.clock.dateSize, config.clock.showPeriod, timeFormat, boardY]);
	const plates = useMemo(
		() => plateLooks(config.plates, config.theme, wallpaperMap, plateRects(layout), config.glass ? wallpaperFrost : null),
		[config.plates, config.theme, wallpaperMap, layout, config.glass, wallpaperFrost]
	);

	// The start-up picture: noted while drawing (so the wallpaper's first save already follows it), applied when it changes
	noteSplashMode(config.splash);
	useEffect(() => {
		const timer = window.setTimeout(() => applySplashMode(config.splash, readWallpaperCache(wallpaperCacheId(config.wallpaper.mode, config.wallpaper.path))), TIMING.backgroundWorkMs);
		return () => window.clearTimeout(timer);
	}, [config.splash]);

	// Pixel shift on Auto follows the screen: on for an OLED model (asked once, a few seconds after the start)
	const [oled, setOled] = useState<boolean | null>(knownOled);
	useEffect(() => {
		if (oled !== null || config.pixelShift !== 'auto') return undefined;
		const timer = window.setTimeout(() => detectOled().then(setOled), TIMING.backgroundWorkMs);
		return () => window.clearTimeout(timer);
	}, [config.pixelShift]);

	const appearance: Appearance = useMemo(
		() => ({iconScale: config.iconScale, sideMargin: config.sideMargin, bottomMargin: config.bottomMargin, showLabels: config.showLabels, labelsSelectedOnly: config.labelsSelectedOnly, rows: 1, shadows: config.shadows, pixelShift: config.pixelShift === 'on' || (config.pixelShift === 'auto' && oled === true), clock: config.clock}),
		[config.iconScale, config.sideMargin, config.bottomMargin, config.showLabels, config.labelsSelectedOnly, config.shadows, config.pixelShift, oled, config.clock]
	);

	// ----- actions -----
	const onActivate = useCallback((item: TileModel) => {
		if (item.key === MISC.settings) { setScreen('settings'); return; }
		if (item.key === MISC.system) { launchSystemSettings().catch(() => showToast(tr('Could not open the TV settings', 'Не удалось открыть настройки ТВ'), 'error')); return; }
		if (item.key === MISC.lgHome) {
			const open = () => launchLgHome().catch(() => showToast(tr('Could not open LG Home', 'Не удалось открыть LG Home'), 'error'));
			// With LG Home replaced, the real one is put back first (it comes back at the next launch of this launcher).
			if (config.replaceHome) stockHome().then(() => window.setTimeout(open, 1200));
			else open();
			return;
		}
		// With the helper on: say the launcher is alive and note what is being opened, so the helper's check
		// can never mistake this moment for a failure, and its log shows what happened if an app does not open.
		if (config.replaceHome) markAliveSoon(`launcher: opening ${item.key}`, false);
		launchApp(item.key).catch(() => {
			if (config.replaceHome) logLine(`launcher: the TV refused to open ${item.key}`);
			showToast(fmt(tr('Could not open {0}', 'Не удалось открыть {0}'), item.label), 'error');
		});
	}, [showToast, config.replaceHome]);

	const onToggle = useCallback((tabId: string, key: string) => {
		if (tabId === TAB.apps) {
			// apps are shown unless hidden: OK adds the app to the hide-list, or takes it off
			const hidden = config.hiddenIds !== null ? config.hiddenIds : hiddenAfterUpgrade(apps, config.appIds);
			updateConfig({...config, hiddenIds: toggleHidden(hidden, key)});
		} else {
			updateConfig({...config, sourceIds: toggleId(config.sourceIds, key)});
		}
	}, [config, apps]);

	/** Replace a row's order with `keys` (the full list of what's shown). */
	const onReorder = useCallback((tabId: string, keys: string[]) => {
		// the tiles of this tab take the places they already hold on the shared row, in their new order
		const whole = reslot(orderedTiles.map((t) => t.key), keys);
		const update = orderUpdate(whole, shownApps.map((a) => a.id), shownInputs.map((i) => i.appId), config.appIds, config.sourceIds);
		const hiddenIds = config.hiddenIds !== null ? config.hiddenIds : hiddenAfterUpgrade(apps, config.appIds);
		updateConfig({...config, tileOrder: update.tileOrder, hiddenIds, appIds: tabId === TAB.apps ? keys.concat((config.appIds || []).filter((id) => keys.indexOf(id) === -1)) : update.appIds, sourceIds: tabId === TAB.sources ? keys : update.sourceIds});
	}, [config, apps, orderedTiles, shownApps, shownInputs]);

	const changeOption = useCallback((optionId: string, press: Press) => {
		const pref = findPref(optionId);
		if (!pref) return;
		const blocked = disabledReason(optionId, systemInfo);
		if (blocked) { showToast(blocked, 'error'); return; }
		if (optionId === 'weatherOn' && !config.weather.city) { showToast(tr('Choose a city first', 'Сначала выберите город')); setScreen('cities'); return; }
		if (optionId === 'calendarOn' && !config.calendar.sources.length) { showToast(tr('Add a calendar first', 'Сначала подключите календарь')); setScreen('calendarUrl'); return; }
		if (optionId === 'newsOn' && !config.news.google && !config.news.feeds.length) { showToast(tr('Turn on Google News or add a feed first', 'Сначала включите Google Новости или добавьте ленту')); setScreen('feeds'); return; }
		if (pref.kind === 'info') return;
		if (pref.kind === 'action') {
			if (optionId === 'wallpaper') setScreen('picker');
			else if (optionId === 'weatherCity') setScreen('cities');
			else if (optionId === 'calendarUrl') setScreen('calendarUrl');
			else if (optionId === 'newsFeeds') setScreen('feeds');
			else if (optionId === 'newsCountry') setScreen('country');
			else if (optionId === 'homeMethod') setScreen('homeMethod');
			else if (optionId === 'resetLook') { updateConfig(resetLook(config)); showToast(tr('Look and clock reset', 'Вид и часы сброшены')); }
			return;
		}
		const next = applyPref(config, optionId, press);
		if (next !== config) updateConfig(next);
	}, [config, showToast, systemInfo]);

	// Settings → Home screen → How the launcher opens. Every way but the helper is switched on the TV by tools/homemode.sh,
	// which first switches off whatever is on; the helper is installed by the launcher after that has switched everything off.
	const [modeBusy, setModeBusy] = useState<HomeMode | null>(null);
	const onPickHomeMode = useCallback((mode: HomeMode) => {
		setModeBusy(mode);
		const name = modeText(mode).name;
		const refresh = () => Promise.all([readHomeMode(), getHookStatus(), takeoverActive()]).then(([homeMode, homeHook, takeover]) => setSystemInfo((previous) => ({...previous, homeMode, homeHook, takeover})));
		const done = (ok: boolean, why: string) => {
			setModeBusy(null);
			refresh();
			showToast(ok ? fmt(tr('Done: {0}', 'Готово: {0}'), name) : fmt(tr('Could not switch: {0}', 'Не удалось переключить: {0}'), why), ok ? 'info' : 'error');
		};
		switchHomeMode(mode === 'helper' ? 'none' : mode).then(({ok, why}) => {
			if (!ok || mode !== 'helper') {
				// once another way is on, the helper setting must not bring the helper back at the next start (a failed switch
				// leaves the way that was on, so the setting stays as it was)
				if (ok && config.replaceHome) updateConfig({...config, replaceHome: false});
				done(ok, why);
				return;
			}
			installHomeHook(true).then(({result, detail}) => {
				if (result === 'applied') updateConfig({...config, replaceHome: true, hookBoot: true});
				done(result === 'applied', hookMessage(result, detail));
			});
		});
	}, [config, showToast]);

	const onRefresh = useCallback(() => {
		manualLoad.current = true;
		showToast(tr('Looking for new apps…', 'Ищем новые приложения…'));
		setReloads((n) => n + 1);
	}, [showToast]);

	const onOption = useCallback((optionId: string) => changeOption(optionId, 'ok'), [changeOption]);
	const onAdjust = useCallback((optionId: string, direction: -1 | 1) => changeOption(optionId, direction < 0 ? 'left' : 'right'), [changeOption]);

	const onChooseWallpaper = useCallback((choice: WallpaperChoice) => {
		const keep = {intervalSec: config.wallpaper.intervalSec, shuffle: config.wallpaper.shuffle};
		const wallpaper = choice.mode === 'gradient'
			? {...DEFAULT_WALLPAPER, ...keep}
			: {mode: choice.mode, path: choice.path, ...keep};
		updateConfig({...config, wallpaper});
		setScreen('settings');
		showToast(choice.mode === 'gradient' ? tr('Default background restored', 'Фон по умолчанию восстановлен') : choice.mode === 'image' ? tr('Loading the wallpaper…', 'Загружаем обои…') : tr('Starting the slideshow…', 'Запускаем слайд-шоу…'));
	}, [config, showToast]);

	const onChooseCalendars = useCallback((sources: string[]) => {
		updateConfig({...config, calendar: {...config.calendar, sources, on: sources.length > 0}});
		showToast(sources.length ? tr('Connecting the calendars…', 'Календари подключаются…') : tr('Calendars removed', 'Календари убраны'));
	}, [config, showToast]);

	const onChooseFeeds = useCallback((feeds: string[]) => {
		updateConfig({...config, news: {...config.news, feeds, on: config.news.on && (config.news.google || feeds.length > 0)}});
	}, [config]);

	const onChooseCountry = useCallback((country: string) => {
		updateConfig({...config, news: {...config.news, country}});
		setScreen('settings');
	}, [config]);

	const onChooseCity = useCallback((city: City) => {
		updateConfig({...config, weather: {...config.weather, city, on: true}});
		setScreen('settings');
		showToast(fmt(tr('Weather for {0}', 'Погода для города: {0}'), city.name));
	}, [config, showToast]);

	// Hold OK on an app or input: its Move / Hide (/ Uninstall) menu. LG Home and Settings have no menu.
	const onLongPress = useCallback((item: TileModel) => {
		if (item.key === MISC.lgHome || item.key === MISC.settings || item.key === MISC.system) return;
		const app = apps.find((a) => a.id === item.key) || null;
		setMenu({key: item.key, title: item.label, app});
	}, [apps]);

	const hideTile = useCallback((tile: {key: string; title: string; app: AppEntry | null}) => {
		onToggle(tile.app ? TAB.apps : TAB.sources, tile.key);
		setMenu(null);
		showToast(fmt(tr('{0} hidden. Bring it back in Settings → {1}', '{0} скрыто. Вернуть можно в Настройки → {1}'), tile.title, tile.app ? tr('Apps', 'Приложения') : tr('Sources', 'Источники')));
	}, [onToggle, showToast]);

	// Move to start / left / right on the shared row of apps and inputs; the highlight follows the tile.
	const moveTile = useCallback((key: string, action: MoveAction) => {
		const keys = orderedTiles.map((t) => t.key);
		const next = moveKey(keys, key, action);
		const update = orderUpdate(next, shownApps.map((a) => a.id), shownInputs.map((i) => i.appId), config.hiddenIds !== null ? (config.appIds || []) : null, config.sourceIds);
		updateConfig({...config, tileOrder: update.tileOrder, appIds: update.appIds !== null ? update.appIds : config.appIds, sourceIds: update.sourceIds});
		setFocus((f) => ({key, seq: (f ? f.seq : 0) + 1}));
		setMenu(null);
	}, [config, orderedTiles, shownApps, shownInputs]);

	// Uninstall (after the menu's confirmation): remove it, then re-read the app list to see whether it is really gone.
	const removeApp = useCallback((app: AppEntry) => {
		uninstallApp(app.id).then((result) => listApps(config.useHomebrew).then(
			({apps: list, source}) => { setApps(list); if (source === 'homebrew') cacheApps(list); },
			() => { if (result === 'removed') setApps((previous) => previous.filter((a) => a.id !== app.id)); }
		).then(() => {
			setMenu(null);
			if (result === 'removed') {
				updateConfig({...config, appIds: config.appIds ? config.appIds.filter((id) => id !== app.id) : null, hiddenIds: config.hiddenIds ? config.hiddenIds.filter((id) => id !== app.id) : config.hiddenIds});
				showToast(fmt(tr('{0} uninstalled', '{0} удалено'), app.title));
			} else {
				showToast(result === 'still-installed' ? fmt(tr('The TV did not remove {0} (details: {1})', 'ТВ не удалил {0} (подробности: {1})'), app.title, UNINSTALL_LOG) : tr('Could not uninstall the app', 'Не удалось удалить приложение') + (whySuffix() || tr('. The TV did not accept the command', '. ТВ не принял команду')), 'error');
			}
		}));
	}, [config, showToast]);

	const menuVerdict = menu && menu.app ? uninstallVerdict(menu.app) : null;
	const menuBlocked = menuVerdict && !menuVerdict.ok ? menuVerdict.reason : (config.useHomebrew ? null : tr('Turn on "App list through the Homebrew Channel" in Options', 'Включите «Список приложений через Homebrew Channel» в Параметрах'));
	const menuMoves = menu ? movesFor(orderedTiles.map((t) => t.key), menu.key) : {start: false, left: false, right: false, end: false};

	return (
		<>
			{wallpaperOn && <Wallpaper config={config.wallpaper} onProblem={(text) => showToast(text, 'error')} onTone={(map, frost) => { setWallpaperMap(map); setWallpaperFrost(frost); }} frost={config.frost} />}
			{/* Home stays mounted under the other screens so it keeps its focus position. */}
			<Home items={homeItems} timeFormat={timeFormat} appearance={appearance} active={screen === 'home' && !menu} paused={asleep} onActivate={onActivate} onLongPress={onLongPress} wake={wakes} focus={focus} weatherAt={weatherAt} cardSize={config.cardSize} calendar={calendarCard} news={newsCard} quote={quoteCard} layout={layout} plates={plates} boardTop={boardY} />
			{menu && <AppMenu title={menu.title} moves={menuMoves} hideDetail={menu.app ? tr('Bring it back in Settings → Apps', 'Вернуть можно в Настройки → Приложения') : tr('Bring it back in Settings → Sources', 'Вернуть можно в Настройки → Источники')} showUninstall={!!menu.app} blockedReason={menuBlocked} onMove={(action) => moveTile(menu.key, action)} onHide={() => hideTile(menu)} onUninstall={() => { if (menu.app) removeApp(menu.app); }} onClose={() => setMenu(null)} />}
			{/* Settings stays mounted under the picker so it keeps its tab. */}
			{screen !== 'home' && <Settings tabs={tabs} active={screen === 'settings'} onToggle={onToggle} onReorder={onReorder} onOption={onOption} onRefresh={onRefresh} onAdjust={onAdjust} onClose={() => setScreen('home')} />}
			{screen === 'picker' && <Picker onChoose={onChooseWallpaper} onClose={() => setScreen('settings')} />}
			{screen === 'homeMethod' && <HomeMethod info={systemInfo.homeMode || null} busy={modeBusy} onPick={onPickHomeMode} onClose={() => setScreen('settings')} />}
			{screen === 'calendarUrl' && <CalendarSetup sources={config.calendar.sources} canReadFiles={config.useHomebrew && isWebOS()} onChoose={onChooseCalendars} onClose={() => setScreen('settings')} />}
			{screen === 'feeds' && <FeedsSetup feeds={config.news.feeds} onChoose={onChooseFeeds} onClose={() => setScreen('settings')} />}
			{screen === 'country' && <CountryPicker current={countryOf(config.news.country, tvLanguageTag())} onChoose={onChooseCountry} onClose={() => setScreen('settings')} />}
			{WEATHER_ENABLED && screen === 'cities' && <CityPicker allowShell={config.useHomebrew && isWebOS()} onChoose={onChooseCity} onClose={() => setScreen('settings')} />}
			<Toast message={toast} />
		</>
	);
}
