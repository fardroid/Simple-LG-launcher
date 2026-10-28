#!/bin/sh
# Switches how the launcher replaces LG Home, from Settings → Home screen → "How the launcher opens" (or by hand).
# One way at a time: the way that is on now is switched off and checked to be off before the new one is switched on,
# and a way that does not take is undone, so the TV is never left with two of them or none half-done.
#
#   sh homemode.sh status            prints mode=<none|helper|default|mount>, can_default=<yes|no>, release=<webOS release>
#   sh homemode.sh set <none|default|mount>
#   sh homemode.sh run <none|default|mount>     what the launcher starts: set, in the background, with the result in $RESULT
#
# The ways:
#   default  tools/defaulthome.sh: the TV's own choice of Home app (webOS 25 and newer)
#   mount    tools/takeover.sh: the launcher mounted in LG Home's own folder
#   helper   the Home key helper (src/homehook): the launcher installs and starts it itself, after "set none" here
#   none     LG Home is Home; the launcher is an ordinary app
#
# The run takes the launcher away from the screen for a moment with "mount" (and when "mount" is switched off), so the
# launcher does not wait for the answer: it reads $RESULT (one line: "ok <mode>" or "failed <mode>: <why>").

HERE=$(cd "$(dirname "$0")" && pwd)
TAKEOVER_DIR=/media/developer/launcher-takeover
DEFAULT_DIR=/media/developer/launcher-defaulthome
HOOK=/media/developer/launcher-homehook
HOOK_LINK=/var/lib/webosbrew/init.d/48-launcher-homehook
RESULT=/tmp/launcher-homemode.result
LOCK=/tmp/launcher-homemode.lock
LOG=/tmp/launcher-homehook.log
AM=luna://com.webos.service.applicationmanager

log () { echo "$(date '+%H:%M:%S') homemode: $*" >> "$LOG"; }

helper_on () {
	[ -L "$HOOK_LINK" ] && return 0
	pid=$(cat /tmp/launcher-homehook-watch.pid 2>/dev/null)
	[ -n "$pid" ] && kill -0 "$pid" 2>/dev/null
}

mode () {
	if [ -f "$TAKEOVER_DIR/enabled" ]; then echo mount
	elif [ -f "$DEFAULT_DIR/enabled" ]; then echo default
	elif helper_on; then echo helper
	else echo none
	fi
}

can_default () {
	luna-send -n 1 "$AM/getAppInfo" '{"id":"com.webos.app.home","properties":["id","appCategory"]}' 2>/dev/null | grep -q '"appCategory"[^]]*"home"'
}

# The same as the launcher's own "helper off" (homehookcmd.ts, STOP_WATCHER): the watcher and the Home key helper
# stopped, the boot link removed. Its files stay, so switching it on again is quick.
helper_off () {
	pid=$(cat /tmp/launcher-homehook-watch.pid 2>/dev/null)
	[ -n "$pid" ] && { pkill -P "$pid" 2>/dev/null; kill -9 "$pid" 2>/dev/null; }
	pkill -9 -f '[l]auncher-homehook/watch.sh' 2>/dev/null
	kpid=$(cat /tmp/launcher-homekey.pid 2>/dev/null)
	[ -n "$kpid" ] && kill -9 "$kpid" 2>/dev/null
	pkill -9 -f '[l]auncher-homehook/homekey.py' 2>/dev/null
	rm -f /tmp/launcher-homehook-watch.pid /tmp/launcher-homekey.pid "$HOOK_LINK" "$TAKEOVER_DIR/48-launcher-homehook.off"
}

all_off () {
	[ -f "$TAKEOVER_DIR/enabled" ] && sh "$HERE/takeover.sh" disable >/dev/null 2>&1
	[ -f "$DEFAULT_DIR/enabled" ] && sh "$HERE/defaulthome.sh" disable >/dev/null 2>&1
	# takeover.sh disable starts the helper again if it had been on before; it is not wanted now
	helper_off
	[ "$(mode)" = none ]
}

# Turns on a way again after a switch to another one failed, so the TV is left as it was. "helper" is the launcher's to start.
restore () {
	case "$1" in
		default) sh "$HERE/defaulthome.sh" enable >/dev/null 2>&1 ;;
		mount) sh "$HERE/takeover.sh" enable >/dev/null 2>&1 ;;
		*) return 0 ;;
	esac
	if [ "$(mode)" = "$1" ]; then log "back to $1"; echo " (back to $1)"; else log "$1 could not be turned on again"; echo " ($1 could not be turned on again)"; fi
}

set_mode () {
	want=$1
	was=$(mode)
	[ "$was" = "$want" ] && { echo "ok $want"; return 0; }
	log "switching from $was to $want"
	if ! all_off; then echo "failed $want: $was could not be switched off (now: $(mode))"; return 1; fi
	case "$want" in
		none) echo "ok none" ;;
		default)
			out=$(sh "$HERE/defaulthome.sh" enable 2>&1)
			if [ "$(mode)" = default ]; then echo "ok default"; else echo "failed default: $(echo "$out" | tail -n 1)$(restore "$was")"; return 1; fi
			;;
		mount)
			out=$(sh "$HERE/takeover.sh" enable 2>&1)
			# takeover.sh puts LG Home back itself when the TV does not take the launcher
			if [ "$(mode)" = mount ] && grep -q " /usr/palm/applications/com.webos.app.home " /proc/mounts; then echo "ok mount"
			else sh "$HERE/takeover.sh" disable >/dev/null 2>&1; helper_off; echo "failed mount: $(echo "$out" | tail -n 1)$(restore "$was")"; return 1; fi
			;;
	esac
}

case "$1" in
	status)
		echo "mode=$(mode)"
		echo "can_default=$(can_default && echo yes || echo no)"
		echo "release=$(nyx-cmd OSInfo query webos_release 2>/dev/null)"
		;;
	set)
		case "$2" in none|default|mount) ;; *) echo "usage: sh $0 set none|default|mount"; exit 1 ;; esac
		set_mode "$2"
		;;
	run)
		case "$2" in none|default|mount) ;; *) echo "failed $2: unknown mode" > "$RESULT"; exit 1 ;; esac
		# one switch at a time; a lock older than two minutes is left over from a run that never finished
		if [ -f "$LOCK" ] && [ $(( $(date +%s) - $(cat "$LOCK" 2>/dev/null || echo 0) )) -lt 120 ]; then echo "busy"; exit 0; fi
		date +%s > "$LOCK"
		rm -f "$RESULT"
		( set_mode "$2" > "$RESULT.new" 2>&1; tail -n 1 "$RESULT.new" > "$RESULT"; rm -f "$RESULT.new" "$LOCK"; log "result: $(cat "$RESULT")" ) </dev/null >/dev/null 2>&1 &
		echo started
		;;
	*) echo "usage: sh $0 status | set <none|default|mount> | run <none|default|mount>"; exit 1 ;;
esac
