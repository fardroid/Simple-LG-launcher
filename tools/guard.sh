#!/bin/sh
# What happens to the TV when the launcher goes: the one check for an uninstall, the reset and the uninstall of Settings.
#
# The check. At every full start of the TV, exactly one start-up script asks whether the launcher is still installed:
#   - while a way of opening the launcher is on (Home app, Swap, Helper) or the webOS 26 access is, the start-up script of
#     that way: when it finds the launcher's folder empty it hands over to "check" here and stops;
#   - while none is on, the boot link $LINK of this script, when Settings → "Delete the data when the launcher is uninstalled"
#     is on; with it off and no way on, nothing of the launcher runs at start-up at all.
# "check" waits for the TV's app list (the app manager answers with LG Home in it) and looks twice, 10 s apart, before it
# decides that the launcher is gone: a slow start never counts as an uninstall. Then it takes back what the launcher put on
# the TV: every way switched off (LG Home is Home again, its start-up rules LG's own, the last input as it was), every boot
# link and folder of the launcher, and itself last. With "Delete the data" on ($WIPE_FILE says on, or is missing) it also
# deletes the launcher's data (settings, caches, the collection's photos) and the person's own files it used (the calendar
# links, the wallpapers folder, the web version's settings), and puts back what tools/tv-tweaks.sh changed; with it off, those
# stay and only what the TV needs to work as before is taken back.
#
#   sh guard.sh sync [on|off]  (the installed launcher, and the ways' scripts) the files above as they should be now; on|off is
#                              Settings → "Delete the data when the launcher is uninstalled" (kept as it was when left out)
#   sh guard.sh boot           what the boot link runs: "check" in the background
#   sh guard.sh check          the check; exit 3 when it took everything back, 0 otherwise
#   sh guard.sh reset          Settings → "Reset the launcher" (run detached, after the launcher closes): every way off, the
#                              launcher's data and the person's files deleted, and the launcher started again as if new
#   sh guard.sh uninstall      Settings → "Uninstall the launcher": every way off, the launcher uninstalled, then everything
#                              taken back as above, with the data (it runs from a copy in /tmp, detached)
#   DRY_RUN=1 sh guard.sh check   says what it would do and changes nothing (GUARD_ID, GUARD_APPS, GUARD_LOG for tests)
#
# This file is the native version's own (scripts/gen-tools.sh copies it with the id and folders of the build).

APP_ID=${GUARD_ID:-tld.my.simplelauncher}
APPS=${GUARD_APPS:-/media/developer/apps/usr/palm/applications}
LAUNCHER=${LAUNCHER:-$APPS/$APP_ID}
TOOLS=$APPS/$APP_ID/tools
DATA=/media/developer/simplelauncher
DIR=/media/developer/launcher-guard
LINK=/var/lib/webosbrew/init.d/40-launcher-guard
WIPE_FILE=${GUARD_WIPE:-$DIR/wipe-data}
LOG=${GUARD_LOG:-/tmp/launcher-guard.log}
LOCK=${GUARD_LOCK:-/tmp/launcher-guard.lock}
DH_DIR=/media/developer/launcher-defaulthome
DH_LINK=/var/lib/webosbrew/init.d/45-launcher-defaulthome
TK_DIR=/media/developer/launcher-takeover
TK_LINK=/var/lib/webosbrew/init.d/46-launcher-takeover
HOOK=/media/developer/launcher-homehook
HOOK_LINK=/var/lib/webosbrew/init.d/48-launcher-homehook
ACCESS_DIR=/media/developer/launcher-access
ACCESS_LINK=/var/lib/webosbrew/init.d/44-launcher-access
LASTINPUT=/media/developer/launcher-lastinput.json
TWEAKS=/media/developer/launcher-tweaks
TWEAKS_SCRIPT=/media/developer/tv-tweaks.sh
BOOTD=/var/luna/preferences/bootd.json
SETTINGS=luna://com.webos.settingsservice
AM=luna://com.webos.applicationManager
INSTALL=luna://com.webos.appInstallService
# Written by the Home app way at a full start while its start-up rules are there (defaulthome.sh check): the TV's boot manager
# reads them at a full start only and keeps them until the next one, and /tmp is emptied at every full start. While it is
# there, the TV still opens the launcher when switched on, also once the rules file is gone.
RULES_MARK=${GUARD_MARK:-/tmp/launcher-rules-loaded}
WATCH_PID=/tmp/launcher-guard-watch.pid
# the person's own files the launcher used; shared by the launcher and its native build, so kept while either is installed
WEB_ID=tld.my.simple""launcher
NATIVE_ID=tld.my.simple""launcher.native
PERSONAL="/media/developer/calendar.txt /media/developer/cals /media/developer/wallpapers /media/developer/launcher-config.json"
HOME_ID=com.webos.app.home

