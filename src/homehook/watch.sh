#!/bin/sh
# Watches which app is in front. When the TV brings LG Home to the front (Home button, leaving
# an app with Back, boot) it opens the launcher instead, then CHECKS that the launcher really
# showed itself and asks again if it did not. Started by apply.sh; stopped by a reboot or by
# turning the setting off. Writes to /tmp/launcher-homehook.log.
#
# It listens to the TV's own "app in front" reports (a subscription). If that subscription keeps
# ending at once on some TV, it asks once a second instead.
#
# How it knows the launcher showed itself: the launcher touches /tmp/launcher-alive each time it
# becomes visible (when the setting is on). After opening it, this script waits a few seconds and
# looks for that file being newer than its own mark.

LAUNCHER_ID="${LAUNCHER_ID:-tld.my.simplelauncher}"
LAUNCHER="${LAUNCHER:-/media/developer/apps/usr/palm/applications/tld.my.simplelauncher}"
LUNA="${LUNA_SEND:-luna-send}"
PIDFILE="${PIDFILE:-/tmp/launcher-homehook-watch.pid}"
LOG="${LOG:-/tmp/launcher-homehook.log}"
ALLOW="${ALLOW:-/tmp/launcher-allow-stock-home}"     # exists while the user asked for LG's own Home
TAKEOVER="${TAKEOVER:-/media/developer/launcher-takeover/enabled}"   # the launcher is the TV's Home itself (tools/takeover.sh): nothing to do
ALIVE="${ALIVE:-/tmp/launcher-alive}"                # touched by the launcher whenever it becomes visible
MARK="${MARK:-/tmp/launcher-launch-mark}"            # touched when the launcher is asked for
VLOCK="${VLOCK:-/tmp/launcher-verify-lock}"          # holds the time a check started: only one check runs at a time
VFIX="${VFIX:-/tmp/launcher-verify-fix}"             # holds the time a check last had to ask for the launcher again
FRONT_URI="luna://com.webos.applicationManager/getForegroundAppInfo"
HOME_PATTERN='*"appId":"com.webos.app.home"*'
LAUNCHER_PATTERN="*\"appId\":\"$LAUNCHER_ID\"*"

log () { echo "$(date '+%H:%M:%S') watcher: $*" >> "$LOG"; }

open_launcher () {
    "$LUNA" -n 1 luna://com.webos.applicationManager/launch "{\"id\":\"$LAUNCHER_ID\"}" >/dev/null 2>&1
}

# After the launcher was asked for: wait, and if it has not shown itself (or LG Home is still
# in front) ask again, up to three times. Runs in the background.
verify () {
    [ -f "$MARK" ] || touch "$MARK"
    tries=0
    while [ "$tries" -lt 3 ]; do
        sleep "${VERIFY_WAIT:-3}"
        [ -f "$ALIVE" ] && [ "$ALIVE" -nt "$MARK" ] && return   # the launcher has shown itself
        [ -f "$ALLOW" ] && return                                # LG's own Home was asked for
        front=$("$LUNA" -n 1 "$FRONT_URI" '{}' 2>/dev/null)
        case "$front" in
            $HOME_PATTERN|$LAUNCHER_PATTERN) ;;                  # nothing showing yet: ask again
            *) return ;;                                         # another app is in front: done
        esac
        tries=$((tries + 1))
        case "$front" in
            $HOME_PATTERN) where="LG Home" ;;
            *) where="the launcher" ;;
        esac
        log "the launcher has not shown itself ($where is in front): opening it again (try $tries of 3)"
        echo "${FAKE_NOW:-$(date +%s)}" > "$VFIX"
        open_launcher
        touch "$MARK"
    done
}

# Runs a check in the background, unless one is already running. (Every report that the launcher is in
# front used to start its own check; with several at once, each could ask for the launcher again and
# take the screen back from an app that was just being opened, over and over.) A lock older than 30 s
# is taken to be left over.
start_verify () {
    [ -n "$NO_VERIFY" ] && return
    now="${FAKE_NOW:-$(date +%s)}"
    if [ -f "$VLOCK" ]; then
        began=$(cat "$VLOCK" 2>/dev/null); began="${began:-0}"
        [ $((now - began)) -lt 30 ] && return
    fi
    echo "$now" > "$VLOCK"
    ( ( verify; rm -f "$VLOCK" ) >/dev/null 2>&1 & )
}

