# Setting up Simple LG launcher on a TV (for AI coding agents)

You are helping someone install this launcher on their own LG TV. Work through the steps below in order, from a clone of this repository on their computer. Say in one line what you are about to do before each step. **Ask before every step that changes something on the TV** (steps 4, 5, 6 and 7), and wait for a yes. Everything here can be undone; the undo is given at each step.

Do not root the TV, change its firmware or touch anything outside what these steps describe. If the TV is not rooted yet, stop and point the person to https://www.webosbrew.org/; rooting depends on the model and firmware, and it is their call.

## 0. What to ask the person first

- The TV's IP address (TV: Settings → Network → the connection's details). Call it `TV` below.
- Is the TV rooted, with the Homebrew Channel installed and **SSH enabled** in the Homebrew Channel's settings?
- Optional, for later steps: Google Calendar secret iCal links (see step 6), and a folder of wallpaper pictures.

Never write the calendar links, the TV's IP address or any password into a file in this repository, a commit, an issue or a log you share. The TV's address may go in `tv.env`, which git ignores.

## 1. Check the computer

```bash
node --version    # 18 or newer
ssh -V
```

Install Node.js if it is missing (ask first). Then `npm ci` in the repository.

## 2. Reach the TV

```bash
ssh -o ConnectTimeout=5 root@TV 'nyx-cmd OSInfo query webos_release; ls /media/developer'
```

- If this asks for a password, it is the one set in the Homebrew Channel (the default was `alpine`). Suggest copying a key so later steps don't ask again: `ssh-copy-id root@TV`. Let the person type any password themselves.
- `Connection refused`: SSH is off in the Homebrew Channel's settings, or the TV is in standby.
- Note the webOS release it prints. The ways of step 5 have been tried on release 10.x (webOS 25). On an older release, tell the person that; the helper is the safest there.

Write `TV_HOST=root@TV` to `tv.env` (copy `tv.env.example`).

## 3. Build

```bash
npm test
npm run build && npm run package
```

This makes `build/tld.my.simplelauncher_<version>_all.ipk`. All tests should pass, on Linux and macOS; a failure is worth reading before going on.

## 4. Install (ask first)

```bash
npm run install:tv
```

It copies the package over SSH, installs it with the TV's developer installer and opens it. The person should now see the launcher on the TV. The first start takes a few seconds while it reads the app list.

Undo: uninstall "Simple LG launcher" from the Homebrew Channel, or run `ssh root@TV "luna-send -n 1 luna://com.webos.appInstallService/dev/remove '{\"id\":\"tld.my.simplelauncher\"}'"`.

## 5. Make it the home screen (ask first; explain the ways)

There are four ways, and the person can also pick one on the TV: Settings (the last tile on the row) → Home screen → *How the launcher opens*, which shows each with its pros and cons (OK twice switches). From here, use `homemode.sh`, which ships in the installed app. It switches off the way that is on, turns the new one on, checks it, and turns the old one back on if the new one does not take:

```bash
APP=/media/developer/apps/usr/palm/applications/tld.my.simplelauncher
ssh -tt root@TV sh $APP/tools/homemode.sh status        # mode=…, can_default=yes|no, release=…
ssh -tt root@TV sh $APP/tools/homemode.sh set default   # or: mount, none
```

