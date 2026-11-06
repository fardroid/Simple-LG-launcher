#!/bin/sh
# The app's icon from assets/app/icon.svg (the Chrome on this Mac draws it): 400 px (LG's store size), and the package's
# icon.png 80 x 80 and largeIcon.png 130 x 130 (webOS: the two are the same picture in two sizes). Run: sh scripts/gen-app-icon.sh
set -e
cd "$(dirname "$0")/.."
T=$(mktemp -d)
printf '<html><body style="margin:0">%s</body></html>' "$(cat assets/app/icon.svg)" > "$T/icon.html"
"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" --headless=new --disable-gpu --hide-scrollbars --window-size=400,400 \
	--screenshot="$PWD/assets/app/icon-400.png" "file://$T/icon.html" >/dev/null 2>&1
sips -z 130 130 assets/app/icon-400.png --out assets/app/largeIcon.png >/dev/null
sips -z 80 80 assets/app/icon-400.png --out assets/app/icon.png >/dev/null
rm -r "$T"
echo "assets/app: icon-400.png largeIcon.png icon.png"