log () { echo "$(date '+%H:%M:%S') guard: $*" >> "$LOG"; [ -n "$DRY_RUN" ] && echo "$*"; }
# a change: done, or only said under DRY_RUN
act () { if [ -n "$DRY_RUN" ]; then log "would run: $*"; else "$@"; fi; }
fdclean () {
	_w=0; ( eval "exec 10>&-" ) 2>/dev/null && _w=1
	for _f in /proc/self/fd/*; do
		_n=${_f##*/}
		case "$_n" in ''|*[!0-9]*|0|1|2) continue ;; esac
		[ "${#_n}" -gt 1 ] && [ "$_w" = 0 ] && continue
		eval "exec $_n>&-" 2>/dev/null
	done
}

# A start-up script of the launcher's own that does the check (a way, or the webOS 26 access): then this one has no boot link.
carrier_on () { [ -f "$DH_DIR/enabled" ] || [ -f "$TK_DIR/enabled" ] || [ -e "$HOOK_LINK" ] || [ -e "$ACCESS_LINK" ]; }
wipe_on () { [ "$(cat "$WIPE_FILE" 2>/dev/null)" != off ]; }

# The start-up rules are ours when they name the launcher (LG's own never do).
ours_bootd () { [ -f "$BOOTD" ] && grep -q "\"$APP_ID\"" "$BOOTD"; }

# The app manager's list of apps: printed when it answered and is complete (LG Home is in it), nothing otherwise.
app_list () {
	out=$(luna-send -n 1 -w 20000 "$AM/listApps" '{}' </dev/null 2>/dev/null)
	case "$out" in *'"returnValue":true'*|*'"returnValue": true'*) ;; *) return 1 ;; esac
	echo "$out" | grep -q "\"id\"[[:space:]]*:[[:space:]]*\"$HOME_ID\"" || return 1
	echo "$out"
}
listed () { echo "$1" | grep -q "\"id\"[[:space:]]*:[[:space:]]*\"$2\""; }

# installed, gone (the list came, the launcher is not in it, the apps folder is there and its appinfo.json is not), or
# unknown (no list within the wait)
launcher_state () {
	n=0
	until list=$(app_list); do
		n=$((n + 1)); [ "$n" -ge "${WAIT_ROUNDS:-60}" ] && { echo unknown; return; }
		sleep "${NAP:-10}"
	done
	if listed "$list" "$APP_ID" || [ -f "$LAUNCHER/appinfo.json" ] || [ ! -d "$APPS" ]; then echo installed; else echo gone; fi
}

# The other build of the launcher (web id or native id): installed? Then the person's files stay.
other_installed () {
	list=$(app_list) || return 0                       # cannot tell: keep them
	for id in "$WEB_ID" "$NATIVE_ID"; do
		[ "$id" = "$APP_ID" ] && continue
		listed "$list" "$id" && return 0
		[ -f "$APPS/$id/appinfo.json" ] && return 0
	done
	return 1
}

setting () { echo "$2" | sed -n "s/.*\"$1\"[[:space:]]*:[[:space:]]*\"\([^\"]*\)\".*/\1/p"; }

