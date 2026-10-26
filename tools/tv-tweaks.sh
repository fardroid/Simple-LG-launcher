#!/bin/sh
# Lightens a rooted LG webOS TV for this launcher: stops preloading apps nobody asked for, and switches off LG's
# promotions and Live Plus (ACR: the TV recognising what is on screen). Everything is reversible and nothing on the
# read-only system is touched. Ideas and setting names from webos-debloat by lucataco (MIT):
# https://gist.github.com/lucataco/7c303b443934008a9031a55cfca552ec
#
# Run on the TV as root:   sh tv-tweaks.sh apply | restore | status
#
# Preloads: the preload manager keeps some apps in memory before anyone opens them. Turning that off for LG Channels and
# the browser leaves more memory, so the TV is less likely to unload the launcher while another app runs. The setting is
# made at once (setPreloadPolicy) and kept across restarts (the allowedApps map of the preload manager's own file).
# Home, Live TV, the inputs, Netflix, YouTube and Prime Video stay preloaded (they open faster that way).
#
# The first run saves the preload file and the settings' old values in $DIR; restore puts them back.

DIR=/media/developer/launcher-tweaks
CONF=/var/preferences/webos-preload-manager-conf.json
PRELOAD=luna://com.webos.service.preloadmanager
SAM=luna://com.webos.service.applicationmanager
SETTINGS=luna://com.webos.settingsservice
PRELOAD_OFF='com.webos.app.lgchannels com.webos.app.browser'
SETTINGS_OFF='general:homePromotion general:sportsAlarm option:livePromotion option:webOSPromotionVideo option:promotionStreamer option:livePlus other:contentRecommendation'

say () { echo "$*"; }

get_setting () {    # category key -> value, or nothing when the TV has no such setting
	luna-send -n 1 "$SETTINGS/getSystemSettings" "{\"category\":\"$1\",\"keys\":[\"$2\"]}" 2>/dev/null \
		| sed -n "s/.*\"$2\"[[:space:]]*:[[:space:]]*\"\([^\"]*\)\".*/\1/p"
}

set_setting () {    # category key value
	luna-send -n 1 "$SETTINGS/setSystemSettings" "{\"category\":\"$1\",\"settings\":{\"$2\":\"$3\"}}" 2>&1 | grep -q '"returnValue":true'
}

set_preload () {    # id true|false: at once, and in the file the preload manager reads when it starts
	luna-send -n 1 "$PRELOAD/setPreloadPolicy" "{\"application\":{\"id\":\"$1\",\"isEnabled\":$2}}" 2>&1 | grep -q '"returnValue":true' || say "  preload manager refused $1"
	if [ -f "$CONF" ]; then
		re=$(printf '%s' "$1" | sed 's/\./\\./g')
		if [ "$2" = false ]; then from=true; else from=false; fi
		sed -i "s/\(\"$re\"[[:space:]]*:[[:space:]]*\)$from/\1$2/" "$CONF"
	fi
}

apply () {
	mkdir -p "$DIR"
	[ -f "$CONF" ] && [ ! -f "$DIR/preload-conf.orig" ] && cp "$CONF" "$DIR/preload-conf.orig"
	fg=$(luna-send -n 1 "$SAM/getForegroundAppInfo" '{}' 2>/dev/null)
	for id in $PRELOAD_OFF; do
		set_preload "$id" false
		say "preload off: $id"
		case "$fg" in *"\"appId\":\"$id\""*) continue ;; esac       # never close what is on screen
		luna-send -n 1 "$SAM/running" '{}' 2>/dev/null | grep -q "\"$id\"" || continue
		if luna-send -n 1 "$SAM/closeByAppId" "{\"id\":\"$id\"}" 2>&1 | grep -q '"returnValue":true'; then say "  closed $id (it was waiting in memory)"; else say "  could not close $id"; fi
	done
	for ck in $SETTINGS_OFF; do
		c=${ck%%:*}; k=${ck#*:}
		cur=$(get_setting "$c" "$k")
		if [ -z "$cur" ]; then say "setting $c/$k: not on this TV"; continue; fi
		grep -q "^$c $k " "$DIR/settings.orig" 2>/dev/null || echo "$c $k $cur" >> "$DIR/settings.orig"
		if [ "$cur" = off ]; then say "setting $c/$k: already off"; continue; fi
		set_setting "$c" "$k" off && say "setting $c/$k: off (was $cur)"
	done
}

restore () {
	if [ -f "$DIR/preload-conf.orig" ]; then cp "$DIR/preload-conf.orig" "$CONF" && say "preload file restored"; fi
	for id in $PRELOAD_OFF; do luna-send -n 1 "$PRELOAD/setPreloadPolicy" "{\"application\":{\"id\":\"$id\",\"isEnabled\":true}}" >/dev/null 2>&1; say "preload on: $id"; done
	if [ -f "$DIR/settings.orig" ]; then
		while read -r c k v; do [ -n "$v" ] && set_setting "$c" "$k" "$v" && say "setting $c/$k: back to $v"; done < "$DIR/settings.orig"
	fi
}

status () {
	grep MemAvailable /proc/meminfo
	luna-send -n 1 "$PRELOAD/getPreloadPolicy" '{}' 2>/dev/null | tr '{' '\n' | sed -n 's/.*"id":"\([^"]*\)".*"isEnabled":\([a-z]*\).*/  preload \1: \2/p'
	for ck in $SETTINGS_OFF; do c=${ck%%:*}; k=${ck#*:}; echo "  $c/$k: $(get_setting "$c" "$k")"; done
}

case "$1" in
	apply) apply ;;
	restore) restore ;;
	status) status ;;
	*) echo "usage: sh $0 apply | restore | status"; exit 1 ;;
esac
