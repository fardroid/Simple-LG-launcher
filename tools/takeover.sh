#!/bin/sh
# Puts the native launcher in LG Home's place (the Swap), the way ecklf/lg-luma does it on webOS 10/11 (tv-helper/src/home.rs):
# a folder with an appinfo.json of LG Home's id (com.webos.app.home, type native) and links to the installed launcher's files
# is bind-mounted over LG Home's app folder, and the TV's app manager is made to read that one app again (blockSystemApp,
# then releaseBlockSystemApp). From then on everything that means "Home" starts the launcher: the Home key, Back out of the
# last app, waking up. LG's files are not changed: unmounting (disable, or a full restart without the boot link) gives LG Home
# back.
#
#   sh takeover.sh enable    put the launcher in place now, and again at every full start (boot link)
#   sh takeover.sh disable   LG Home back, boot link removed
#   sh takeover.sh apply     what the boot link runs (only when enabled)
#   sh takeover.sh status
#
# Unlike the web version's Swap, LG Home's bus permissions are left alone: the program is a link to the installed one, so the
# bus knows it by its own id, with its own permissions. As Home it names its window and registers with the app manager under
# LG Home's id itself (src/main.cpp, register_as_home).
#
# This file is the native version's own (scripts/gen-tools.sh copies it with the id and folders of the build).

DIR=/media/developer/launcher-takeover
STAGE=$DIR/app
FLAG=$DIR/enabled
APP_ID=tld.my.simplelauncher
LAUNCHER=/media/developer/apps/usr/palm/applications/tld.my.simplelauncher
PROGRAM=simplelauncher                   # the launcher's program (appinfo.json "main")
HOME_ID=com.webos.app.home
APPDIR=/usr/palm/applications/$HOME_ID
LINK=/var/lib/webosbrew/init.d/46-launcher-takeover
HOOK=/media/developer/launcher-homehook
HOOK_LINK=/var/lib/webosbrew/init.d/48-launcher-homehook
LOG=/tmp/launcher-homehook.log
SAM=luna://com.webos.applicationManager

log () { echo "$(date '+%H:%M:%S') takeover: $*" | tee -a "$LOG"; }

mounted () { grep -q " $APPDIR " /proc/mounts; }

# Is the launcher what the app manager has for Home now (its native manifest, not LG's)?
is_ours () { luna-send -n 1 "$SAM/getAppInfo" "{\"id\":\"$HOME_ID\"}" 2>/dev/null | grep -q '"main"[[:space:]]*:[[:space:]]*"[^"]*'"$PROGRAM"'"'; }

# The app manager reads one app's appinfo.json again: block it and release it at once.
reread () {
	luna-send -n 1 "$SAM/blockSystemApp" "{\"id\":\"$HOME_ID\"}" >/dev/null 2>&1
	i=0
	until luna-send -n 1 "$SAM/releaseBlockSystemApp" "{\"id\":\"$HOME_ID\"}" 2>/dev/null | grep -q '"returnValue":true'; do
		i=$((i + 1)); [ "$i" -ge 3 ] && break; sleep 1
	done
}

wait_ours () {    # yes | no: wait up to 8 s for the app manager to report it
	i=0
	while [ "$i" -lt 8 ]; do
		if [ "$1" = yes ]; then is_ours && return 0; else is_ours || return 0; fi
		sleep 1; i=$((i + 1))
	done
	return 1
}

# LG's own Home: on webOS 10 the Flutter runner with its id.
home_running () { luna-send -n 1 -w 5000 "$SAM/running" '{}' 2>/dev/null | grep -q "\"id\":\"$HOME_ID\""; }

