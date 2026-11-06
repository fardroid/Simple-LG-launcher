#!/bin/sh
# Lets LG's own Home be shown: the watcher stands down until the user leaves LG Home again.
# The launcher runs this before opening LG Home from its "LG Home" tile.

APP_QML="${APP_QML:-/usr/palm/applications/com.webos.app.home/qml}"
ALLOW="${ALLOW:-/tmp/launcher-allow-stock-home}"

# (earlier versions put a stand-in over LG Home's QML: take it off if it is still there)
if [ -z "$DRY_RUN" ]; then
    while grep -q " $APP_QML " /proc/mounts; do
        umount "$APP_QML" || break
    done
fi
date +%s > "$ALLOW"
echo stock
