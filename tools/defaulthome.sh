#!/bin/sh
# Makes the launcher the TV's Home app the way webOS itself chooses one: the launcher's appinfo.json lists
# "appCategory": ["home"], and the app manager's setDefaultApp names it for the "home" category. The Home key,
# Back out of the last app and waking up then open the launcher under its own id. Nothing is mounted, no LG file
# or permission is changed, and it lasts across a full restart (no LG Home for the first seconds after a start).
# The method and its checks follow docs/native-home.md in ammar027/lumen-webos (tried there on webOS 25).
#
#   sh defaulthome.sh enable    the launcher becomes Home (refuses while takeover.sh or the Home key helper is on)
#   sh defaulthome.sh disable   LG Home becomes Home again
#   sh defaulthome.sh status
#   sh defaulthome.sh check     what the boot link runs: LG Home back if the launcher is no longer installed
#
# Before uninstalling the launcher, run disable: the TV's Home must never name an app that is not there.
# The boot link is a safety net for when that was forgotten.

DIR=/media/developer/launcher-defaulthome
FLAG=$DIR/enabled
BEFORE=$DIR/before.json
APP_ID=tld.my.simplelauncher
LG_HOME=com.webos.app.home
LAUNCHER=/media/developer/apps/usr/palm/applications/$APP_ID
LINK=/var/lib/webosbrew/init.d/45-launcher-defaulthome
HOOK=/media/developer/launcher-homehook
HOOK_LINK=/var/lib/webosbrew/init.d/48-launcher-homehook
TAKEOVER_FLAG=/media/developer/launcher-takeover/enabled
LOG=/tmp/launcher-homehook.log
AM=luna://com.webos.service.applicationmanager
SETTINGS=luna://com.webos.settingsservice

log () { echo "$(date '+%H:%M:%S') defaulthome: $*" | tee -a "$LOG"; }

defaults () { luna-send -n 1 "$SETTINGS/getSystemSettings" '{"category":"general","keys":["defaultApps"]}' 2>/dev/null; }

# Can the TV's default apps be read at all? (Out of the box the list has no "home" entry: LG Home is then Home.)
readable () { defaults | grep -q '"returnValue"[[:space:]]*:[[:space:]]*true'; }

# The app that the "home" category names now; LG Home when the list names none.
current () {
	out=$(defaults)
	echo "$out" | grep -q '"returnValue"[[:space:]]*:[[:space:]]*true' || return 0
	app=$(echo "$out" | sed -n 's/.*"home"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p')
	echo "${app:-$LG_HOME}"
}

# Does the app manager list "home" among this app's categories?
has_home_category () {
	luna-send -n 1 "$AM/getAppInfo" "{\"id\":\"$1\",\"properties\":[\"id\",\"appCategory\"]}" 2>/dev/null | grep -q '"appCategory"[^]]*"home"'
}

set_home () {
	luna-send -n 1 "$AM/setDefaultApp" "{\"category\":\"home\",\"appId\":\"$1\"}" 2>/dev/null | grep -q '"returnValue"[[:space:]]*:[[:space:]]*true'
}

# Opens Home the way the Home key does and waits up to 8 s for $1 to be the app in front.
open_home () {
	luna-send -n 1 "$AM/launchDefaultApp" '{"category":"home"}' >/dev/null 2>&1
	i=0
	while [ "$i" -lt 8 ]; do
		luna-send -n 1 "$AM/getForegroundAppInfo" '{}' 2>/dev/null | grep -q "\"appId\"[[:space:]]*:[[:space:]]*\"$1\"" && return 0
		sleep 1; i=$((i + 1))
	done
	return 1
}

helper_on () {
	[ -L "$HOOK_LINK" ] && return 0
	pid=$(cat /tmp/launcher-homehook-watch.pid 2>/dev/null)
	[ -n "$pid" ] && kill -0 "$pid" 2>/dev/null
}

give_back () {
	set_home "$LG_HOME" || log "setDefaultApp for LG Home was not accepted"
	rm -f "$FLAG" "$LINK"
	[ "$(current)" = "$LG_HOME" ] && log "LG Home is the TV's Home again" || log "the TV's Home is now: $(current) (expected $LG_HOME)"
}

case "$1" in
	enable)
		[ -f "$LAUNCHER/appinfo.json" ] || { echo "refused: the launcher is not installed"; exit 1; }
		[ -f "$TAKEOVER_FLAG" ] && { echo "refused: takeover.sh is on; run 'sh takeover.sh disable' first"; exit 1; }
		helper_on && { echo "refused: the Home key helper is on; switch off Settings > Home screen > Background helper first"; exit 1; }
		has_home_category "$APP_ID" || { echo "refused: the installed launcher has no \"home\" category (install 1.9.2 or newer)"; exit 1; }
		has_home_category "$LG_HOME" || { echo "refused: LG Home has no \"home\" category on this TV, so this method is not for it"; exit 1; }
		readable || { echo "refused: could not read the TV's default apps"; exit 1; }
		was=$(current)
		mkdir -p "$DIR"
		defaults > "$BEFORE"
		set_home "$APP_ID" || { log "setDefaultApp was not accepted; nothing changed"; exit 1; }
		if [ "$(current)" != "$APP_ID" ]; then
			log "the TV did not keep the launcher as Home"; give_back; exit 1
		fi
		touch "$FLAG"
		cp -f "$0" "$DIR/defaulthome.sh" 2>/dev/null && chmod +x "$DIR/defaulthome.sh"
		printf '#!/bin/sh\n# Gives LG Home back at start-up if the launcher was uninstalled. Remove with: sh %s disable\nsh %s check >/dev/null 2>&1 &\n' "$DIR/defaulthome.sh" "$DIR/defaulthome.sh" > "$LINK"
		chmod +x "$LINK"
		# waking up opens Home only with LG's "start with Home" on
		luna-send -n 1 "$SETTINGS/setSystemSettings" '{"category":"general","settings":{"homeAutoLaunch":"on"}}' >/dev/null 2>&1
		if open_home "$APP_ID"; then
			log "the launcher is the TV's Home (was $was)"
		else
			log "Home did not open the launcher: LG Home put back"; give_back; exit 1
		fi
		;;
	disable)
		give_back
		open_home "$LG_HOME" >/dev/null || true
		;;
	check)
		[ -f "$FLAG" ] || exit 0
		i=0; until readable || [ "$i" -ge 60 ]; do sleep 1; i=$((i + 1)); done
		if [ ! -f "$LAUNCHER/appinfo.json" ] || ! has_home_category "$APP_ID"; then
			log "the launcher is gone or has no home category: LG Home put back"; give_back
		fi
		;;
	status)
		echo "enabled: $([ -f "$FLAG" ] && echo yes || echo no)"
		echo "Home app: $(current)"
		echo "launcher has home category: $(has_home_category "$APP_ID" && echo yes || echo no)"
		echo "LG Home has home category: $(has_home_category "$LG_HOME" && echo yes || echo no)"
		echo "boot link: $([ -e "$LINK" ] && echo yes || echo no)"
		echo "takeover.sh on: $([ -f "$TAKEOVER_FLAG" ] && echo yes || echo no)"
		echo "helper on: $(helper_on && echo yes || echo no)"
		;;
	*) echo "usage: sh $0 enable | disable | status | check"; exit 1 ;;
esac
