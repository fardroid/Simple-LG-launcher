#!/bin/sh
# Makes the launcher the TV's Home app the way webOS itself chooses one: the launcher's appinfo.json lists
# "appCategory": ["home"], and the app manager's setDefaultApp names it for the "home" category. The Home key and
# Back out of the last app then open the launcher under its own id. Nothing is mounted, no LG file or permission is
# changed, and it lasts across a full restart. The method and its checks follow docs/native-home.md in
# ammar027/lumen-webos (tried there on webOS 25).
#
# Waking from standby does not ask for the Home app: the TV's start-up (bootd) opens LG Home by name when LG's "start with
# Home" (homeAutoLaunch) is on, and otherwise the last input (the general settings lastInputApp / physicalLastInputApp). So
# "start with Home" is switched off and the launcher is named as the last input: the TV then wakes straight into the launcher.
# (The idea is from devic021/openhome-webos.) Switching to an input (HDMI…) names that input again; the launcher names itself
# once more each time it comes to the front or goes to the back ("point"). The three settings as they were are kept in
# $ORIG and put back by disable.
#
#   sh defaulthome.sh enable    the launcher becomes Home (refuses while takeover.sh or the Home key helper is on)
#   sh defaulthome.sh disable   LG Home becomes Home again, and the three settings are put back
#   sh defaulthome.sh status
#   sh defaulthome.sh check     what the boot link runs: LG Home back if the launcher is no longer installed
#   sh defaulthome.sh refresh   what the launcher runs once each time it starts: while this is on, the copy of this script is
#                               brought up to the installed launcher's (after an update) and the settings are set again
#   sh defaulthome.sh point     what the launcher runs when it comes to the front or goes to the back: the launcher as last input
#
# Before uninstalling the launcher, run disable: the TV's Home must never name an app that is not there.
# The boot link is a safety net for when that was forgotten.

DIR=/media/developer/launcher-defaulthome
FLAG=$DIR/enabled
BEFORE=$DIR/before.json
ORIG=$DIR/settings.orig
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
KEYS='"homeAutoLaunch","lastInputApp","physicalLastInputApp"'

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

# The value of one general setting, out of a getSystemSettings reply ($2, or the TV's now).
setting () {
	out=${2:-$(luna-send -n 1 "$SETTINGS/getSystemSettings" "{\"category\":\"general\",\"keys\":[$KEYS]}" 2>/dev/null)}
	echo "$out" | sed -n "s/.*\"$1\"[[:space:]]*:[[:space:]]*\"\([^\"]*\)\".*/\1/p"
}

set_settings () {   # homeAutoLaunch lastInputApp physicalLastInputApp (an empty one is left as it is)
	body=''
	[ -n "$1" ] && body="\"homeAutoLaunch\":\"$1\""
	[ -n "$2" ] && body="$body${body:+,}\"lastInputApp\":\"$2\""
	[ -n "$3" ] && body="$body${body:+,}\"physicalLastInputApp\":\"$3\""
	[ -n "$body" ] || return 0
	luna-send -n 1 "$SETTINGS/setSystemSettings" "{\"category\":\"general\",\"settings\":{$body}}" 2>/dev/null | grep -q '"returnValue"[[:space:]]*:[[:space:]]*true'
}

# The three settings as they are, kept once (the first time this way is turned on), so disable can put them back.
keep_settings () {
	[ -s "$ORIG" ] && return 0
	luna-send -n 1 "$SETTINGS/getSystemSettings" "{\"category\":\"general\",\"keys\":[$KEYS]}" 2>/dev/null > "$ORIG.new"
	if grep -q '"returnValue"[[:space:]]*:[[:space:]]*true' "$ORIG.new"; then mv -f "$ORIG.new" "$ORIG"; else rm -f "$ORIG.new"; return 1; fi
}

