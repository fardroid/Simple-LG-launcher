#!/bin/sh
# Puts the launcher in LG Home's place, the way Glasshouse Home and Luma do it (lib/home-takeover.js in
# rorygallagher2024/glasshouse, tv/home.sh in ecklf/lg-luma): a folder with the launcher's files and an appinfo.json
# with LG Home's id (com.webos.app.home, type web) is bind-mounted over LG Home's app folder, and the TV's app manager
# is made to read that one app again (blockSystemApp, then releaseBlockSystemApp). From then on everything that means
# "Home" opens the launcher: the Home key, Back out of the last app, waking up. LG's files are not changed: unmounting
# (disable, or a full restart of the TV without the boot link) gives LG Home back.
#
#   sh takeover.sh enable    put the launcher in place now, and again at every full start (boot link)
#   sh takeover.sh disable   LG Home back, boot link removed, the Home key helper started again
#   sh takeover.sh apply     what the boot link runs (only when enabled)
#   sh takeover.sh status
#
# While it is on, the Home key helper (launcher-homehook) is stopped: the TV itself opens the launcher.
#
# LG Home's bus permissions (/usr/share/luna-service2/client-permissions.d/com.webos.app.home.app.json) do not include
# the Homebrew Channel's root service, which the launcher uses for the full app list, wallpapers, calendar links and its
# settings file. A copy with LG's groups plus that service is bind-mounted over that file and the bus rescans (a file in
# the developer folder, /var/luna-service2-dev, is ignored for a system app's id). This is a change to the TV's
# permissions: whatever runs as Home can then run commands as root, as the launcher already can under its own id.
# disable unmounts it; a full start without the boot link gives LG's file back as well.

DIR=/media/developer/launcher-takeover
STAGE=$DIR/app
FLAG=$DIR/enabled
LAUNCHER=/media/developer/apps/usr/palm/applications/tld.my.simplelauncher
HOME_ID=com.webos.app.home
APPDIR=/usr/palm/applications/$HOME_ID
LINK=/var/lib/webosbrew/init.d/46-launcher-takeover
HOOK=/media/developer/launcher-homehook
HOOK_LINK=/var/lib/webosbrew/init.d/48-launcher-homehook
LOG=/tmp/launcher-homehook.log
SAM=luna://com.webos.applicationManager
PERMS=/usr/share/luna-service2/client-permissions.d/$HOME_ID.app.json
PERMS_COPY=$DIR/home-permissions.json

log () { echo "$(date '+%H:%M:%S') takeover: $*" | tee -a "$LOG"; }

mounted () { grep -q " $APPDIR " /proc/mounts; }

# The type the app manager has for Home now: web while the launcher is in its place.
home_type () { luna-send -n 1 "$SAM/getAppInfo" "{\"id\":\"$HOME_ID\"}" 2>/dev/null | sed -n 's/.*"type"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p'; }

# The app manager reads one app's appinfo.json again: block it and release it at once.
reread () {
	luna-send -n 1 "$SAM/blockSystemApp" "{\"id\":\"$HOME_ID\"}" >/dev/null 2>&1
	i=0
	until luna-send -n 1 "$SAM/releaseBlockSystemApp" "{\"id\":\"$HOME_ID\"}" 2>/dev/null | grep -q '"returnValue":true'; do
		i=$((i + 1)); [ "$i" -ge 3 ] && break; sleep 1
	done
}

wait_type () {    # web | other: wait up to 8 s for the app manager to report it
	i=0
	while [ "$i" -lt 8 ]; do
		t=$(home_type)
		if [ "$1" = web ]; then [ "$t" = web ] && return 0; else [ -n "$t" ] && [ "$t" != web ] && return 0; fi
		sleep 1; i=$((i + 1))
	done
	return 1
}

# LG's own Home: on webOS 10 the Flutter runner with its id.
stop_lg_home () {
	for p in $(ls /proc | grep -E '^[0-9]+$'); do
		cmd=$(tr '\0' ' ' < "/proc/$p/cmdline" 2>/dev/null)
		case "$cmd" in
			*flutter-client*" -i $HOME_ID"*|"/usr/bin/$HOME_ID"*) kill "$p" 2>/dev/null && log "stopped LG Home (pid $p)" ;;
		esac
	done
}

