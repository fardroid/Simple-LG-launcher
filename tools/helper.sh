#!/bin/sh
# The Helper way of opening the launcher on Home, for the native version: the helper of the web version (1.x; tools/homehook:
# watch.sh opens the launcher whenever LG Home comes to the front, homekey.py at the moment the Home key goes down),
# packaged in tools/homehook and copied from there to its folder. The web version writes these files itself (homehook.ts).
#
#   sh helper.sh run       what the launcher starts: homemode.sh set none, then enable, in the background, the result in
#                          homemode.sh's result file (one line: "ok helper" or "failed helper: <why>")
#   sh helper.sh enable    the files copied, the boot link made, the helper started
#   sh helper.sh refresh   at each start of the launcher, while the helper is on: the files brought up to this package's,
#                          the helper started if it is not running (after an update, or when it was stopped)
#
# Switching it off is homemode.sh's (helper_off, with "set" of any other way).
#
# This file is the native version's own (scripts/gen-tools.sh copies it with the id and folders of the build).

HERE=$(cd "$(dirname "$0")" && pwd)
HOOK=/media/developer/launcher-homehook
HOOK_LINK=/var/lib/webosbrew/init.d/48-launcher-homehook
PIDFILE=/tmp/launcher-homehook-watch.pid
ALIVE=/tmp/launcher-alive
RESULT=/tmp/launcher-homemode.result
LOCK=/tmp/launcher-homemode.lock
LOG=/tmp/launcher-homehook.log
FILES="watch.sh apply.sh stock.sh diagnose.sh homekey.py boot.sh"

log () { echo "$(date '+%H:%M:%S') helper: $*" >> "$LOG"; }

running () { pid=$(cat "$PIDFILE" 2>/dev/null); [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; }

# The same as homemode.sh's helper_off, but the boot link is left alone.
stop () {
	pid=$(cat "$PIDFILE" 2>/dev/null)
	[ -n "$pid" ] && { pkill -P "$pid" 2>/dev/null; kill -9 "$pid" 2>/dev/null; }
	pkill -9 -f '[l]auncher-homehook/watch.sh' 2>/dev/null
	kpid=$(cat /tmp/launcher-homekey.pid 2>/dev/null)
	[ -n "$kpid" ] && kill -9 "$kpid" 2>/dev/null
	pkill -9 -f '[l]auncher-homehook/homekey.py' 2>/dev/null
	rm -f "$PIDFILE" /tmp/launcher-homekey.pid
}

same () {    # the installed copy is this package's
	for f in $FILES; do cmp -s "$HERE/homehook/$f" "$HOOK/$f" || return 1; done
}

copy () {
	mkdir -p "$HOOK" || return 1
	# under a temporary name and moved into place, so a running script is never overwritten half-way
	for f in $FILES; do cp -f "$HERE/homehook/$f" "$HOOK/$f.new" || return 1; done
	for f in $FILES; do mv -f "$HOOK/$f.new" "$HOOK/$f" || return 1; done
	chmod +x "$HOOK"/*.sh "$HOOK/homekey.py"
	# the launcher marks itself on screen in this file (watch.sh checks it); it runs without root, so the file is writable to it
	touch "$ALIVE" && chmod 666 "$ALIVE"
}

start () {
	out=$(sh "$HOOK/apply.sh" 2>&1)
	i=0
	while [ "$i" -lt 5 ]; do running && return 0; sleep 1; i=$((i + 1)); done
	echo "$out" | tail -n 1
	return 1
}

enable () {
	[ -f "$HERE/homehook/watch.sh" ] || { echo "failed helper: its files are not in the package"; return 1; }
	copy || { echo "failed helper: the files could not be copied"; return 1; }
	ln -sf "$HOOK/boot.sh" "$HOOK_LINK" || { echo "failed helper: the boot link could not be made"; return 1; }
	stop
	if why=$(start); then log "on"; echo "ok helper"; return 0; fi
	stop
	rm -f "$HOOK_LINK"
	echo "failed helper: it did not start${why:+ ($why)}"
	return 1
}

case "$1" in
	run)
		if [ -f "$LOCK" ] && [ $(( $(date +%s) - $(cat "$LOCK" 2>/dev/null || echo 0) )) -lt 120 ]; then echo "busy"; exit 0; fi
		date +%s > "$LOCK"
		rm -f "$RESULT"
		(
			# homemode.sh takes the lock itself only for "run"; "set" is what it runs inside it
			out=$(sh "$HERE/homemode.sh" set none 2>&1 | tail -n 1)
			case "$out" in
				"ok none") enable > "$RESULT.new" 2>&1 ;;
				*) echo "failed helper: the way that was on could not be switched off (${out#failed none: })" > "$RESULT.new" ;;
			esac
			tail -n 1 "$RESULT.new" > "$RESULT"; rm -f "$RESULT.new" "$LOCK"; log "result: $(cat "$RESULT")"
		) </dev/null >/dev/null 2>&1 &
		echo started
		;;
	enable) enable ;;
	refresh)
		[ -L "$HOOK_LINK" ] || { echo "off"; exit 0; }
		# the boot link of an earlier version (to apply.sh): to boot.sh, which also opens the launcher over the input at a full start
		[ "$(readlink "$HOOK_LINK")" = "$HOOK/boot.sh" ] || { [ -f "$HERE/homehook/boot.sh" ] && ln -sf "$HOOK/boot.sh" "$HOOK_LINK"; }
		if ! same; then
			stop
			copy || { echo "failed: the files could not be copied"; exit 1; }
			log "files brought up to the launcher's"
		fi
		[ -f "$ALIVE" ] || { touch "$ALIVE" && chmod 666 "$ALIVE"; }
		running || start >/dev/null || { echo "failed: it did not start"; exit 1; }
		echo "ok"
		;;
	*) echo "usage: sh $0 run | enable | refresh"; exit 1 ;;
esac