# The last input as it was before the Swap or the Helper named Home there (lastinput.sh restore, which goes with the app).
lastinput_back () {
	[ -s "$LASTINPUT" ] || return 0
	o=$(cat "$LASTINPUT")
	now=$(luna-send -n 1 "$SETTINGS/getSystemSettings" '{"category":"general","keys":["lastInputApp","physicalLastInputApp"]}' </dev/null 2>/dev/null)
	last=$(setting lastInputApp "$now")
	if { [ "$last" = "$APP_ID" ] || [ "$last" = "$HOME_ID" ]; } && [ -n "$(setting lastInputApp "$o")" ]; then
		act luna-send -n 1 "$SETTINGS/setSystemSettings" "{\"category\":\"general\",\"settings\":{\"lastInputApp\":\"$(setting lastInputApp "$o")\",\"physicalLastInputApp\":\"$(setting physicalLastInputApp "$o")\"}}" </dev/null >/dev/null 2>&1
		log "the last input put back ($(setting lastInputApp "$o"))"
	fi
}

helper_stop () {
	pid=$(cat /tmp/launcher-homehook-watch.pid 2>/dev/null)
	[ -n "$pid" ] && act kill "$pid" 2>/dev/null
	kpid=$(cat /tmp/launcher-homekey.pid 2>/dev/null)
	[ -n "$kpid" ] && act kill "$kpid" 2>/dev/null
	act pkill -f '[l]auncher-homehook/' 2>/dev/null
	return 0
}

# The launcher's data and the person's own files it used.
wipe_data () {
	act rm -rf "$DATA"
	if other_installed; then
		log "the other build of the launcher is installed: the calendar links and the wallpapers stay"
	else
		act rm -rf $PERSONAL
		log "deleted: the launcher's data, the calendar links, the wallpapers folder"
	fi
}

# Everything of the launcher taken back; $1 = wipe | keep (the data and the person's files)
clean_up () {
	log "the launcher is not installed: taking back what it put on the TV (data: $1)"
	in_memory=no; [ -f "$RULES_MARK" ] && in_memory=yes
	# the ways, each with its own copy (they live outside the app folder)
	[ -f "$DH_DIR/enabled" ] && [ -f "$DH_DIR/defaulthome.sh" ] && { act sh "$DH_DIR/defaulthome.sh" disable >/dev/null 2>&1; log "Home app: LG Home is the TV's Home again"; }
	[ -f "$TK_DIR/enabled" ] && [ -f "$TK_DIR/takeover.sh" ] && { act sh "$TK_DIR/takeover.sh" disable >/dev/null 2>&1; log "Swap: LG Home is back in its place"; }
	grep -q " /usr/palm/applications/$HOME_ID " /proc/mounts 2>/dev/null && act umount -l "/usr/palm/applications/$HOME_ID"
	helper_stop
	[ -f "$ACCESS_DIR/webos26-access.sh" ] && act sh "$ACCESS_DIR/webos26-access.sh" remove >/dev/null 2>&1
	lastinput_back
	if ours_bootd; then act rm -f "$BOOTD"; act sync; log "the start-up rules are LG's again (from the next full start)"; fi
	act rm -f "$DH_LINK" "$TK_LINK" "$HOOK_LINK" "$ACCESS_LINK" "$LASTINPUT"
	act rm -rf "$DH_DIR" "$TK_DIR" "$HOOK" "$ACCESS_DIR"
	if [ "$1" = wipe ]; then
		if [ -d "$TWEAKS" ] && [ -f "$TWEAKS_SCRIPT" ]; then act sh "$TWEAKS_SCRIPT" restore >/dev/null 2>&1; log "tv-tweaks: LG's values put back"; fi
		act rm -rf "$TWEAKS"; act rm -f "$TWEAKS_SCRIPT"
		wipe_data
	else
		log "Delete the data was off: the launcher's data, the calendar links and the wallpapers stay"
	fi
	act rm -f /tmp/launcher-* 2>/dev/null
	act rm -f "$LINK"
	act rm -rf "$DIR"
	act sync
	log "done: nothing of the launcher runs on the TV any more"
	# the TV still has the launcher's start-up rules in memory: only a full start makes it read LG's again
	if [ "$in_memory" = yes ]; then
		log "the TV keeps the launcher's start-up rules until a full start: restarting it"
		restart_tv
	fi
}

