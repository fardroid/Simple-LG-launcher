#!/bin/sh
# Makes sure the watcher (watch.sh) is running, so that LG Home is replaced by the launcher.
# Safe to run again and again. Prints one word: applied, launcher-missing or files-missing.
# Run at boot by a link in /var/lib/webosbrew/init.d, and by the launcher whenever it opens.

APP_QML="${APP_QML:-/usr/palm/applications/com.webos.app.home/qml}"
HOOK="${HOOK:-/media/developer/launcher-homehook}"
LAUNCHER="${LAUNCHER:-/media/developer/apps/usr/palm/applications/tld.my.simplelauncher}"
PIDFILE="${PIDFILE:-/tmp/launcher-homehook-watch.pid}"
LOG="${LOG:-/tmp/launcher-homehook.log}"

log () { echo "$(date '+%H:%M:%S') $*" >> "$LOG"; }

# Earlier versions put a stand-in over LG Home's QML. Take it off if it is still there.
if [ -z "$DRY_RUN" ]; then
    while grep -q " $APP_QML " /proc/mounts; do
        umount "$APP_QML" || break
    done
fi

# The launcher is the TV's Home itself (tools/takeover.sh): the helper is not needed.
if [ -f "${TAKEOVER:-/media/developer/launcher-takeover/enabled}" ]; then
    log "the launcher is the TV's Home: the helper is not started"
    echo applied
    exit 0
fi

# The launcher must be installed (if it was uninstalled, LG Home is left alone).
if [ ! -f "$LAUNCHER/appinfo.json" ]; then
    log "launcher not installed: not starting the watcher"
    echo launcher-missing
    exit 2
fi

if [ ! -f "$HOOK/watch.sh" ]; then
    log "watch.sh missing: not starting the watcher"
    echo files-missing
    exit 4
fi

# The Home key helper (homekey.py): opens the launcher the moment the Home button goes down. Optional: without python3
# the watcher alone still does the job, a little later.
KEYPID="${KEYPIDFILE:-/tmp/launcher-homekey.pid}"
if [ -f "$HOOK/homekey.py" ]; then
    if ! command -v python3 >/dev/null 2>&1; then
        log "python3 not found: the Home key helper is not started"
    elif ! { [ -f "$KEYPID" ] && kill -0 "$(cat "$KEYPID")" 2>/dev/null; }; then
        if [ -z "$DRY_RUN" ]; then
            ( nohup python3 "$HOOK/homekey.py" >/dev/null 2>&1 </dev/null & )
        fi
        log "Home key helper started"
    fi
fi

# Already running?
if [ -f "$PIDFILE" ] && kill -0 "$(cat "$PIDFILE")" 2>/dev/null; then
    log "watcher already running (pid $(cat "$PIDFILE"))"
    echo applied
    exit 0
fi

if [ -z "$DRY_RUN" ]; then
    ( nohup sh "$HOOK/watch.sh" >/dev/null 2>&1 </dev/null & )
fi
log "watcher started"
echo applied