# LG's own Home, closed through the app manager, so that it knows it is gone: a Home whose process was only killed stayed in its
# list of running apps, and the launcher as Home waited behind it with a black screen (seen on webOS 10.3.2, just after a full
# start). Its process (on webOS 10 the Flutter runner with its id) is killed only if it does not close, and then it is closed again.
stop_lg_home () {
	home_running || return 0
	luna-send -n 1 -w 8000 "$SAM/closeByAppId" "{\"id\":\"$HOME_ID\"}" >/dev/null 2>&1
	i=0; while home_running && [ "$i" -lt 5 ]; do sleep 1; i=$((i + 1)); done
	home_running || { log "closed LG Home"; return 0; }
	for p in $(ls /proc | grep -E '^[0-9]+$'); do
		cmd=$(tr '\0' ' ' < "/proc/$p/cmdline" 2>/dev/null)
		case "$cmd" in
			*flutter-client*" -i $HOME_ID"*|"/usr/bin/$HOME_ID"*) kill "$p" 2>/dev/null && log "stopped LG Home (pid $p)" ;;
		esac
	done
	sleep 1
	luna-send -n 1 -w 8000 "$SAM/closeByAppId" "{\"id\":\"$HOME_ID\"}" >/dev/null 2>&1
}

front () { luna-send -n 1 "$SAM/getForegroundAppInfo" '{}' 2>/dev/null | sed -n 's/.*"appId"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p'; }