# Three looks, ${RECHECK:-15} s apart: each waits for the TV's whole app list and asks whether the launcher is in it and in its
# folder; only "gone" three times running counts (an update replaces the app in a few seconds, a slow start mounts the apps
# folder late).
check () {
	mkdir "$LOCK" 2>/dev/null || { log "another check is running"; return 0; }
	state=$(launcher_state); looks=1
	while [ "$state" = gone ] && [ "$looks" -lt 3 ]; do sleep "${RECHECK:-15}"; state=$(launcher_state); looks=$((looks + 1)); done
	case "$state" in
		installed) rmdir "$LOCK"; [ -n "$DRY_RUN" ] && echo installed; return 0 ;;
		unknown) rmdir "$LOCK"; log "the TV's app list did not come: nothing done"; return 0 ;;
	esac
	if wipe_on; then clean_up wipe; else clean_up keep; fi
	rmdir "$LOCK" 2>/dev/null
	return 3
}

# The watcher, only while the Home app way or the Swap is on: an uninstall in another way would leave the TV with something
# wrong until its next full start (the Home app way: its start-up rules in memory, the store at every power-on; the Swap: the
# Home button opening nothing). The Helper and the webOS 26 access do nothing without the launcher: the check at the next
# full start is enough for them. Every 5 s: is the launcher's appinfo.json still there? When it is gone, the check above decides
# (three looks, so an update is not taken for it) and takes everything back at once. It runs from the copy in $DIR, which
# outlives the app, and ends by itself when neither way is on.
watch_needed () { [ -f "$DH_DIR/enabled" ] || [ -f "$TK_DIR/enabled" ]; }
watching () { pid=$(cat "$WATCH_PID" 2>/dev/null); [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; }
watch_start () {
	watching && return 0
	[ -f "$DIR/guard.sh" ] || return 0
	( fdclean; setsid nohup sh "$DIR/guard.sh" watch </dev/null >/dev/null 2>&1 & )
}
watch_stop () { pid=$(cat "$WATCH_PID" 2>/dev/null); [ -n "$pid" ] && kill "$pid" 2>/dev/null; rm -f "$WATCH_PID"; }
watch_loop () {
	echo $$ > "$WATCH_PID"
	while watch_needed; do
		if [ ! -f "$LAUNCHER/appinfo.json" ]; then
			log "watch: the launcher's folder is empty: checking"
			check; [ $? = 3 ] && exit 0
			sleep 60
		fi
		sleep 5
	done
	rm -f "$WATCH_PID"
}

# The files as they should be: this script in $DIR while anything may need it, its own boot link only while no other
# start-up script of the launcher does the check and the data is to be deleted.
sync_files () {
	case "$1" in on|off) mkdir -p "$DIR" && echo "$1" > "$WIPE_FILE" ;; esac
	want_dir=no; want_link=no
	if carrier_on; then want_dir=yes; elif wipe_on; then want_dir=yes; want_link=yes; fi
	if [ "$want_dir" = yes ]; then
		mkdir -p "$DIR"
		here=$(cd "$(dirname "$0")" && pwd)
		[ "$here" = "$DIR" ] || cmp -s "$0" "$DIR/guard.sh" || cp -f "$0" "$DIR/guard.sh"
		chmod +x "$DIR/guard.sh"
		[ -f "$WIPE_FILE" ] || echo on > "$WIPE_FILE"
	fi
	if [ "$want_link" = yes ]; then
		text=$(printf '#!/bin/sh\n# Takes back what the launcher put on the TV once it is uninstalled. See %s\nsh %s boot >/dev/null 2>&1 &' "$DIR/guard.sh" "$DIR/guard.sh")
		if [ "$(cat "$LINK" 2>/dev/null)" != "$text" ]; then
			printf '%s\n' "$text" > "$LINK.new" && chmod +x "$LINK.new" && mv -f "$LINK.new" "$LINK" && log "boot link written"
		fi
	elif [ -e "$LINK" ]; then
		rm -f "$LINK"; log "boot link removed (a way's script does the check, or none is wanted)"
	fi
	[ "$want_dir" = no ] && [ -d "$DIR" ] && { rm -rf "$DIR"; log "nothing of the launcher runs at start-up: the check removed"; }
	sync
	if watch_needed; then watch_start; else watch_stop; fi
	if [ "$want_link" = yes ]; then echo "check: own; delete the data: on"
	elif [ "$want_dir" = yes ]; then echo "check: in the ways' scripts; delete the data: $(wipe_on && echo on || echo off)"
	else echo "check: none; delete the data: off"; fi
}

