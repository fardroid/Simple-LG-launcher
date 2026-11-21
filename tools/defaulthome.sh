#!/bin/sh
# Makes the launcher the TV's Home app the way webOS itself chooses one: the launcher's appinfo.json lists
# "appCategory": ["home"], and the app manager's setDefaultApp names it for the "home" category. The Home key and
# Back out of the last app then open the launcher under its own id. Nothing is mounted, no LG file or permission is
# changed, and it lasts across a full restart. The method and its checks follow docs/native-home.md in
# ammar027/lumen-webos (tried there on webOS 25).
#
# Switching the TV on is left to the TV, and so is LG's "start with Home" (homeAutoLaunch): this way does not change it. With
# it on, the TV opens LG Home by name, and the watcher below opens the launcher instead. With it off, the TV opens its last
# input (the general settings lastInputApp / physicalLastInputApp), and that is only ever a real input (HDMI, Live TV): after
# a full start (the plug, a power cut) it opens that input even when the launcher was on the screen (seen on webOS 10.3.2).
# Up to 2.2.2 this way switched "start with Home" off. Where it did, the value from before (kept in $ORIG) is put back once,
# at the first refresh or check of this version ($AUTO_LEFT marks it done); after that the setting is the person's alone.
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
#   sh defaulthome.sh disable   LG Home becomes Home again (and the last input the TV's own, see above)
#   sh defaulthome.sh status
#   sh defaulthome.sh check     what the boot link runs: LG Home back if the launcher is no longer installed
#   sh defaulthome.sh refresh   what the launcher runs once each time it starts: while this is on, the copy of this script is
#                               brought up to the installed launcher's (after an update) and the settings checked again
#   sh defaulthome.sh point     what the launcher of 2.2.0 ran on coming to the front: the same settings check (nothing else)
#   sh defaulthome.sh watch     the watcher above (the boot link, enable and refresh start it in the background)
#
# When the last app closes (also after a long press of Back), the TV's app manager opens its "idle app", which the TV's system
# profile names: LG Home, whatever the Home app is (lastAppHandlerPolicy "idleApp"; idleApp.appId in
# /var/webos-profile/platform/profile.json, seen on webOS 10.3.2). LG's own way to change a profile is a file in
# /var/webos-profile/override/ (*_profile.json), which the profile service reads at a full start (and writes into the profile)
# and at once when asked to rescan. This way puts one there that names the launcher as the idle app, so the launcher opens
# with no LG Home before it; disable removes it (and asks for a rescan: LG Home at once). Tried on webOS 10.3.2 (2026-11-21):
# closing the last app and a long press of Back opened the launcher, also after a full start. An idle app that is not installed
# leaves the screen black, so the check for an uninstall (guard.sh) removes it too.
#
# Before uninstalling the launcher, run disable: the TV's Home must never name an app that is not there.
# The boot link is a safety net for when that was forgotten.

DIR=/media/developer/launcher-defaulthome
FLAG=$DIR/enabled
BEFORE=$DIR/before.json
ORIG=$DIR/settings.orig
AUTO_LEFT=$DIR/start-with-home-left
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
GUARD=/media/developer/launcher-guard/guard.sh
RULES_MARK=/tmp/launcher-rules-loaded
STOCK_RULES=/etc/palm/bootd.json
RULES=/var/luna/preferences/bootd.json
RULES_SUM=$DIR/bootd.stock.md5
PROFILES=/var/webos-profile/override
IDLE=$PROFILES/$(echo "$APP_ID" | tr . _)_profile.json
SYSPROFILE=luna://com.webos.service.systemprofile

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

# "start with Home" as it was before an older version switched it off (see the top): the value in $ORIG, once, and only
# while the setting is still off. Empty when there is nothing to put back.
auto_back () {
	[ -f "$AUTO_LEFT" ] && return 0
	[ -s "$ORIG" ] || return 0
	was=$(setting homeAutoLaunch "$(cat "$ORIG")")
	[ -n "$was" ] && [ "$was" != off ] && [ "$(setting homeAutoLaunch "$1")" = off ] && echo "$was"
}

