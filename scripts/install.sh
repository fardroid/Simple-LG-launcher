#!/bin/sh
# Installs build/*.ipk on the TV over SSH with the developer installer, closes the running copy and opens the new one.
# TV_HOST: from the environment, or from tv.env (git-ignored; copy tv.env.example).
set -e
cd "$(dirname "$0")/.."
[ -n "$TV_HOST" ] || { [ -f tv.env ] && . ./tv.env; }
[ -n "$TV_HOST" ] || { echo "TV_HOST is not set (e.g. TV_HOST=root@<tv-ip> make install)" >&2; exit 1; }
APP_ID=$(node -p "require('./build/app/appinfo.json').id")
# the package of this build's id and version only (packages of the other id can lie beside it)
IPK="build/$(node -p "const a=require('./build/app/appinfo.json'); a.id+'_'+a.version").ipk"
[ -f "$IPK" ] || IPK="$(ls build/"$APP_ID"_*_arm.ipk 2>/dev/null | head -1)"
[ -f "$IPK" ] || { echo "no package for $APP_ID in build/" >&2; exit 1; }
echo "installing $IPK"
REMOTE=/tmp/$(basename "$IPK")
scp -q -O "$IPK" "$TV_HOST:$REMOTE"
# luna-send prints only with a terminal (-tt)
ssh -tt "$TV_HOST" "luna-send -n 1 luna://com.webos.applicationManager/closeByAppId '{\"id\":\"$APP_ID\"}' >/dev/null; \
	luna-send -w 60000 -i luna://com.webos.appInstallService/dev/install '{\"id\":\"com.ares.defaultName\",\"ipkUrl\":\"$REMOTE\",\"subscribe\":true}' \
	| grep -oE '\"state\":\"installed\"|\"errorText\":\"[^\"]*\"' | head -1; rm -f '$REMOTE'; \
	luna-send -n 1 luna://com.webos.applicationManager/launch '{\"id\":\"$APP_ID\"}' >/dev/null" | tr -d '\r'