stage () {
	[ -f "$LAUNCHER/appinfo.json" ] || { log "the launcher is not installed"; return 1; }
	mkdir -p "$STAGE"
	# everything of the launcher's, linked, so an update of the launcher reaches the Home copy at its next start (and the
	# start-up picture the launcher writes, splash-wallpaper.jpg, is the one the TV shows when it starts Home)
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
  "type": "web",
  "main": "index.html",
  "title": "Home",
  "icon": "icon.png",
  "largeIcon": "largeIcon.png",
  "iconColor": "#050505",
  "bgColor": "#000000",
  "resolution": "1920x1080",
  "visible": false,
  "splashBackground": "splash-wallpaper.jpg",
  "disableBackHistoryAPI": true,
  "appCategory": ["home"]
}
EOF
}

granted () { grep -q " $PERMS " /proc/mounts; }

grant () {
	printf '%s\n' '{"com.webos.app.home-*":["applications","applications.launcher","applications.operation","applications.internal","hlp.internal","launcher.status","launcher.control","sdx.control","settings","system","database.internal","database","private","usercontextmanager.internal","public","org.webosbrew.hbchannel.service.group"]}' > "$PERMS_COPY"
	granted || mount --bind "$PERMS_COPY" "$PERMS"
	ls-control scan-services >/dev/null 2>&1
}

ungrant () {
	granted || return 0
	umount "$PERMS" 2>/dev/null || umount -l "$PERMS"
	ls-control scan-services >/dev/null 2>&1
}

take () {
	stage || return 1
	granted || grant
	if ! mounted; then
		mount --bind "$STAGE" "$APPDIR" || { log "the mount did not take"; return 1; }
	fi
	if [ "$(home_type)" != web ]; then
		reread
		if ! wait_type web; then
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
	front=$(luna-send -n 1 "$SAM/getForegroundAppInfo" '{}' 2>/dev/null)
	stop_lg_home
	luna-send -n 1 "$SAM/closeByAppId" '{"id":"tld.my.simplelauncher"}' >/dev/null 2>&1
	case "$front" in
		*'"appId":"'"$HOME_ID"'"'*|*'"appId":"tld.my.simplelauncher"'*|*'"appId":""'*) luna-send -n 1 "$SAM/launch" "{\"id\":\"$HOME_ID\"}" >/dev/null 2>&1 ;;
	esac
	log "the launcher is the TV's Home"
}

give_back () {
	if mounted; then umount "$APPDIR" 2>/dev/null || umount -l "$APPDIR"; fi
	reread
	wait_type other || log "the app manager has not reported LG Home back yet"
	luna-send -n 1 luna://com.palm.webappmanager/killApp "{\"appId\":\"$HOME_ID\"}" >/dev/null 2>&1
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
		take
		;;
	apply)
		[ -f "$FLAG" ] || exit 0
		# at a full start: wait until the app manager answers
		i=0; until [ -n "$(home_type)" ] || [ "$i" -ge 30 ]; do sleep 1; i=$((i + 1)); done
		take
		;;
	disable)
		rm -f "$FLAG" "$LINK"
		give_back
		ungrant
		[ -e "$DIR/48-launcher-homehook.off" ] && mv -f "$DIR/48-launcher-homehook.off" "$HOOK_LINK"
		[ -x "$HOOK/apply.sh" ] && sh "$HOOK/apply.sh" >/dev/null 2>&1
		luna-send -n 1 "$SAM/launch" '{"id":"tld.my.simplelauncher"}' >/dev/null 2>&1
		log "LG Home is back in its place; the Home key helper runs again"
		;;
	status)
		echo "enabled: $([ -f "$FLAG" ] && echo yes || echo no)"
		echo "mounted: $(mounted && echo yes || echo no)"
		echo "Home type: $(home_type)"
		echo "boot link: $([ -e "$LINK" ] && echo yes || echo no)"
		echo "Homebrew access for Home: $(granted && echo yes || echo no)"
		;;
	*) echo "usage: sh $0 enable | disable | apply | status"; exit 1 ;;
esac
