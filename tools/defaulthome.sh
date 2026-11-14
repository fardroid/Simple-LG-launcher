#!/bin/sh
# Makes the launcher the TV's Home app the way webOS itself chooses one: the launcher's appinfo.json lists
# "appCategory": ["home"], and the app manager's setDefaultApp names it for the "home" category. The Home key and
# Back out of the last app then open the launcher under its own id. Nothing is mounted, no LG file or permission is
# changed, and it lasts across a full restart. The method and its checks follow docs/native-home.md in
# ammar027/lumen-webos (tried there on webOS 25).
#
# Switching the TV on is left to the TV: it opens its last input (the general settings lastInputApp / physicalLastInputApp),
# what was on the screen before. Only LG's "start with Home" (homeAutoLaunch) is switched off, which would open LG Home by name
# over it. Its value as it was is kept in $ORIG and put back by disable.
#
# The launcher is never named as the last input. Up to 2.2.0 it was (so that waking up opened it), and a full start with the
# launcher's id as physicalLastInputApp left the HDMI inputs and the tuner black until the app manager was restarted (issue 16,
# seen on webOS 10.3.2: the input reports a signal, the picture stays black; switching inputs or standby does not help). That
# is undone here: the launcher's id in either setting is replaced by the TV's own value from before, or LG Home's.
# Nor is the launcher kept loaded through standby any more (keepAliveApps, which needed the app manager restarted at every
# full start, closing what was open): after standby the Home key starts it from scratch, in 2 to 3 s. The copies of 2.2.0,
# mounted in RAM, are taken off (the app manager keeps the list it read until the next full start).
#
# When the last input is Home (com.webos.app.home: LG's own value when LG Home was on the screen), switching the TV on opens LG
# Home by its id (the boot manager does it, whatever the Home app is). A small watcher ("watch", started by the boot link and by
# the launcher, the TV's power state through luna-send) opens the launcher instead, just after the TV comes on or a full start,
# when LG Home is what came up. An input or an app that came up is left alone.
#
#   sh defaulthome.sh enable    the launcher becomes Home (refuses while takeover.sh or the Home key helper is on)
#   sh defaulthome.sh disable   LG Home becomes Home again, and the three settings are put back
#   sh defaulthome.sh status
#   sh defaulthome.sh check     what the boot link runs: LG Home back if the launcher is no longer installed
#   sh defaulthome.sh refresh   what the launcher runs once each time it starts: while this is on, the copy of this script is
#                               brought up to the installed launcher's (after an update) and the settings are set again
#   sh defaulthome.sh point     what the launcher of 2.2.0 ran on coming to the front: the same settings check (nothing else)
#   sh defaulthome.sh watch     the watcher above (the boot link, enable and refresh start it in the background)
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
LAYERS=/etc/configd/layers
COPIES=$DIR/configd
CONFIG=luna://com.webos.service.config
POWER=luna://com.webos.service.tvpower/power/getPowerState
WATCH_PID=/tmp/launcher-wake-watch.pid

log () { echo "$(date '+%H:%M:%S') defaulthome: $*" | tee -a "$LOG"; }
# A script started through the Homebrew Channel's service inherits its open sockets (file descriptors 3 and up), and one that
# stays running keeps them open, which can hold that service up (Hearth 2.2.0, its pull request 54). fdclean closes every
# descriptor above 2 in the shell that runs it: called in the ( ... ) that starts a background script, just before it.
# A shell that cannot name a two-digit descriptor (dash) closes 3 to 9 only.
fdclean () {
	_w=0; ( eval "exec 10>&-" ) 2>/dev/null && _w=1
	for _f in /proc/self/fd/*; do
		_n=${_f##*/}
		case "$_n" in ''|*[!0-9]*|0|1|2) continue ;; esac
		[ "${#_n}" -gt 1 ] && [ "$_w" = 0 ] && continue
		eval "exec $_n>&-" 2>/dev/null
	done
}
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

# What the launcher's id in a last-input setting ($1) is replaced with: the TV's own value from before (kept in $ORIG), unless
# that was the launcher too, and otherwise LG Home's (the TV's own value when "start with Home" was on).
own_value () {
	v=$([ -s "$ORIG" ] && setting "$1" "$(cat "$ORIG")")
	[ -n "$v" ] && [ "$v" != "$APP_ID" ] && echo "$v" || echo "$LG_HOME"
}