# True when a check had to ask for the launcher again less than 15 s ago.
recently_fixed () {
    now="${FAKE_NOW:-$(date +%s)}"
    [ -f "$VFIX" ] || return 1
    at=$(cat "$VFIX" 2>/dev/null); at="${at:-0}"
    [ $((now - at)) -lt 15 ]
}

# One report from the TV about the app in front. The launch comes first (no commands are run
# before it, so the hand-over is as quick as it can be); the log line and the check come after.
was_home=0
last_launch=0
handle () {
    line="$1"
    [ -f "$TAKEOVER" ] && return
    case "$line" in
        *'"appId":"'*) ;;
        *) return ;;                                  # not a report about the app in front
    esac
    case "$line" in
        $HOME_PATTERN)
            if [ "$was_home" = 1 ]; then              # LG Home was already in front: a repeat report
                now="${FAKE_NOW:-$(date +%s)}"
                [ $((now - last_launch)) -lt 3 ] && return
            fi
            was_home=1
            [ -f "$ALLOW" ] && return                 # the user asked for LG's own Home: leave it
            [ -f "$LAUNCHER/appinfo.json" ] || return # nothing to open (launcher uninstalled)
            open_launcher
            last_launch="${FAKE_NOW:-$(date +%s)}"
            log "LG Home is in front: opened the launcher"
            touch "$MARK"                             # a check that is already running waits for this opening
            start_verify
            ;;
        $LAUNCHER_PATTERN)
            # The TV says the launcher is in front. Make sure it is really there (the TV can report
            # an app in front when its process has been closed, which is a black screen). Not right
            # after the watcher itself opened the launcher or asked for it again: that opening is
            # already being checked, and checking again could take the screen back from an app.
            was_home=0
            now="${FAKE_NOW:-$(date +%s)}"
            if [ $((now - last_launch)) -ge 15 ] && ! recently_fixed; then
                log "the launcher is in front: checking that it really showed itself"
                touch "$MARK"
                start_verify
            fi
            ;;
        *)
            was_home=0
            # LG Home is no longer in front: the request to see it is over (after a short grace
            # period, because the request is made just before LG Home arrives)
            if [ -f "$ALLOW" ]; then
                now="${FAKE_NOW:-$(date +%s)}"
                since=$(cat "$ALLOW" 2>/dev/null); since="${since:-0}"
                [ $((now - since)) -gt 10 ] && rm -f "$ALLOW"
            fi
            ;;
    esac
}

# Test modes: read reports from standard input (a line "TIME n" sets the clock), or run the check once.
if [ -n "$WATCH_STDIN" ]; then
    while IFS= read -r report; do
        case "$report" in
            "TIME "*) FAKE_NOW="${report#TIME }" ;;
            *) handle "$report" ;;
        esac
    done
    exit 0
fi
if [ -n "$WATCH_VERIFY_ONLY" ]; then
    verify
    exit 0
fi

# Only one watcher at a time.
if [ -f "$PIDFILE" ]; then
    other=$(cat "$PIDFILE" 2>/dev/null)
    if [ -n "$other" ] && [ "$other" != "$$" ] && kill -0 "$other" 2>/dev/null; then
        log "another watcher is already running (pid $other): exiting"
        exit 0
    fi
fi
echo $$ > "$PIDFILE"
rm -f "$VLOCK" "$VFIX"
# (a stop request is only noticed when the current command ends, so the launcher stops the watcher with kill -9 instead)
trap 'pkill -P $$ 2>/dev/null; [ "$(cat "$PIDFILE" 2>/dev/null)" = "$$" ] && rm -f "$PIDFILE"; exit 0' TERM INT
log "started (pid $$)"

quick_ends=0
while true; do
    started=$(date +%s)
    "$LUNA" -i "$FRONT_URI" '{"subscribe":true}' 2>/dev/null | while IFS= read -r report; do handle "$report"; done
    if [ $(( $(date +%s) - started )) -lt 5 ]; then quick_ends=$((quick_ends + 1)); else quick_ends=0; fi
    if [ "$quick_ends" -ge 2 ]; then
        log "the subscription keeps ending: asking once a second instead"
        while true; do
            handle "$("$LUNA" -n 1 "$FRONT_URI" '{}' 2>/dev/null)"
            sleep 1
        done
    fi
    sleep 1
done
