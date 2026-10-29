# Changelog

## 1.9.5

- **Switching the official way on could fail and undo itself.** It restarted the app manager first and then checked that Home opens the launcher, giving it 8 seconds; a launcher that has to start from scratch on a busy TV took longer, so the way was switched back off. Home is now checked before the restart, with 20 seconds.
- After the app manager is restarted at start-up, the launcher is opened by its own id: just after a restart the app manager still opens LG Home when asked for the Home app.

## 1.9.4

- **The official way, reworked: the TV wakes up straight into the launcher.** Choosing the launcher as the TV's Home app (`setDefaultApp`) covers the Home button and Back out of the last app, but waking from standby does not ask for it: the TV's start-up opens LG Home by name while LG's *start with Home* is on, and its last input otherwise. So the official way now also switches *start with Home* off and names the launcher as the TV's last input (`lastInputApp`, `physicalLastInputApp`; the trick of [openhome-webos](https://github.com/devic021/openhome-webos)). Opening an HDMI input names that input again, so the launcher names itself once more when it comes to the front and a few seconds after it goes to the back. Both settings are kept as they were and put back when the official way is switched off.
- **Kept loaded through standby.** When the TV goes to standby its app manager closes every app but those in `com.webos.applicationManager.keepAliveApps` (LG Home is one), so the launcher had to start from scratch after waking: 3 to 5 seconds of black. The official way mounts a copy of each config file that sets that list, with the launcher added, has the config service read them again and restarts the app manager. The launcher is now back about a second after waking. The copies live in RAM, so the boot link mounts them again at every start and restarts the app manager once: after a full restart the launcher shows up about a minute after switching on, after a short black screen. After an update, the first load of the launcher does the same once (the launcher closes and opens again).
- The wake-up guard of 1.9.3 (`tools/wakeguard.py`) is gone. A TV updated from 1.9.3 has it stopped and removed the first time the launcher loads.
- Settings → Home screen → *How the launcher opens*: the official way's description now says what it changes on the TV, and its pros and cons.

## 1.9.3

- **Official way: waking the TV.** The TV's start-up opens LG Home by name when it wakes from standby, whatever app is Home, and the Home key does nothing over it. A small guard (`tools/wakeguard.py`, started with the official way and at every full start) opens the launcher when the TV wakes up with LG Home in front. An app the TV woke up into (a console on HDMI, an app button on the remote) is left alone. LG's "start with Home" is left as it is.
- **Updating keeps the official way up to date.** Its scripts are copied out of the app (so they can give LG Home back if the app is uninstalled), and those copies did not change with an update. Each time the launcher is loaded it now refreshes them from the installed version and starts the guard (`defaulthome.sh refresh`; nothing happens when the official way is off). TVs updated from 1.9.2 get the guard this way, without switching anything.
- The weather and the calendar try again 15 s, 30 s and 1 minute after a failed fetch (it was 2 minutes), and at once when the TV's network comes back: right after the TV starts, they waited with empty cards.

## 1.9.2

- **Polish.** Thanks to [DriftEngineer](https://github.com/DriftEngineer), who translated it ([pull request #4](https://github.com/fardroid/Simple-LG-launcher/pull/4))! It comes with its own plural rule (1 zdjęcie, 3 zdjęcia, 5 zdjęć, 22 zdjęcia, but 21 zdjęć, not like Russian). The texts added in 1.9.2 were translated here; corrections are welcome.
- **How the launcher opens**, chosen on the TV: Settings → Home screen → *How the launcher opens* lists four ways, each with how it works, its pros and its cons. OK twice switches; the way that is on is switched off and checked first, and a way that does not take is undone (`tools/homemode.sh`, packaged in the app's `tools/` folder).
  - *Official*: webOS's own choice of Home app (`appCategory: ["home"]` and `setDefaultApp`, `tools/defaulthome.sh`; the method of Lumen's docs/native-home.md). Nothing mounted, and it lasts across a full restart. A boot link gives LG Home back if the launcher has been uninstalled.
  - *Swap*: `tools/takeover.sh`, as before. *Helper*: the Home key helper (version 18 stands down while the official way is on). *Off*.
- **Pixel shift (OLED)** (Settings → Appearance: Auto, On or Off): the clock, the cards and the apps move a few whole pixels once a minute round a loop of eight places (Hearth's idea). It follows the clock's own once-a-minute redraw, so it costs no timer. *Auto* (the default) switches it on when the TV's model name starts with OLED (asked once, `getSystemInfo` modelName).
- **App names: all, the selected one only, or none** (Settings → Appearance → App names). With the selected one only, the others keep their room, so nothing moves.
- **Sharper on the TV**: nothing with text is scaled any more but the focused card (Settings rows, pickers; the selected app's icon still grows, its name stays at its size), and the app row, the dialog and the toast sit on whole pixels (no `translate(-50%)`, rounded offsets). Text drawn at fractional positions or scales is soft through the TV's 2x scale-up of its 1080p picture.
- **Start-up picture** (Settings → Wallpaper): what the TV shows for a moment while the launcher starts after being unloaded: the wallpaper (the default), the logo card or black. The picture (`splash-wallpaper.jpg`) is written into the installed app's folder (as Luma does); the wallpaper's copy is the one the launcher already keeps for its own fast start. The Swap's copy of LG Home now shows it too (takeover.sh: `splashBackground` instead of `noSplashOnLaunch`).
- **The frosted glass changes with the wallpaper**: it starts to fade in the same frame as the picture (the plate sets its layer up while it renders, not after the next paint), and at start it comes from the cache with the picture instead of being worked out from it again a moment later. The plate's colour is drawn in the same two layers, so a light plate turns dark in the same fade instead of at once, and the new glass fades in over the old one (the sharp wallpaper no longer shows through half-way). The rim (the bright top edge) stays above both layers.
- **Frost** (Settings → Appearance, under Frosted glass): how much the glass blurs the picture behind it, 100% (as before) down to 30%. A change remakes the glass of the picture on screen at once; the saved glass for the fast start is kept per setting.
- **The clock's digits are centred in their card** by what the font really draws (measured on the canvas), not by where its line box puts them: the time and the date move together by whole pixels. Fonts with low digits (Cormorant, Roboto Mono, the TV's own) sat low before. Web views too old to measure text (Chrome 53) keep the layout as it was.
- **More of the screen on whole pixels**: the clock's date (the time's width is rounded up), Settings' headings, rows, values, buttons and pickers (whole-pixel line heights; button and value widths rounded up by `useSnapWidths`) and the toast (an even width).
- **Updates reach a launcher that stayed loaded**: each build writes its id to `assets/version.txt`; the launcher reads it whenever it goes to the background or is relaunched and loads itself again when another build is on disk (a rebuild of the same version too). Before, the TV kept the old version running (above all the Swap's copy behind the Home button) until it was fully restarted. An update from 1.9.1 or older still needs that one restart, as those versions don't check. `npm run install:tv` also closes the launcher and the Swap's copy, so the new version loads without restarting the TV; AGENTS.md says how to do the same after installing another way.
- The official way works on a TV whose list of default apps names no Home app (as out of the box: LG Home is then Home); it was refused with "could not read the TV's default apps".
- A switch of the way that fails turns the way that was on back on (it was left with none), and the Home key helper setting is changed only when a switch worked.
- Tests run on macOS too (`timeout` falls back to perl).

## 1.9.1

- **Twelve languages.** Spanish, French, German, Italian, Portuguese, Turkish, Ukrainian, Chinese (Simplified), Japanese and Korean join English and Russian. *Same as the TV* picks the TV's language when the launcher has it, English otherwise. Dates, month and weekday names come from the browser's own locale data; the city search answers in the chosen language.
- The texts of the other languages are dictionaries in `src/lib/locales/`, keyed by the English text; a text missing from one shows in English. `npm test` checks that every `tr()` text has an entry in every language.

## 1.9.0

- **Widgets.** The clock, the weather and the calendar are set up in one section, Settings → Widgets. Its first row, *Font*, now sets the font of all three: the clock, the date, and the weather and calendar cards.
- *Weight* goes the other way too: thin, light, regular, medium and bold.
- **The cards change colour together.** With *Card colour: Auto*, the clock, weather and calendar plates are all light or all dark. Which one depends on the brightest part of the picture behind any of them, so the text stays readable on each. Before, each plate picked its own, and a light one next to a dark one looked like a mistake.
- The language, "Reset look and clock" and About moved to Home screen; the General section is gone.
- Settings no longer closes by itself. Running in LG Home's place, the TV now and then relaunches its Home app with `{"keepAlive": true}` only to keep it loaded; the launcher took that for the Home key and went back to its home screen.
- The wallpaper, city and calendar pickers have the look of Settings: one rounded group of rows on the dimmed wallpaper, with Settings hidden behind them. The text typed into the city or calendar link field is dark on its light row (it was white on white).
- The focused tile on the left of the Apps and Inputs grids is no longer cut off.
- `wallpapers/`: thirteen wallpapers drawn for the launcher, to copy to the TV.

## 1.8.0

- **New Settings.** The sections are listed on the left, each with its own colour, and the chosen one opens on the right in rounded groups. The focused row is a light pill, and its explanation sits at the foot of the screen. ↑ ↓ pick a section, → or OK open it, Back goes back. The pickers (wallpaper, city, calendars) and the app menu share the look.
- Rows show only where they apply: the slideshow interval and shuffle only with a slideshow, frosted glass only with a picture, the date size only with the date on.
- **Home screen** (was Options) says whether the launcher is in LG Home's place (tools/takeover.sh). The background helper is offered only when it is not, as one switch that also starts it with the TV. "Close LG Home now" and "Close LG Home at start" are gone: with the takeover the first would have closed the launcher itself. LG Home is now closed when the launcher opens only together with the helper.
- The colour theme moved to Wallpaper; the language, "Reset look and clock" and the version moved to a new General section.
- Removed the code of options that were no longer shown (the TV input, the LG logo switch, the webOS 5 home-app setting).

## 1.7.0

- **New name: Simple LG launcher**, with a new icon (a frosted card with a clock over a row of apps, the chosen one larger). New app ID too, `tld.my.simplelauncher`, so it no longer collides with Hearth. It installs as a separate app; a launcher that has no settings of its own yet takes them from the copy kept on the TV (/media/developer/launcher-config.json). The Home key helper (version 17) and tools/takeover.sh now look for the new ID; on a TV set up with the old one, run `takeover.sh disable` with the old script, then `enable` with the new one.
- **English or Russian.** Settings → Look → Language: *Same as the TV* (the default: Russian when the TV is set to Russian, English otherwise), *English* or *Русский*. Every screen, message, date and weather word follows it. In English the wind is in km/h (mph with Fahrenheit), and the city search answers in English.
- The preset cities have English and Russian names; Milan is on the list.
- The calendar card shows up to five events of the day before "and N more" takes the last line; it used to stop at three with room to spare.
- In the desktop preview (`npm run dev`) wallpaper pictures from the project folder load through the dev server.
- README rewritten in English, with new screenshots and setup steps; AGENTS.md walks an AI coding agent through installing the launcher on a TV.

## 1.6.0 to 1.6.3

- **The launcher can take LG Home's place** (tools/takeover.sh, the method of Glasshouse Home and Luma): a folder with the launcher's files and an appinfo.json with LG Home's id is bind-mounted over LG Home's app folder, and the TV's app manager reads that app again. The TV then opens the launcher itself for the Home key, Back out of the last app and waking up, with nothing to correct afterwards, and keeps it in memory as it does LG Home. LG's files are not changed; `disable` gives LG Home back. A boot link puts it back in place after a full start (some 40 s in; LG Home shows until then).
- Running as the TV's Home the launcher does not start the Home key helper or close "LG Home" (that is itself now), and has no "LG Home" tile. The helper (version 16) stands down while the takeover is on.
- LG Home's bus permissions do not include the Homebrew Channel's root service (the full app list, wallpapers, calendar links and the settings file depend on it). `enable` bind-mounts a copy of LG Home's permissions file with that service added over the system one and makes the bus rescan; `disable` unmounts it. (A file in the developer folder is ignored for a system app's id.)
- Running as the TV's Home the launcher always starts from the settings kept on the TV, the latest of both copies.
- The settings are also kept in a file on the TV (/media/developer/launcher-config.json), and a copy of the launcher without settings of its own (the first start as the TV's Home) takes them from there.

## 1.5.9

- **LG Home does not start at all on wake-up any more**: LG's "start with Home" (homeAutoLaunch) is off, and the Home key helper (version 15) opens the launcher itself as soon as the TV is back on, whatever was on screen before, unless an HDMI input is in front (a console that switched the TV on).

## 1.5.8

- Back to the launcher after every wake-up: the app from before the standby is no longer brought back (the TV starts LG Home when it wakes, and the helper replaces it with the launcher, as with the Home key). Helper version 14 no longer follows the power state.

## 1.5.7

- **Waking up brings back the app that was on screen.** The TV wakes from standby into LG Home (LG's "start with Home" is back on, and the launcher is no longer one of the TV's inputs, which is what put "Hearth" on screen at wake-up). The Home key helper (version 13) notes the app in front as the TV goes to standby; when LG Home starts within a minute of the wake-up, that app (still in memory) is brought back instead and LG Home is closed before it shows. If the launcher was in front, the launcher opens.

## 1.5.6

- After a wake-up the launcher is no longer opened over the app that was on screen. Just after waking the TV reports nothing in front for a moment, and the helper took that for an empty screen; now only LG Home or the TV tuner in front make it open the launcher (helper version 12).

## 1.5.5

- **Going to standby is left alone again**: the launcher is no longer brought to the front as the TV switches off, so the TV wakes up with what was on screen (Netflix stays Netflix). After the wake-up the launcher only replaces LG Home or the TV tuner.
- **A full start of the TV goes straight into the launcher.** The launcher is one of the TV's inputs (named «Главный экран») and the Home key helper (version 11) makes it the TV's last input each time it comes to the front, so with homeAutoLaunch off the TV opens it at start-up by itself, without waiting for the helper (which starts some 40 s later).
- No splash screen when the launcher starts (noSplashOnLaunch): the "Hearth / loading" card is gone; the launcher draws its saved wallpaper as soon as it starts.

## 1.5.4

- **For use with LG's "start with Home" switched off** (homeAutoLaunch off; the TV then comes back on showing whatever was in front). The Home key helper (version 10) follows the TV's power state: as the TV starts switching off (standby or power-off, not "screen off" with the sound on) it brings the launcher to the front, so the TV sleeps and wakes up with it. After the TV is back on, and once just after a full start, it opens the launcher if LG Home, the TV tuner (Live TV, black without an aerial) or nothing is in front; an HDMI input or another app is left alone.

## 1.5.3

- **LG Home is caught before it reaches the screen also when it starts without the Home key**, above all when the TV is switched on (it wakes from standby and starts LG Home). The Home key helper (homekey.py, helper version 9) now also follows the TV's reports of apps starting (getAppLifeEvents): LG Home's "splash" comes about a second before it is on screen, and the launcher is asked for and LG Home closed at that moment, as for the key. Starts in the background (preloads) show no splash and are left alone. Idea from webos-debloat by lucataco.
- **tools/tv-tweaks.sh**: a one-off for the TV (apply / restore / status). Stops preloading LG Channels and the browser (more free memory, so the TV unloads the launcher less often) and switches off LG's promotions, sports alerts, recommendations and Live Plus (ACR). It keeps the old values and puts them back with restore. The launcher itself cannot be preloaded: the TV's preload manager only takes apps from LG's own read-only list.

## 1.5.2

- The selected app now grows and shrinks in one smooth movement. Before, the point it grew from jumped back to the icon's centre the moment it was no longer selected, so it first seemed to drop and only then shrank.
- The name under the selected app grows with the icon (the whole tile is scaled: the icon upwards, the name downwards). The name no longer turns bold, which made it jump; it just turns brighter.

## 1.5.1

- **No more Mac-style dock.** The row of apps no longer magnifies the selected app and its neighbours (nor follows the pointer with a wave); the selected app simply grows a little (x 1.12, from its bottom edge, so it does not cover its name), and the others stay as they are. The pointer still selects the app under it.
- The widgets block is placed for the new size of the selected app: 74 px above and below it at the default settings.

## 1.5.0

Settings tidied up.

- **New: Look → «Тень карточек».** Turns off the soft shadow under the clock, the weather and the calendar (the TV then has a little less to draw).
- **Removed: «Прозрачность виджетов»** (it only ever changed the clock), and **«Рядов приложений»**: two rows of apps leave no room for the cards, so the home row is always one row.
- **Removed: «Показывать AM / PM»**: in 12-hour mode AM / PM is always shown.
- **Moved and renamed: «Цвет виджетов» is now «Цвет часов» on the Clock tab**, since it only colours the time and the date.
- **Options tab: only what this fork uses.** The Home helper, starting it with the TV, and closing LG Home stay. Removed: «Список приложений через Homebrew Channel» (this fork needs it on), the experimental «домашнее приложение ТВ» method for webOS 5, «Добавить лаунчер как вход ТВ» (it is what put «Hearth загрузка» on the start-up screen) and the hidden LG logo switch. Settings saved earlier keep working; the removed ones simply stay as they were.

## 1.4.9

- **Smooth scrolling along the row of apps again.** The diagnostics added in 1.4.1 to 1.4.5 are removed. One of them replaced Preact's scheduling of redraws (normally at once, in the same turn of the event loop) with a timer, to time each redraw; that let the screen be drawn before the change from a key press was applied, so the movement of the row came a frame or more late and looked jerky. The others (a 25 ms timer for 8 s after every return, a frame watch after every key) are gone too.

## 1.4.8

- With the remote on the weather or the calendar, the app icons no longer get a light outline: the row keeps its dock look (no outlines, soft shadows) when nothing in it is magnified.

## 1.4.7

- The white glow around the selected app icon is gone (its shadow stays).
- The block of the clock, the weather and the calendar keeps its size and is moved down so that the space above it (to the top of the screen) and the space below it (to the top of the biggest the selected icon gets, the magnified one) are the same: 56 px each at the default settings (it was 40 above and 72 below). The position follows the icon size, the bottom margin, the names under the icons and the number of rows, so the two spaces stay equal when these are changed in Settings.

## 1.4.6

- **The clock, the weather and the calendar are no longer rebuilt on every key press and every move of the pointer** (they are redrawn only when what they show changes), and the calendar keeps its month grids, the dots under the days and the day's events instead of working them out again each time. Measured on the TV with the browser's remote profiler: the page's script time during a sweep of the pointer along the row of apps dropped to about a third, and the calendar's functions disappeared from the top of the list.

## 1.4.5

- Coming back to the launcher keeps the selection on the app row where it was (the app you have just used stays selected); only the weather and calendar cards go back to today. The focus goes back to the row.
- The clock does not redraw the screen when the launcher comes back and the minute is the same.
- The log (`/tmp/launcher-homehook.log`) now also gets a line every 20 s of everyday use, if there was any: how many redraws took over 60 ms and which key presses needed over 150 ms to show on the screen.

## 1.4.4

- **The whole home screen was being redrawn every second** (the clock's tick re-rendered the row of apps, the weather and the calendar, each time taking 60 to 180 ms on the TV, so the remote kept running into a busy page). It is now redrawn only when the minute changes. The watch no longer counts the time a page spends in the background as a stall.

## 1.4.3

- **The remote no longer reacts late after coming back to the launcher.** The log showed the slideshow loading its next picture 1.5 s after the return, which kept the page busy for about half a second, just when the remote is used. Now (a) the next picture is not loaded until 12 s after the return, and (b) the screen-sized copy of the picture for the startup cache is encoded only when the launcher goes to the background (2.5 s after, not at the very moment another app starts), not when the picture appears.

## 1.4.2

- The same watch now also notes what the launcher was doing in those seconds (changing the wallpaper and the steps of it, reading the app list, the weather and the calendar, and every re-drawing of the screen that takes over 60 ms).

## 1.4.1

- The first 8 seconds after the launcher comes back to the front are now watched, and one line goes into the helper's log (`/tmp/launcher-homehook.log`): the longest time the page's thread was stuck and when the first remote key arrived. It is for finding why the remote sometimes reacts late after leaving YouTube or Netflix.

## 1.4.0

- **Frosted glass.** The plates behind the clock, the weather and the calendar now blur the wallpaper under them, like frosted glass, instead of showing its lines as they are. A small blurred copy of the picture (480 x 270, a few KB, made once per picture on the TV) lies under each plate, shifted so it lines up with the wallpaper behind; nothing is blurred live, so the TV has nothing extra to redraw. The blur keeps the picture's own colours (no extra saturation), so the plates do not glow. A bright rim on top and a faint one all round give the glass its edge. When the wallpaper changes, the glass and the plate's colour cross-fade with the same length and curve as the wallpaper itself. It works whenever a picture is the wallpaper; Look → "Матовое стекло" turns it off.
- The plate is now a small component of its own (`Frost`) under the card's contents: two cross-fading layers of glass, the colour and the rim.

## 1.3.5

- The dock's magnification now follows the pointer exactly: the tile under the pointer is found from where the tiles really are on the screen (the magnified tiles push the others aside, which the earlier calculation did not take into account).
- The brightening of a card that has the focus is much weaker.

## 1.3.4

- **The dock follows the pointer.** With the Magic Remote's pointer over the row of apps the magnification follows it continuously (the tile under the pointer is the largest, the neighbours fall off smoothly), instead of jumping from one tile to the next. The row no longer slides away under the pointer: it scrolls with the arrow keys and the remote's wheel.
- The weather card is taller again, and the extra height goes to the day tiles (more room around their text). The clock card is a little lower in height; the row of apps sits 10 px lower.
- Moving the focus onto a card grows it slightly (transform only; its shadow is untouched) together with the brighter plate.

## 1.3.3

- Smaller glow around the selected tile and more room for it around the row (no more visible edge); the row sits 10 px lower again.
- The weather card is taller and its hourly curve is more pronounced; the "today high/low" line under the city is gone (the day tile has it) together with its setting.
- When the focus is on a card, the row of apps returns to normal size (the magnification goes away, like a Mac dock when the pointer leaves it).
- Moving the focus to a card is cheaper to draw (only an overlay fades in, no shadow or size change), which should be faster and smoother on the TV.

## 1.3.2

- **The Home button opens the launcher at once, and LG Home no longer flashes.** A small Python helper (`homekey.py`, started with the watcher) reads the remote's Home key straight from `/dev/input` (the TV keeps getting it too), asks for the launcher in the same moment (about 0.2 s) and cancels the start of LG Home with a few quick requests, so LG Home does not get to the front. If it still gets through, the watcher opens the launcher again, as before. Needs a rooted TV with Python 3 (the TV has it).

## 1.3.1

- The selected tile's glow is no longer cut off above the home row (more room around the row); the empty margin around the row lets clicks through to the cards.
- Moving the focus to the weather and calendar cards is three times faster.
- The month view comes down from above and pushes the days down and out (one vertical move), and goes back the same way, instead of fading.
- Today stays marked in the accent colour in the calendar's strip of days when another day is chosen.
- The weather card is taller (and the board with it) so the day tiles have room around their text.

## 1.3.0

**New**
- **Plates that adapt to the wallpaper** (Settings → Look → Plates: Auto / Light / Dark). In Auto the colour of each plate (clock, weather, calendar) is chosen
  from the brightness of the wallpaper right behind it: light on dark, dark on light. The wallpaper is measured once, when it appears (a 16 x 9 grid), so it costs nothing while the home screen runs.
  Plates have a soft shadow and no outlines; the selected tile has a glow instead of a ring.
- **Month view** in the calendar (Up from the day view), with dots of the calendars' colours; days and months slide as one strip, without fading.
- **Several calendars at once**: web addresses, files and folders of `.ics` files, each with its own colour; repeating events are handled.
- **Weather**: choose a day of the week (← → on the weather card) to see its hourly curve.
- **Clock card** with the day and date hanging from the top of the digits; one text size for the weather and calendar cards (Look → Text size of the cards).
- Wider gaps (40 px) between the cards; the cards are centred as one group.

**Changed**
- Removed the clock position settings: the clock sits at the top of the left column.

## 1.2.0

A fork with a new home screen layout, in Russian.

**New**
- **Weather card:** now, today's temperature curve (every 3 hours, with a marker for the present moment) and the next 7 days, each with an icon and the day and night temperature.
  Feels-like and wind under the temperature. Text size is a setting (Weather tab).
- **Calendar card:** the events of one day from a Google Calendar (or any iCal feed) address. Up from the apps row moves to the card, ← → change the day, OK returns to today.
  Repeating events, exceptions, moved events and all-day events are handled. The address can be typed with the on-screen keyboard or read from `/media/developer/calendar.txt`.
  Google does not let web pages read the feed, so it is fetched through the TV's shell (rooted TV with the Homebrew Channel). Text size is a setting (Calendar tab).
- **Dock-style apps row:** the selected tile grows and its neighbours a little less; tiles and their names are larger.
- **Date next to the time**, in Russian, with its own size setting (Clock tab). 24-hour time by default.
- **Evening sky** theme (default), Cyrillic letters for Inter, Oswald, Roboto Mono and Cormorant Garamond.
- All text is in Russian.

**Changed**
- The clock's position setting now sets its left edge (0 to 70 %), not its centre.

## 1.1.0

The launcher is now called **Hearth**, with a new icon, and it is faster, easier to set up and much more complete.

**New**
- **Weather** under the clock: temperature, conditions and today's high and low, from [Open-Meteo](https://open-meteo.com) (no account, no key). Choose a city from a list of about 75 or search for any place
  in the world. Celsius or Fahrenheit. It is read from a saved copy at start-up and refreshed in the background, so it never slows the home screen down.
- **Widget colour and opacity** (Settings → Wallpaper) for the clock, date and weather: white, warm white, ice blue, amber, grey, dark grey, black and three gradients (Ember, Ocean, Dusk), at 10 to 100 %.
  Readable on light pictures, and a description explains why.
- **Six clock fonts** (Inter, TV font, Poppins, Oswald, Roboto Mono, Cormorant Garamond).
- **Add Hearth as a TV input** so the TV can start straight into it (Options → Add the launcher as a TV input).
- **LG logo switch:** Options → *Show the LG logo when the TV starts* reads and changes LG's own (normally hidden) setting.
- **Refresh list** button in Settings → Apps, to find newly installed apps at once.
- **Move to end** in an app's menu (hold OK), **System Settings** tile, and **LG Home** moved before it.
- **Back on the home row** jumps to the first app.
- Icon size down to 50 % and side margin up to 300 px. **Reset look and clock** moved to the Look tab.
- **Install and update through the Homebrew Channel:** add this repository once and the Homebrew Channel shows new versions (see the README).

**Faster and smoother**
- The app list is checked and reloaded in the background a few seconds after Hearth appears, not at start-up, and merged into fewer calls. The saved full app list and its icons are never replaced by the small built-in list.
- Smoother scrolling along the row (only movement and fade are animated).
- The Home-button helper now starts about a second after Hearth appears, which removes the short flash of LG Home after boot, even with *Start the launcher helper at boot* switched off.

**Clearer messages and non-rooted TVs**
- Every failure now says what failed and why (service not running, no root, refused, too slow, or the command's own error), with a *Try again* in the wallpaper picker.
- Options that need a rooted TV are **greyed out with the reason** when the TV cannot run commands, so a non-rooted TV still gets the core launcher.

**Fixes**
- Settings → Options showed only its first row on some TVs (seen on webOS 6): the list now measures the real screen.
- Launching an app could leave the launcher reappearing in a loop until the TV was restarted: the helper no longer feeds back on itself.
- TV input add and remove now work (the app declares the `supportGIP` capability).
- After a restart the saved full app list and icons are kept when the Homebrew service is not up yet.
- Options order: TV input, live app list, helper (the "experimental" labels were removed from those three); the alternative home-app method is last.

**Tested on:** LG firmware 03.01.16 (webOS 8.0.0) and newer, webOS 6.x and webOS 4.x. Newer versions should work.

Co-authored-by: Claude (Anthropic) <noreply@anthropic.com>

## 1.0.0

First release of this fork of [partridgeworks/webos-launcher](https://github.com/partridgeworks/webos-launcher).

**Home screen**
- A centre clock (12-hour with a small AM/PM, or 24-hour) with the date, and one row of apps along the bottom, plus **LG Home** and **Settings** tiles.
- **OLED black** (default) or a calm blue-grey theme.
- Everything adjustable from the remote: clock size, thickness, colour, vertical and horizontal position, and whether to show the date and AM/PM;
  icon size; side and bottom margins; app order and which apps and HDMI inputs appear.

**Wallpaper**
- Plain background, one picture, or a **slideshow from a folder on a USB drive or the TV's storage**, with a folder browser, shuffle and an interval of 10 seconds to 1 hour (including 30 minutes).
- Pictures are read through the Homebrew Channel (the TV's web view cannot open files), shrunk if far larger than the screen, and skipped with a clear
  message if too big or not a picture.

**Speed and memory**
- The app list and the last wallpaper are cached, so the launcher draws its full screen at once; a black splash replaces the grey flash.
- While in the background the launcher lets go of its wallpaper and stops its clock.
- Optional **Close LG Home when the app opens**, and **Close LG Home now** (shows the memory freed).

**Experimental: making the launcher the TV's default**
- **Make this launcher the default instead of LG Home:** a small background helper opens the launcher whenever the TV brings LG Home to the front
  (Home, Back, boot). Nothing is replaced or mounted. **Start the launcher helper at boot** is a separate switch (one link in the startup folder).
  After opening the launcher the helper checks that it really appeared and asks again if not. The LG Home tile still opens LG's own Home.
  It writes a log and a `diagnose.sh` report for troubleshooting (see `src/homehook/README.md`). LG Home is never ended while it is the app in front.
- **Alternative method:** the TV's own home-app setting, which may only work on webOS 5 and newer (it did not on webOS 4).

**Targeted at older webOS:** built for, and tested on, an LG TV on webOS 4.x with the Homebrew Channel. Newer versions are untested.
