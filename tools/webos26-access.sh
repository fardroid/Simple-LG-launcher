#!/bin/sh
# webOS 26: lets the launcher use the Homebrew Channel's command service again.
#
# On webOS 26 (internal release 11.x) apps are no longer in the "public" permission group, and the Homebrew Channel (0.7.3)
# opens its service luna://org.webosbrew.hbchannel.service/exec to that group only. Every command the launcher runs is then
# refused inside the app ("Denied method call "exec" for category "/""), even on a rooted TV: the live app list, wallpapers
# from the TV's storage, the news through the Homebrew Channel and the ways of opening the launcher on Home stop working.
# Adding the group org.webosbrew.hbchannel.service.group to the launcher's Luna client permissions lets it through (the
# Homebrew Channel's own app has that group). webOS 25 and older do not need this, and there this script does nothing.
#
# The installer writes the launcher's permission file again on every install or update of the launcher, without the group.
# So a watcher stays running in the background (started at each boot from the Homebrew Channel's startup folder): every few
# seconds it checks the file, and when the group is missing (just after an update) it adds it, has the Luna hub read the
# permissions again and restarts the launcher if it is open (an app keeps the permissions it started with). In front, it is
# opened again at once; in the background it is only closed, and the next Home press opens it with the new permissions.
#
#   sh webos26-access.sh install   once, over SSH: copies itself to /media/developer/launcher-access, adds the boot link,
#                                  fixes the permission now and starts the watcher. npm run install:tv does it by itself.
#   sh webos26-access.sh remove    undoes it: the watcher stops, the boot link and the copy go (the permission stays until
#                                  the launcher is next installed)
#   sh webos26-access.sh status
#   (no argument, from the boot link) starts the watcher in the background and returns at once
#
# Based on Hearth's tools/webos11-permission.sh, by ganondorft (Hearth pull request #6). Here it also watches for updates.
# The finding is webosbrew/webos-homebrew-channel#255; the Homebrew Channel's own fix (#233) is not in a release yet.
# Set FORCE=1 to run it on webOS 25 or older (for testing).

APP_ID=tld.my.simplelauncher
GROUP=org.webosbrew.hbchannel.service.group
PERMS=/var/luna-service2-dev/client-permissions.d/$APP_ID.app.json
LAUNCHER=/media/developer/apps/usr/palm/applications/$APP_ID
DIR=/media/developer/launcher-access
COPY=$DIR/webos26-access.sh
LINK=/var/lib/webosbrew/init.d/44-launcher-access
PID=/tmp/launcher-access.pid
LOG=/tmp/launcher-access.log
AM=luna://com.webos.applicationManager
EVERY=5

log () { echo "$(date '+%H:%M:%S') access: $*" | tee -a "$LOG"; }

# webOS 26 and newer (internal release 11 and up)
needed () {
	[ "$FORCE" = 1 ] && return 0
	release=$(nyx-cmd OSInfo query webos_release 2>/dev/null)
	[ "${release%%.*}" -ge 11 ] 2>/dev/null
}

watching () { [ -f "$PID" ] && kill -0 "$(cat "$PID")" 2>/dev/null; }

# The launcher open again with the new permissions: in front, closed and opened; in the background, only closed.
restart () {
	front=$(luna-send -n 1 "$AM/getForegroundAppInfo" '{}' 2>/dev/null | sed -n 's/.*"appId"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p')
	luna-send -n 1 "$AM/running" '{}' 2>/dev/null | grep -q "\"$APP_ID\"" || return 0
	luna-send -n 1 "$AM/closeByAppId" "{\"id\":\"$APP_ID\"}" >/dev/null 2>&1
	if [ "$front" = "$APP_ID" ]; then
		sleep 1
		luna-send -n 1 "$AM/launch" "{\"id\":\"$APP_ID\"}" >/dev/null 2>&1
		log "launcher reopened"
	else
		log "launcher closed in the background; Home opens it again"
	fi
}

# Adds the group when it is missing; true when it changed something. Only after an existing entry, so an empty list can
# never turn into broken JSON.
fix () {
	[ -f "$PERMS" ] || return 1
	grep -q "$GROUP" "$PERMS" && return 1
	sed -i "s/\"\]}\$/\",\"$GROUP\"]}/" "$PERMS"
	if grep -q "$GROUP" "$PERMS"; then
		/usr/sbin/ls-control scan-services >/dev/null 2>&1
		log "permission added"
		return 0
	fi
	log "could not add the permission: unexpected file format"
	return 1
}

watch () {
	echo $$ > "$PID"
	while [ -e "$LINK" ]; do
		fix && restart
		sleep "$EVERY"
	done
	rm -f "$PID"
}

case "$1" in
	install)
		needed || { echo "not needed on this webOS ($(nyx-cmd OSInfo query webos_release 2>/dev/null))"; exit 0; }
		[ -d /var/lib/webosbrew/init.d ] || { echo "no /var/lib/webosbrew/init.d: is the Homebrew Channel installed?"; exit 1; }
		mkdir -p "$DIR"
		[ "$(readlink -f "$0")" = "$COPY" ] || cp "$0" "$COPY"
		chmod 755 "$COPY"
		ln -sf "$COPY" "$LINK"
		fix && restart
		watching || ( setsid nohup sh "$COPY" watch </dev/null >/dev/null 2>&1 & )
		echo "installed: the launcher keeps its access to the Homebrew Channel, also after updates"
		;;
	remove)
		rm -f "$LINK"
		watching && kill "$(cat "$PID")" 2>/dev/null
		rm -f "$PID"
		rm -rf "$DIR"
		echo "removed"
		;;
	status)
		if [ -e "$LINK" ]; then echo "boot link: on"; else echo "boot link: off"; fi
		if watching; then echo "watcher: running"; else echo "watcher: stopped"; fi
		if [ ! -f "$PERMS" ]; then echo "permission: the launcher is not installed"
		elif grep -q "$GROUP" "$PERMS"; then echo "permission: present"
		else echo "permission: missing"; fi
		;;
	'')
		# the startup folder runs its scripts without arguments: start the watcher, never hold up the TV's start
		case "$0" in */init.d/*) ;; *) echo "usage: sh $0 install|remove|status"; exit 0 ;; esac
		needed || exit 0
		watching || ( setsid nohup sh "$COPY" watch </dev/null >/dev/null 2>&1 & )
		;;
	watch)
		watch
		;;
	*)
		echo "usage: sh $0 install|remove|status"
		;;
esac
exit 0
