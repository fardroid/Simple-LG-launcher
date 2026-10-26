#!/bin/sh
# Installs the packaged .ipk on a rooted TV over SSH and launches it.
#
# Set TV_HOST (an ssh config alias, or root@<tv-ip>) in the environment, or in a
# git-ignored `tv.env` file at the project root (copy tv.env.example):
#
#   TV_HOST=root@192.0.2.10 sh scripts/install.sh
#
# Uses webOS's developer install service (the same one the Homebrew Channel and
# ares-install use). Nothing outside the app's own directory is touched.
# Without SSH: `npx ares-setup-device`, then `npx ares-install --device <name> build/*.ipk`.
set -e
cd "$(dirname "$0")/.."
# shellcheck disable=SC1091
[ -f tv.env ] && . ./tv.env
[ -n "$TV_HOST" ] || { echo "TV_HOST is not set. Copy tv.env.example to tv.env and edit it, or run: TV_HOST=root@<tv-ip> npm run install:tv" >&2; exit 1; }

APP_ID="$(sed -n 's/.*"id": *"\([^"]*\)".*/\1/p' public/appinfo.json | head -1)"
IPK="$(ls build/*.ipk 2>/dev/null | head -1)"
[ -n "$IPK" ] || { echo "No .ipk in build/ — run 'npm run package' first" >&2; exit 1; }

REMOTE_IPK="/tmp/$(basename "$IPK")"
echo "Copying $(basename "$IPK") to $TV_HOST..."
scp -q "$IPK" "$TV_HOST:$REMOTE_IPK"

echo "Installing $APP_ID..."
# luna-send needs a tty (-tt) to print anything; the subscription ends when the install does.
ssh -tt "$TV_HOST" "luna-send -w 60000 -i luna://com.webos.appInstallService/dev/install '{\"id\":\"com.ares.defaultName\",\"ipkUrl\":\"$REMOTE_IPK\",\"subscribe\":true}' | grep -oE '\"state\":\"[^\"]*\"|\"errorText\":\"[^\"]*\"' | uniq; rm -f '$REMOTE_IPK'" | tr -d '\r'

echo "Launching $APP_ID..."
ssh -tt "$TV_HOST" "luna-send -n 1 luna://com.webos.applicationManager/launch '{\"id\":\"$APP_ID\"}'" | tr -d '\r'