# Waking up opens the launcher: no "start with Home", the launcher as last input. Only what differs is written.
wake_into_launcher () {
	now=$(luna-send -n 1 "$SETTINGS/getSystemSettings" "{\"category\":\"general\",\"keys\":[$KEYS]}" 2>/dev/null)
	auto=''; last=''; phys=''
	[ "$(setting homeAutoLaunch "$now")" = off ] || auto=off
	[ "$(setting lastInputApp "$now")" = "$APP_ID" ] || last=$APP_ID
	[ "$(setting physicalLastInputApp "$now")" = "$APP_ID" ] || phys=$APP_ID
	set_settings "$auto" "$last" "$phys"
}

put_settings_back () {
	[ -s "$ORIG" ] || return 0
	o=$(cat "$ORIG")
	if set_settings "$(setting homeAutoLaunch "$o")" "$(setting lastInputApp "$o")" "$(setting physicalLastInputApp "$o")"; then
		rm -f "$ORIG"; log "start with Home and the last input put back"
	else
		log "could not put back start with Home and the last input (kept in $ORIG)"
	fi
}

# The wake-up guard of 1.9.3 (tools/wakeguard.py), not needed any more: stopped and its copy removed.
old_guard_off () {
	pid=$(cat /tmp/launcher-wakeguard.pid 2>/dev/null)
	[ -n "$pid" ] && kill "$pid" 2>/dev/null
	pkill -f '[l]auncher-defaulthome/wakeguard.py' 2>/dev/null
	rm -f /tmp/launcher-wakeguard.pid "$DIR/wakeguard.py"
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
	put_settings_back
	rm -f "$FLAG" "$LINK"
	old_guard_off
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
		here=$(cd "$(dirname "$0")" && pwd)
		[ "$here" = "$DIR" ] || cp -f "$0" "$DIR/defaulthome.sh" 2>/dev/null
		chmod +x "$DIR/defaulthome.sh" 2>/dev/null
		old_guard_off
		printf '#!/bin/sh\n# Gives LG Home back at start-up if the launcher was uninstalled. Remove with: sh %s disable\nsh %s check >/dev/null 2>&1 &\n' "$DIR/defaulthome.sh" "$DIR/defaulthome.sh" > "$LINK"
		chmod +x "$LINK"
		keep_settings || log "could not read start with Home and the last input: they will not be put back by disable"
		wake_into_launcher || log "the TV did not take the launcher as last input: waking up may show LG Home"
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
		else
			old_guard_off
			wake_into_launcher
		fi
		;;
	refresh)
		[ -f "$FLAG" ] || { echo "off"; exit 0; }
		here=$(cd "$(dirname "$0")" && pwd)
		if [ "$here" != "$DIR" ] && ! cmp -s "$0" "$DIR/defaulthome.sh"; then
			cp -f "$0" "$DIR/defaulthome.sh" && chmod +x "$DIR/defaulthome.sh" && log "updated its copy from the installed launcher"
		fi
		old_guard_off
		keep_settings
		wake_into_launcher && echo "ok"
		;;
	point)
		[ -f "$FLAG" ] || exit 0
		wake_into_launcher
		;;
	status)
		echo "enabled: $([ -f "$FLAG" ] && echo yes || echo no)"
		echo "Home app: $(current)"
		echo "launcher has home category: $(has_home_category "$APP_ID" && echo yes || echo no)"
		echo "LG Home has home category: $(has_home_category "$LG_HOME" && echo yes || echo no)"
		echo "start with Home: $(setting homeAutoLaunch); last input: $(setting lastInputApp) / $(setting physicalLastInputApp)"
		echo "settings kept for disable: $([ -s "$ORIG" ] && echo yes || echo no)"
		echo "boot link: $([ -e "$LINK" ] && echo yes || echo no)"
		echo "takeover.sh on: $([ -f "$TAKEOVER_FLAG" ] && echo yes || echo no)"
		echo "helper on: $(helper_on && echo yes || echo no)"
		;;
	*) echo "usage: sh $0 enable | disable | status | check | refresh | point"; exit 1 ;;
esac