# The launcher's id out of the last input (see the top), and "start with Home" given back once. Only what differs is written.
settings_check () {
	now=$(luna-send -n 1 "$SETTINGS/getSystemSettings" "{\"category\":\"general\",\"keys\":[$KEYS]}" 2>/dev/null)
	echo "$now" | grep -q '"returnValue"[[:space:]]*:[[:space:]]*true' || return 1
	auto=$(auto_back "$now"); last=''; phys=''
	[ "$(setting lastInputApp "$now")" = "$APP_ID" ] && last=$(own_value lastInputApp)
	[ "$(setting physicalLastInputApp "$now")" = "$APP_ID" ] && phys=$(own_value physicalLastInputApp)
	set_settings "$auto" "$last" "$phys" || return 1
	[ -n "$auto" ] && log "start with Home put back as it was ($auto): this way no longer switches it off"
	touch "$AUTO_LEFT"
	[ -n "$last$phys" ] && log "the launcher is no longer the last input (now: ${last:-unchanged} / ${phys:-unchanged})"
	return 0
}

# disable: the last input stays the TV's own (the launcher's id taken out, should it be there); "start with Home" is put back
# only if an older version switched it off and this one has not given it back yet.
put_settings_back () {
	now=$(luna-send -n 1 "$SETTINGS/getSystemSettings" "{\"category\":\"general\",\"keys\":[$KEYS]}" 2>/dev/null)
	auto=$(auto_back "$now"); last=''; phys=''
	[ "$(setting lastInputApp "$now")" = "$APP_ID" ] && last=$(own_value lastInputApp)
	[ "$(setting physicalLastInputApp "$now")" = "$APP_ID" ] && phys=$(own_value physicalLastInputApp)
	if set_settings "$auto" "$last" "$phys"; then
		[ -n "$auto" ] && log "start with Home put back ($auto)"
		rm -f "$ORIG" "$AUTO_LEFT"
	else
		log "could not put the settings back (kept in $ORIG)"
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

# The boot link: runs "check" at every full start (the watcher, LG Home back if the launcher is gone). Written again when it is
# missing or not what it should be (on 2026-11-15 the TV lost power a minute after enable, and the link was left empty, so a
# full start ran nothing), and flushed to the storage at once.
link_text () {
	printf '#!/bin/sh\n# Gives LG Home back at start-up if the launcher was uninstalled. Remove with: sh %s disable\nsh %s check >/dev/null 2>&1 &\n' "$DIR/defaulthome.sh" "$DIR/defaulthome.sh"
}
link_check () {
	[ "$(cat "$LINK" 2>/dev/null)" = "$(link_text)" ] && [ -x "$LINK" ] && return 0
	link_text > "$LINK.new" && chmod +x "$LINK.new" && mv -f "$LINK.new" "$LINK" && sync && log "boot link written" && return 0
	rm -f "$LINK.new"; log "could not write the boot link $LINK"; return 1
}

helper_on () {
	[ -L "$HOOK_LINK" ] && return 0
	pid=$(cat /tmp/launcher-homehook-watch.pid 2>/dev/null)
	[ -n "$pid" ] && kill -0 "$pid" 2>/dev/null
}

# The start-up rules. At a full start and on waking the TV, its boot manager picks the first app by why the TV was switched
# on (/etc/palm/bootd.json, "firstAppByPowerOnReason"); it reads /var/luna/preferences/bootd.json instead when that file is
# there (LG's own override; Hearth's "When the TV turns on, open..." uses it). Our copy is LG's file with the power button,
# the TV's own button, the plug (lastPowerOn) and a cold start (micomPower) sent to the launcher; it is opened behind LG's
# start-up cover (an app that is gone is not replaced by the last input there: the TV opens LG's store with "Content
# unavailable", until guard.sh has taken the rules back). Seen on webOS 10.3.2 (2026-11-15): with it the TV
# opened the launcher at once after the plug and after standby, and LG Home never showed. The TV reads it at its next full
# start. Made only from LG's file as this script knows it (refused otherwise), and never over a file that is not ours.
rules_ours () { [ -f "$RULES" ] && grep -q "\"$APP_ID\"" "$RULES"; }
rules_make () {   # LG's file with our reasons -> stdout; fails when LG's file is not the one this script knows
	grep -q '^[[:space:]]*"remoteKey": "{LAST_APP}",$' "$STOCK_RULES" && grep -q '^[[:space:]]*"localKey": "{LAST_APP}",$' "$STOCK_RULES" \
		&& grep -q '^[[:space:]]*"upDownKey": "{LAST_APP}"$' "$STOCK_RULES" && [ "$(grep -c '"firstAppByPowerOnReason"' "$STOCK_RULES")" = 1 ] || return 1
	sed -e "s/^\([[:space:]]*\)\"remoteKey\": \"{LAST_APP}\",\$/\1\"remoteKey\": \"$APP_ID\",/" \
		-e "s/^\([[:space:]]*\)\"localKey\": \"{LAST_APP}\",\$/\1\"localKey\": \"$APP_ID\",/" \
		-e "s/^\([[:space:]]*\)\"upDownKey\": \"{LAST_APP}\"\$/\1\"upDownKey\": \"{LAST_APP}\",\\
\1\"micomPower\": \"$APP_ID\",\\
\1\"lastPowerOn\": \"$APP_ID\"/" "$STOCK_RULES"
}
rules_on () {
	if [ -f "$RULES" ] && ! rules_ours; then log "start-up rules: $RULES is not ours: left alone"; return 1; fi
	sum=$(md5sum "$STOCK_RULES" 2>/dev/null | cut -d ' ' -f 1)
	rules_ours && [ "$sum" = "$(cat "$RULES_SUM" 2>/dev/null)" ] && return 0
	if ! rules_make > "$RULES.new" || [ "$(grep -c "\"$APP_ID\"" "$RULES.new")" != 4 ]; then
		rm -f "$RULES.new"
		rules_ours && rm -f "$RULES" && sync
		log "start-up rules: LG's file is not the one this version knows: not changed (switching the TV on shows LG Home first)"
		return 1
	fi
	mv -f "$RULES.new" "$RULES" && echo "$sum" > "$RULES_SUM" && sync && log "start-up rules: the TV opens the launcher when switched on (from its next full start)"
}
rules_off () {
	rules_ours || return 0
	rm -f "$RULES" "$RULES_SUM"; sync
	log "start-up rules: LG's again (from the TV's next full start)"
}

# The idle app (see the top): the app the TV's profile names now, and our override of it.
idle_app () {
	luna-send -n 1 -w 5000 "$SYSPROFILE/getProfiles" '{"names":["idleApp"]}' 2>/dev/null | sed -n 's/.*"appId"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p'
}
idle_text () { printf '{"idleApp":{"_packageIncluded":true,"_configuration":"idleApp.json","support":true,"appId":"%s"}}\n' "$APP_ID"; }
idle_rescan () { luna-send -n 1 -w 10000 "$SYSPROFILE/debug/rescan" '{}' >/dev/null 2>&1; sleep 2; }
idle_on () {
	[ -d "$PROFILES" ] || { log "idle app: this TV has no $PROFILES: LG Home comes when the last app closes"; return 1; }
	now=$(idle_app)
	case "$now" in
		"$APP_ID") [ -f "$IDLE" ] && return 0 ;;
		"$LG_HOME") ;;
		*) log "idle app: the TV names ${now:-none} (not LG Home): left alone"; return 1 ;;
	esac
	if [ "$(cat "$IDLE" 2>/dev/null)" != "$(idle_text)" ]; then
		idle_text > "$IDLE.new" && mv -f "$IDLE.new" "$IDLE" && sync || { rm -f "$IDLE.new"; log "idle app: could not write $IDLE"; return 1; }
	fi
	[ "$now" = "$APP_ID" ] && return 0
	idle_rescan
	if [ "$(idle_app)" = "$APP_ID" ]; then log "idle app: the launcher (it opens when the last app closes, with no LG Home)"; return 0; fi
	log "idle app: the TV did not take the launcher: override removed"; idle_off; return 1
}
idle_off () {
	[ -f "$IDLE" ] || return 0
	rm -f "$IDLE"; sync
	idle_rescan
	[ "$(idle_app)" = "$APP_ID" ] && log "idle app: still the launcher after the rescan" || log "idle app: LG Home again"
}