# Every way off with the installed launcher's own scripts (it is still there), and the last input as it was.
ways_off () {
	sh "$TOOLS/homemode.sh" set none >/dev/null 2>&1
	sh "$TOOLS/lastinput.sh" restore >/dev/null 2>&1
	ours_bootd && { rm -f "$BOOTD"; sync; }
	true
}

# a full restart through the TV's power service (as its own menu does), the root shell's reboot if it says no
restart_tv () {
	[ -n "$DRY_RUN" ] && { log "would restart the TV"; return 0; }
	luna-send -n 1 -w 10000 luna://com.webos.service.tvpower/power/reboot '{"reason":"reset"}' </dev/null 2>/dev/null | grep -q '"returnValue":true' || reboot
}
relaunch () { luna-send -n 1 "$AM/launch" "{\"id\":\"$APP_ID\"}" </dev/null >/dev/null 2>&1; }

case "${1:-boot}" in
	sync) sync_files "$2" ;;
	boot) ( fdclean; setsid nohup sh "$0" check </dev/null >/dev/null 2>&1 & ) ;;
	check) check ;;
	watch) watching && [ "$(cat "$WATCH_PID")" != $$ ] && exit 0; watch_loop ;;
	watch-start) watch_needed && watch_start ;;
	reset)
		# after the launcher has closed: as if just installed (no way on, "Delete the data" on, nothing of the person's)
		[ -n "$GUARD_NOW" ] || { ( fdclean; GUARD_NOW=1 setsid nohup sh "$0" reset </dev/null >/dev/null 2>&1 & ); echo started; exit 0; }
		sleep 2
		log "reset: every way off, the launcher's data and the person's files deleted"
		ways_off
		wipe_data
		sync_files on >/dev/null
		if [ -f "$RULES_MARK" ]; then
			# the TV keeps the Home app way's start-up rules until a full start: it restarts, and opens what it opens by itself
			log "reset: done; the TV keeps the launcher's start-up rules until a full start: restarting it"; restart_tv
		else
			relaunch; log "reset: done, the launcher started again"
		fi
		;;
	uninstall)
		# from a copy in /tmp, detached: the app folder (and this script's folder) go during the run
		case "$0" in /tmp/*) ;; *) cp -f "$0" /tmp/launcher-guard-uninstall.sh; ( fdclean; setsid nohup sh /tmp/launcher-guard-uninstall.sh uninstall </dev/null >/dev/null 2>&1 & ); echo started; exit 0 ;; esac
		sleep 2
		log "uninstall: every way off, then the launcher uninstalled"
		ways_off
		for service in dev/remove remove; do
			luna-send -n 1 -w 30000 "$INSTALL/$service" "{\"id\":\"$APP_ID\",\"subscribe\":false}" </dev/null >> "$LOG" 2>&1
			i=0; while [ "$i" -lt 30 ] && [ -f "$LAUNCHER/appinfo.json" ]; do sleep 1; i=$((i + 1)); done
			[ -f "$LAUNCHER/appinfo.json" ] || break
		done
		if [ -f "$LAUNCHER/appinfo.json" ]; then
			log "uninstall: the TV did not remove the launcher: nothing more done"; relaunch; rm -f "$0"; exit 1
		fi
		RECHECK=5; NAP=5; WAIT_ROUNDS=24
		if [ "$(launcher_state)" = gone ]; then clean_up wipe; else log "uninstall: the app list still names the launcher: nothing more done"; fi
		rm -f "$0"
		;;
	*) echo "usage: sh $0 sync [on|off] | boot | check | watch-start | reset | uninstall"; exit 1 ;;
esac
