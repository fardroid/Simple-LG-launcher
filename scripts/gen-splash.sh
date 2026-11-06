#!/bin/sh
# The packaged start-up picture (assets/app/splash-default.jpg, 1920 x 1080): the app's icon and name over the dusk theme's sky.
# The TV shows it while the launcher starts until the launcher writes its own (Settings → Start-up picture). The Chrome on this
# Mac draws it. Run: sh scripts/gen-splash.sh
set -e
cd "$(dirname "$0")/.."
T=$(mktemp -d)
cp assets/app/icon-400.png fonts/inter-400.ttf "$T/"
cat > "$T/splash.html" <<'HTML'
<html><head><style>
@font-face { font-family: Inter; src: url(inter-400.ttf); }
html, body { margin: 0; width: 1920px; height: 1080px; overflow: hidden; }
body { background: linear-gradient(160deg, #0b1636 0%, #26357a 62%, #a2506f 100%); display: flex; flex-direction: column;
	align-items: center; justify-content: center; font-family: Inter, sans-serif; }
img { width: 220px; height: 220px; border-radius: 50px; box-shadow: 0 24px 60px rgba(0, 0, 0, .35); }
div { margin-top: 44px; color: #F2F5F8; font-size: 54px; letter-spacing: .5px; }
</style></head><body><img src="icon-400.png"><div>Simple launcher</div></body></html>
HTML
"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" --headless=new --disable-gpu --hide-scrollbars --allow-file-access-from-files \
	--window-size=1920,1080 --screenshot="$T/splash.png" "file://$T/splash.html" >/dev/null 2>&1
sips -s format jpeg -s formatOptions 90 "$T/splash.png" --out assets/app/splash-default.jpg >/dev/null
rm -r "$T"
echo "assets/app/splash-default.jpg"