give_back () {
	watch_stop
	set_home "$LG_HOME" || log "setDefaultApp for LG Home was not accepted"
	put_settings_back
	rm -f "$FLAG" "$LINK"
	rules_off
	idle_off
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
		link_check
		keep_settings || log "could not read the last input: disable will name LG Home there if needed"
		settings_check || log "could not check the last input settings"
		rules_on
		idle_on
		keepalive_off
		if ! open_home "$APP_ID"; then
			log "Home did not open the launcher: LG Home put back"; give_back; exit 1
		fi
		log "the launcher is the TV's Home (was $was)"
		sync
		watch_start
		;;
	disable)
		give_back
		# switched off from the launcher's Settings (homemode.sh): the launcher stays in front, it may have a question to ask
		[ -n "$KEEP_FRONT" ] || open_home "$LG_HOME" >/dev/null || true
		;;
	check)
		# at a full start: the boot manager has just read the rules there are now (guard.sh restarts the TV after an
		# uninstall only while it has ours in memory)
		rules_ours && touch "$RULES_MARK"
		[ -f "$FLAG" ] || exit 0
		i=0; until readable || [ "$i" -ge 60 ]; do sleep 1; i=$((i + 1)); done
		if [ ! -f "$LAUNCHER/appinfo.json" ] && [ -f "$GUARD" ]; then
			# an uninstall is guard.sh's to decide (it waits for the TV's app list; the apps folder can come late); it takes this
			# way back too (exit 3), or says the launcher is there after all
			log "the launcher is not there (yet): asking guard.sh"
			sh "$GUARD" check; [ $? = 3 ] && exit 0
		fi
		if [ ! -f "$LAUNCHER/appinfo.json" ]; then
			log "the launcher is not there: nothing done"
		elif ! has_home_category "$APP_ID"; then
			log "the launcher has no home category: LG Home put back"; give_back
		else
			old_guard_off
			settings_check
			# after a firmware update that changed LG's start-up rules: made again from the new file, or taken out
			rules_on
			idle_on
			# the watcher for an uninstall (guard.sh)
			[ -f "$GUARD" ] && sh "$GUARD" watch-start
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
		link_check
		keep_settings
		settings_check
		rules_on
		idle_on
		keepalive_off
		sync
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
		echo "idle app: $(idle_app) (override: $([ -f "$IDLE" ] && echo ours || echo none))"
		echo "start-up rules: $(rules_ours && echo ours || { [ -f "$RULES" ] && echo "someone else's" || echo "LG's"; })"
		echo "boot link: $([ "$(cat "$LINK" 2>/dev/null)" = "$(link_text)" ] && echo yes || { [ -e "$LINK" ] && echo "not as it should be" || echo no; })"
		echo "wake watcher: $(watching && echo running || echo no)"
		echo "takeover.sh on: $([ -f "$TAKEOVER_FLAG" ] && echo yes || echo no)"
		echo "helper on: $(helper_on && echo yes || echo no)"
		;;
	*) echo "usage: sh $0 enable | disable | status | check | refresh | point | watch"; exit 1 ;;
esac
