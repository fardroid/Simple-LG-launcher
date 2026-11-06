# Setting up Simple LG launcher on a TV (for AI coding agents)

You are helping someone install this launcher on their own LG TV. Work through the steps below in order. Say in one line what you are about to do before each step. **Ask before every step that changes something on the TV** (steps 4, 5, 6 and 7) and before downloading anything (step 3), and wait for a yes. Everything here can be undone; the undo is given at each step.

Do not root the TV, change its firmware or touch anything outside what these steps describe. If the TV is not rooted yet, stop and point the person to https://www.webosbrew.org/; rooting depends on the model and firmware, and it is their call.

The launcher is a native app (C++, SDL). There is nothing to build: you install a ready-made package.

## 0. What to ask the person first

- The TV's IP address (TV: Settings → Network → the connection's details). Call it `TV` below.
- Is the TV rooted, with the Homebrew Channel installed and **SSH enabled** in the Homebrew Channel's settings?
- Optional, for later steps: Google Calendar secret iCal links (see step 6), and a folder of wallpaper pictures.

Never write the calendar links, the TV's IP address or any password into a file in this repository, a commit, an issue or a log you share.

## 1. Check the computer

```bash
ssh -V
```

`ssh` and `scp` are all that is needed.

## 2. Reach the TV

```bash
ssh -o ConnectTimeout=5 root@TV 'nyx-cmd OSInfo query webos_release; ls /media/developer'
```

- If this asks for a password, it is the one set in the Homebrew Channel (the default was `alpine`). Suggest copying a key so later steps don't ask again: `ssh-copy-id root@TV`. Let the person type any password themselves.
- `Connection refused`: SSH is off in the Homebrew Channel's settings, or the TV is in standby.
- Note the webOS release it prints. Making the launcher the home screen (step 5) has been tried on release 10.x (webOS 25). A release of 11 or higher is webOS 26: step 4 has one more command for it.

## 3. Get the package (ask first: it is a download)

The latest `tld.my.simplelauncher_<version>_arm.ipk` from https://github.com/fardroid/Simple-LG-launcher/releases, for example:

```bash
gh release download --repo fardroid/Simple-LG-launcher --pattern '*.ipk' --dir /tmp/launcher-ipk
```

Say the file name and size when you ask. Check it against the checksum in `repo/apps.json` of this repository (`ipkHash.sha256`, for the same version): `shasum -a 256 <file>`. If they differ, stop and tell the person.

## 4. Install (ask first)

```bash
scp -O /tmp/launcher-ipk/tld.my.simplelauncher_<version>_arm.ipk root@TV:/tmp/
ssh -tt root@TV "luna-send -w 60000 -i luna://com.webos.appInstallService/dev/install '{\"id\":\"com.ares.defaultName\",\"ipkUrl\":\"/tmp/tld.my.simplelauncher_<version>_arm.ipk\",\"subscribe\":true}' | grep -E 'installed|errorText' | head -1; rm -f /tmp/tld.my.simplelauncher_<version>_arm.ipk"
ssh -tt root@TV "luna-send -n 1 luna://com.webos.applicationManager/launch '{\"id\":\"tld.my.simplelauncher\"}'"
```

(`-tt`: `luna-send` prints only when ssh has a terminal.) The second command prints `"state":"installed"`; the third opens the launcher, and the person should see it on the TV. The first start takes a few seconds while it reads the app list. If a 1.x version of the launcher was installed, its settings, calendars and the way it opens on Home carry over by themselves at the first start: nothing to run.

**On webOS 26** (step 2 printed a release of 11 or higher) also run, once:

```bash
ssh -tt root@TV sh /media/developer/apps/usr/palm/applications/tld.my.simplelauncher/tools/webos26-access.sh install
```

Tell the person it changes one permission file of the launcher (it adds the Homebrew Channel's service group, which webOS 26 no longer gives apps) and leaves a small watcher running that adds the permission back after each update. Without it every row that needs root stays greyed out in Settings.

Undo: on webOS 26 first `sh …/tools/webos26-access.sh remove` (as above), then in the launcher Settings → Home screen → *Remove everything from the TV*, then uninstall "Simple LG launcher" from the Homebrew Channel, or run `ssh root@TV "luna-send -n 1 luna://com.webos.appInstallService/dev/remove '{\"id\":\"tld.my.simplelauncher\"}'"`.

## 5. Make it the home screen (ask first; explain the ways)

There are four ways. The person can pick one on the TV: Settings (the last tile on the row) → Home screen → *How the launcher opens*, which shows each with its pros and cons (OK twice switches); the launcher sets everything up itself, no SSH is needed. From here, use `homemode.sh`, which ships in the installed app. It switches off the way that is on, turns the new one on, checks it, and turns the old one back on if the new one does not take:

```bash
APP=/media/developer/apps/usr/palm/applications/tld.my.simplelauncher
ssh -tt root@TV sh $APP/tools/homemode.sh status        # mode=…, can_default=yes|no, release=…
ssh -tt root@TV sh $APP/tools/homemode.sh set default   # or: mount, none
```

Explain them in plain words before asking:

- **default: Home app** (recommended where `status` says `can_default=yes`, webOS 25 and newer). webOS's own choice of Home app, like choosing the default browser on a computer: the launcher is a Home app (`appCategory: ["home"]`) and is chosen with the TV's `setDefaultApp`. The Home button and Back out of the last app open it. For waking from standby it switches LG's *start with Home* off and names the launcher as the TV's last input (both put back when switched off), and keeps the launcher loaded through standby by adding it to the app manager's `keepAliveApps` (a config copy mounted in RAM at every start; the app manager is restarted once for it). Nothing of LG's is replaced, and LG Home keeps its own tile. Tell the person: after a full restart of the TV there is no webOS logo and the launcher settles about a minute later; and before uninstalling the launcher, set it back to `none`, otherwise the Home button opens nothing until the TV's next full restart.
- **mount: Swap** (for TVs that cannot choose). It mounts a small folder over LG Home's app folder, with links to the launcher's program and a manifest with LG Home's ID, so the TV starts the launcher as if it were LG Home. Nothing on the read-only system is changed. After a full restart the TV opens the launcher straight away. Tried on webOS 25.
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

Nothing needs to be run. Settings → Home screen → About → *Check for new versions* (on by default) looks on GitHub once a day; when there is a newer version, an *Update* tile appears in the row, and pressing it downloads, checks and installs the package. On a TV without root, or when the person prefers, repeat step 4 with the new package (ask first, as the first time): a launcher that stays loaded reloads itself the next time it comes on screen.

Check the version at the bottom of Settings → Home screen (the "Simple LG launcher" row).