stage () {
	[ -f "$LAUNCHER/appinfo.json" ] || { log "the launcher is not installed"; return 1; }
	mkdir -p "$STAGE"
	# everything of the launcher's, linked: the program too, so the bus knows it by its own path (and its own id), and an
	# update of the launcher reaches Home at its next start (the start-up picture it writes is the one the TV shows)
	for f in "$LAUNCHER"/*; do
		name=$(basename "$f")
		[ "$name" = appinfo.json ] && continue
		ln -sfn "$f" "$STAGE/$name"
	done
	cat > "$STAGE/appinfo.json" <<EOF
{
  "id": "$HOME_ID",
  "version": "1.0.0",
  "vendor": "tld.my.simplelauncher",
  "type": "native",
  "main": "$PROGRAM",
  "title": "Home",
  "icon": "icon.png",
  "largeIcon": "largeIcon.png",
  "iconColor": "#5245d0",
  "bgColor": "#000000",
  "visible": false,
  "splashBackground": "splash-wallpaper.jpg",
  "appCategory": ["home"]
}
EOF
	# (no nativeLifeCycleInterfaceVersion, as lg-luma's: the registration as LG Home is registerNativeApp, which the app manager
	# refuses under a version 2 manifest; with an explicit 1 it did not start the launcher at all, seen on webOS 10.3.2)
}

give_back () {
	if mounted; then umount "$APPDIR" 2>/dev/null || umount -l "$APPDIR"; fi
	reread
	wait_ours no || log "the app manager has not reported LG Home back yet"
	# the launcher running as Home goes (LG Home starts in its place when Home is next asked for)
	luna-send -n 1 "$SAM/closeByAppId" "{\"id\":\"$HOME_ID\"}" >/dev/null 2>&1
}

take () {
	stage || return 1
	if ! mounted; then
		mount --bind "$STAGE" "$APPDIR" || { log "the mount did not take"; return 1; }
	fi
	if ! is_ours; then
		reread
		if ! wait_ours yes; then
			log "the app manager did not take the launcher as Home: LG Home put back"
			give_back
			return 1
		fi
	fi
	# the Home key helper must not open the launcher over itself
	if [ -f "$HOOK/watch.sh" ]; then
		pkill -9 -f '[l]auncher-homehook/watch.sh' 2>/dev/null
		pkill -9 -f '[l]auncher-homehook/homekey.py' 2>/dev/null
		rm -f /tmp/launcher-homehook-watch.pid /tmp/launcher-homekey.pid
	fi
	was=$(front)
	stop_lg_home
	luna-send -n 1 "$SAM/closeByAppId" "{\"id\":\"$APP_ID\"}" >/dev/null 2>&1
	case "$was" in
		"$HOME_ID"|"$APP_ID"|'')
			# Home again: the launcher in front within 15 s, with a second launch after 6 s (just after the old Home stopped
			# the app manager may take the first one without starting anything, as lg-luma found)
			luna-send -n 1 -w 10000 "$SAM/launch" "{\"id\":\"$HOME_ID\"}" >/dev/null 2>&1
			i=0
			while [ "$i" -lt 15 ]; do
				sleep 1; i=$((i + 1))
				[ "$(front)" = "$HOME_ID" ] && is_ours && break
				[ "$i" = 6 ] && luna-send -n 1 -w 10000 "$SAM/launch" "{\"id\":\"$HOME_ID\"}" >/dev/null 2>&1
			done
			if [ "$(front)" != "$HOME_ID" ]; then
				log "the launcher did not come up as Home: LG Home put back"
				give_back
				luna-send -n 1 -w 10000 "$SAM/launch" "{\"id\":\"$HOME_ID\"}" >/dev/null 2>&1
				return 1
			fi
			;;
	esac
	log "the launcher is the TV's Home"
}

case "$1" in
	enable)
		mkdir -p "$DIR"
		touch "$FLAG"
		cp -f "$0" "$DIR/takeover.sh" 2>/dev/null
		printf '#!/bin/sh\n# Puts the launcher in LG Home'"'"'s place at every full start. Remove with: sh %s disable\nsh %s apply >/dev/null 2>&1 &\n' "$DIR/takeover.sh" "$DIR/takeover.sh" > "$LINK"
		chmod +x "$LINK" "$DIR/takeover.sh"
		# the helper is not needed at start-up any more
		[ -L "$HOOK_LINK" ] && mv -f "$HOOK_LINK" "$DIR/48-launcher-homehook.off"
		# with the launcher as Home, LG's "start with Home" is what makes waking up open it
		luna-send -n 1 luna://com.webos.settingsservice/setSystemSettings '{"category":"general","settings":{"homeAutoLaunch":"on"}}' >/dev/null 2>&1
		take || { rm -f "$FLAG" "$LINK"; exit 1; }
		;;
	apply)
		[ -f "$FLAG" ] || exit 0
		# at a full start: wait until the app manager answers
		i=0; until luna-send -n 1 "$SAM/getAppInfo" "{\"id\":\"$HOME_ID\"}" 2>/dev/null | grep -q returnValue || [ "$i" -ge 30 ]; do sleep 1; i=$((i + 1)); done
		take
		;;
	disable)
		rm -f "$FLAG" "$LINK"
		give_back
		[ -e "$DIR/48-launcher-homehook.off" ] && mv -f "$DIR/48-launcher-homehook.off" "$HOOK_LINK"
		[ -x "$HOOK/apply.sh" ] && sh "$HOOK/apply.sh" >/dev/null 2>&1
		luna-send -n 1 -w 10000 "$SAM/launch" "{\"id\":\"$HOME_ID\"}" >/dev/null 2>&1
		log "LG Home is back in its place"
		;;
	refresh)
		# at each start of the launcher, while it is on: the copy the boot link runs brought up to the installed launcher's (an
		# update reaches the next full start), and the links of the stage made again (a file the update added)
		[ -f "$FLAG" ] || { echo "off"; exit 0; }
		here=$(cd "$(dirname "$0")" && pwd)
		if [ "$here" != "$DIR" ] && ! cmp -s "$0" "$DIR/takeover.sh"; then
			cp -f "$0" "$DIR/takeover.sh" && chmod +x "$DIR/takeover.sh" && log "updated its copy from the installed launcher"
		fi
		stage >/dev/null 2>&1
		echo "ok"
		;;
	status)
		echo "enabled: $([ -f "$FLAG" ] && echo yes || echo no)"
		echo "mounted: $(mounted && echo yes || echo no)"
		echo "Home is the launcher: $(is_ours && echo yes || echo no)"
		echo "boot link: $([ -e "$LINK" ] && echo yes || echo no)"
		;;
	*) echo "usage: sh $0 enable | disable | apply | refresh | status"; exit 1 ;;
esac