# Switching on shows what was on before: no "start with Home", and the launcher's id out of the last input (see the top).
# Only what differs is written.
settings_check () {
	now=$(luna-send -n 1 "$SETTINGS/getSystemSettings" "{\"category\":\"general\",\"keys\":[$KEYS]}" 2>/dev/null)
	echo "$now" | grep -q '"returnValue"[[:space:]]*:[[:space:]]*true' || return 1
	auto=''; last=''; phys=''
	[ "$(setting homeAutoLaunch "$now")" = off ] || auto=off
	[ "$(setting lastInputApp "$now")" = "$APP_ID" ] && last=$(own_value lastInputApp)
	[ "$(setting physicalLastInputApp "$now")" = "$APP_ID" ] && phys=$(own_value physicalLastInputApp)
	set_settings "$auto" "$last" "$phys" || return 1
	[ -n "$last$phys" ] && log "the launcher is no longer the last input (now: ${last:-unchanged} / ${phys:-unchanged})"
	return 0
}

# disable: "start with Home" as it was; the last input stays the TV's own (the launcher's id taken out, should it be there).
put_settings_back () {
	auto=''
	[ -s "$ORIG" ] && auto=$(setting homeAutoLaunch "$(cat "$ORIG")")
	now=$(luna-send -n 1 "$SETTINGS/getSystemSettings" "{\"category\":\"general\",\"keys\":[$KEYS]}" 2>/dev/null)
	last=''; phys=''
	[ "$(setting lastInputApp "$now")" = "$APP_ID" ] && last=$(own_value lastInputApp)
	[ "$(setting physicalLastInputApp "$now")" = "$APP_ID" ] && phys=$(own_value physicalLastInputApp)
	if set_settings "$auto" "$last" "$phys"; then
		rm -f "$ORIG"; log "start with Home put back"
	else
		log "could not put back start with Home (kept in $ORIG)"
	fi
}

# The copies of 2.2.0 that added the launcher to keepAliveApps (mounted over files in $LAYERS, in RAM), taken off. The app
# manager is not restarted for it: it keeps the list it read until the next full start, which reads LG's files again.
keepalive_off () {
	for f in $(grep -oE " $LAYERS/[^ ]+" /proc/mounts); do umount "$f" 2>/dev/null; done
	if [ -d "$COPIES" ]; then
		luna-send -n 1 "$CONFIG/reconfigure" '{}' >/dev/null 2>&1
		rm -rf "$COPIES"
		log "the launcher is no longer kept loaded through standby (keepAliveApps of 2.2.0 taken off)"
	fi
	pid=$(cat /tmp/launcher-keep-front.pid 2>/dev/null)
	[ -n "$pid" ] && kill "$pid" 2>/dev/null
	rm -f /tmp/launcher-keep-front.pid
	rmdir /tmp/launcher-keepalive.lock 2>/dev/null
	return 0
}

front_app () { luna-send -n 1 -w 5000 "$AM/getForegroundAppInfo" '{}' 2>/dev/null | sed -n 's/.*"appId"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p'; }

# For up to 12 s: LG Home in front is replaced by the launcher (opened as Home, the way the Home key does); anything else that
# comes up (an input, an app, the launcher) ends it. Just after waking nothing is in front for a moment.
home_to_launcher () {
	i=0; asked=0
	while [ "$i" -lt 12 ]; do
		[ -f "$FLAG" ] || return 0
		case "$(front_app)" in
			'') ;;
			"$LG_HOME")
				[ "$asked" -ge 2 ] && return 0
				luna-send -n 1 -w 10000 "$AM/launchDefaultApp" '{"category":"home"}' >/dev/null 2>&1
				asked=$((asked + 1)); log "$1 with LG Home in front: the launcher opened instead" ;;
			*) return 0 ;;
		esac
		sleep 1; i=$((i + 1))
	done
}

