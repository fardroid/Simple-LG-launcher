#!/bin/sh
# The move from the web version (1.x) to this one, by itself, after the update through the Homebrew Channel: what the web
# version's ways of opening the launcher put on the TV is turned into this version's. Run by the launcher at every start
# (src/main.cpp), and by index.html when the TV still opens the web version's Swap. It does nothing once there is nothing of
# the web version's left, so running it again is harmless. Prints one line per thing it moved.
#
#   Swap      the web version's takeover.sh (its copy in the takeover folder) put a web app with LG Home's id in LG Home's
#             place and gave Home the Homebrew Channel's service: after the update that web app is gone (Home opens nothing).
#             Its own copy takes it off (and the permissions with it), and this version's Swap is put on instead.
#   Helper    the web version kept "on" in its settings (replaceHome) and wrote the files itself, with a version file this
#             version does not write: this version's helper is switched on in its place.
#   Official  defaulthome.sh is the same for both (its copy is refreshed at every start): nothing to move.
#
# This file is the native version's own (scripts/gen-tools.sh copies it with the id and folders of the build).

HERE=$(cd "$(dirname "$0")" && pwd)
TAKEOVER_DIR=/media/developer/launcher-takeover
HOOK=/media/developer/launcher-homehook
CONFIG=/media/developer/launcher-config.json
LOG=/tmp/launcher-homehook.log
LOCK=/tmp/launcher-migrate.lock

log () { echo "$(date '+%H:%M:%S') migrate: $*" >> "$LOG"; }

# one run at a time (the launcher and index.html can both start one); a lock older than two minutes is left over
if [ -f "$LOCK" ] && [ $(( $(date +%s) - $(cat "$LOCK" 2>/dev/null || echo 0) )) -lt 120 ]; then echo "busy"; exit 0; fi
date +%s > "$LOCK"
trap 'rm -f "$LOCK"' EXIT

if [ -f "$TAKEOVER_DIR/takeover.sh" ] && grep -q '"type": "web"' "$TAKEOVER_DIR/takeover.sh"; then
	was_on=no
	[ -f "$TAKEOVER_DIR/enabled" ] && was_on=yes
	log "the web version's Swap found (on: $was_on): taken off"
	sh "$TAKEOVER_DIR/takeover.sh" disable >/dev/null 2>&1
	rm -f "$TAKEOVER_DIR/takeover.sh" "$TAKEOVER_DIR/home-permissions.json"
	rm -rf "$TAKEOVER_DIR/app"
	if [ "$was_on" = yes ]; then
		if sh "$HERE/takeover.sh" enable >/dev/null 2>&1; then log "this version's Swap is on"; echo "swap moved"
		else log "this version's Swap did not take: LG Home is Home"; echo "swap off"; fi
	else
		echo "swap removed"
	fi
fi

if [ -f "$HOOK/version" ]; then
	rm -f "$HOOK/version"
	if grep -q '"replaceHome":true' "$CONFIG" 2>/dev/null && [ ! -f "$TAKEOVER_DIR/enabled" ] && [ ! -f /media/developer/launcher-defaulthome/enabled ]; then
		log "the web version's helper was on: this version's switched on"
		if sh "$HERE/helper.sh" enable >/dev/null 2>&1; then echo "helper moved"; else echo "helper off"; fi
	fi
fi
true