(`-tt`: the scripts ask the TV's app manager through `luna-send`, which answers only when ssh has a terminal.)

Explain them in plain words before asking:

- **default: Official** (recommended where `status` says `can_default=yes`, webOS 25 and newer). webOS's own choice of Home app, like choosing the default browser on a computer: the launcher is a Home app (`appCategory: ["home"]`) and is chosen with the TV's `setDefaultApp`. The Home button, Back out of the last app and waking up open it. Nothing of LG's is replaced or mounted, it lasts across a full restart, and LG Home keeps its own tile. Tell the person: before uninstalling the launcher, set it back to `none`; otherwise the Home button opens nothing until the TV's next full restart (a boot script then gives LG Home back).
- **mount: Swap** (for TVs that cannot choose). It bind-mounts a folder over LG Home's app folder and a copy of LG Home's permissions file with the Homebrew Channel's root service added, so the TV opens the launcher as if it were LG Home. Nothing on the read-only system is changed. After a full restart, LG Home shows for about 40 seconds before the launcher is put back. Tried on webOS 25 only.
- **The Home button helper.** A script that opens the launcher over LG Home whenever LG Home comes up. Lighter, mounts nothing, but LG Home can flash for a moment. It is switched on from the TV only (the same screen: *Helper*); tell the person which keys to press, you can't press them for them.
- **none: Off.** Home opens LG Home; the launcher opens from its tile.

The last line of `set` is `ok <way>` or `failed <way>: <why>`. Then have the person press Home: it should open the launcher. Undo: `set none` (or *Off* on the TV).

## 6. Calendars (optional; ask first)

Ask for the calendar links. In Google Calendar on a computer: Settings → the calendar → *Secret address in iCal format*. They start with `https://calendar.google.com/calendar/ical/` and end in `basic.ics`. Write them to a file on the TV, one per line, straight from the shell without saving them in the repository:

```bash
ssh root@TV 'cat > /media/developer/calendar.txt' <<'EOF'
<link 1>
<link 2>
EOF
```

Then the person chooses, on the TV: Settings → Widgets → Calendars → *Read the list from a file*, and turns on *Show the calendar*. Undo: *Remove all calendars* on the same screen, and `ssh root@TV rm /media/developer/calendar.txt`.

## 7. Wallpapers and tweaks (optional; ask first)

Pictures: JPG or PNG, 1920×1080 to 3840×2160, under 8 MB each. The repository's `wallpapers` folder has a set to start with; offer it if the person has none of their own.

```bash
ssh root@TV mkdir -p /media/developer/wallpapers
scp -O wallpapers/*.jpg root@TV:/media/developer/wallpapers/    # or the person's own pictures
```

On the TV: Settings → Wallpaper → Picture → *TV storage* → `wallpapers` → *Slideshow* (the first tile).

`tools/tv-tweaks.sh` stops some preloading and LG's promotions and Live Plus. Describe it first, as README.md does, and run it only on a yes: `scp -O tools/tv-tweaks.sh root@TV:/media/developer/ && ssh root@TV sh /media/developer/tv-tweaks.sh apply`. Undo: `… tv-tweaks.sh restore`.

## 8. Finish

Tell the person what was done, what is left for them on the TV (the city for the weather: Settings → Widgets → City; the language: Settings → Home screen → Language), and how to undo each step.

## Updating later

`git pull`, then steps 3 and 4 again (ask before step 4, as the first time). The way chosen in step 5 stays on and needs nothing more.

The TV keeps the launcher loaded in memory, and with the Swap on, also its copy behind the Home button (it runs as `com.webos.app.home`). A copy that stays loaded goes on showing the old version, so after an update close both: that is enough, the TV does not need a restart. `npm run install:tv` does it itself. After installing another way (the Homebrew Channel, `ares-install`), close them yourself:

```bash
ssh root@TV "luna-send -n 1 luna://com.webos.applicationManager/closeByAppId '{\"id\":\"com.webos.app.home\"}'; luna-send -n 1 luna://com.webos.applicationManager/closeByAppId '{\"id\":\"tld.my.simplelauncher\"}'"
```

The next Home press loads the new version. (From 1.9.2 on, a copy left loaded also reloads itself the next time it goes to the background: it compares its build with `assets/version.txt`.)

Then press Home and check the version at the bottom of Settings → Home screen (the "Simple LG launcher" row).

Over SSH, `luna-send` to the app manager prints nothing unless ssh has a terminal: use `ssh -tt` when you need to read its answer.