watching () { pid=$(cat "$WATCH_PID" 2>/dev/null); [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; }

# The watcher: follows the TV's power state; each time the TV comes back on, LG Home is replaced (home_to_launcher).
watch_wake () {
	echo $$ > "$WATCH_PID"
	while [ -f "$FLAG" ]; do
		awake=yes
		luna-send -i "$POWER" '{"subscribe":true}' 2>/dev/null | while read -r line; do
			[ -f "$FLAG" ] || break
			state=$(echo "$line" | sed -n 's/.*"state"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p')
			case "$state" in ''|"Screen Off"|*"Screen Saver"*) continue ;; esac     # the picture off with the sound on: no standby
			if [ "$state" != Active ]; then awake=no
			elif [ "$awake" = no ]; then awake=yes; home_to_launcher "TV switched on" & fi
		done
		sleep 2
	done
	rm -f "$WATCH_PID"
}

watch_start () {
	watching && return 0
	( fdclean; setsid nohup sh "$DIR/defaulthome.sh" watch </dev/null >/dev/null 2>&1 & )
}

watch_stop () {
	pid=$(cat "$WATCH_PID" 2>/dev/null)
	if [ -n "$pid" ]; then
		for c in $(cat /proc/[0-9]*/stat 2>/dev/null | awk -v p="$pid" '$4 == p { print $1 }'); do pkill -P "$c" 2>/dev/null; kill "$c" 2>/dev/null; done
		kill "$pid" 2>/dev/null
	fi
	rm -f "$WATCH_PID"
}

# The wake-up guard of 1.9.3 (tools/wakeguard.py), not needed any more: stopped and its copy removed.
old_guard_off () {
	pid=$(cat /tmp/launcher-wakeguard.pid 2>/dev/null)
	[ -n "$pid" ] && kill "$pid" 2>/dev/null
	pkill -f '[l]auncher-defaulthome/wakeguard.py' 2>/dev/null
	rm -f /tmp/launcher-wakeguard.pid "$DIR/wakeguard.py"
}

# Opens Home the way the Home key does and waits up to 20 s for $1 to be the app in front (a launcher that has to start from
# scratch on a busy TV takes a while).
open_home () {
	luna-send -n 1 -w 10000 "$AM/launchDefaultApp" '{"category":"home"}' >/dev/null 2>&1
	i=0
	while [ "$i" -lt 20 ]; do
		luna-send -n 1 -w 5000 "$AM/getForegroundAppInfo" '{}' 2>/dev/null | grep -q "\"appId\"[[:space:]]*:[[:space:]]*\"$1\"" && return 0
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
	watch_stop
	set_home "$LG_HOME" || log "setDefaultApp for LG Home was not accepted"
	put_settings_back
	rm -f "$FLAG" "$LINK"
	keepalive_off
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
		keep_settings || log "could not read start with Home: it will not be put back by disable"
		settings_check || log "could not switch off start with Home: switching the TV on may show LG Home"
		keepalive_off
		if ! open_home "$APP_ID"; then
			log "Home did not open the launcher: LG Home put back"; give_back; exit 1
		fi
		log "the launcher is the TV's Home (was $was)"
		watch_start
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
			settings_check
			rm -rf "$COPIES"
			watch_start
			home_to_launcher "the TV started"
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
		settings_check
		keepalive_off
		watch_start
		echo "ok"
		;;
	point)
		[ -f "$FLAG" ] || exit 0
		settings_check
		;;
	watch)
		[ -f "$FLAG" ] || exit 0
		watching && exit 0
		watch_wake
		;;
	status)
		echo "enabled: $([ -f "$FLAG" ] && echo yes || echo no)"
		echo "Home app: $(current)"
		echo "launcher has home category: $(has_home_category "$APP_ID" && echo yes || echo no)"
		echo "LG Home has home category: $(has_home_category "$LG_HOME" && echo yes || echo no)"
		echo "start with Home: $(setting homeAutoLaunch); last input: $(setting lastInputApp) / $(setting physicalLastInputApp)"
		echo "settings kept for disable: $([ -s "$ORIG" ] && echo yes || echo no)"
		echo "boot link: $([ -e "$LINK" ] && echo yes || echo no)"
		echo "wake watcher: $(watching && echo running || echo no)"
		echo "takeover.sh on: $([ -f "$TAKEOVER_FLAG" ] && echo yes || echo no)"
		echo "helper on: $(helper_on && echo yes || echo no)"
		;;
	*) echo "usage: sh $0 enable | disable | status | check | refresh | point | watch"; exit 1 ;;
esac
