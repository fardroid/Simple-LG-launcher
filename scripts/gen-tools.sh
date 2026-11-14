#!/bin/sh
# The TV-side scripts of the ways of opening the launcher on Home (tools/ and tools/homehook), for this package:
#   sh scripts/gen-tools.sh <app id> <out dir>
# Under the launcher's own id they are copied as they are. Under another id the id is changed, and so are the folders, boot
# links and files the scripts keep their state in (launcher-… → launcher-native-…), so that two launchers installed side by
# side never read or undo each other's way.
# helper.sh copies the Home button helper (tools/homehook) to its folder on the TV.
set -e
ID=$1
OUT=$2
WEB_ID=tld.my.simplelauncher
mkdir -p "$OUT/homehook"
for f in homemode.sh defaulthome.sh webos26-access.sh takeover.sh helper.sh migrate.sh lastinput.sh homehook/watch.sh homehook/apply.sh homehook/stock.sh homehook/diagnose.sh homehook/homekey.py homehook/boot.sh; do
	src=tools/$f
	if [ "$ID" = "$WEB_ID" ]; then cp "$src" "$OUT/$f"; continue; fi
	# ([l]auncher-homehook: the pattern that finds the helper's processes without finding itself)
	sed -e "s/tld\.my\.simplelauncher\$/$ID/" \
		-e "s/tld\.my\.simplelauncher\([^.a-z]\)/$ID\1/g" \
		-e 's#/media/developer/launcher-#/media/developer/launcher-native-#g' \
		-e 's#init\.d/\([0-9][0-9]\)-launcher-#init.d/\1-launcher-native-#g' \
		-e 's#/tmp/launcher-#/tmp/launcher-native-#g' \
		-e 's#\[l\]auncher-homehook/#[l]auncher-native-homehook/#g' \
		"$src" > "$OUT/$f"
done
chmod +x "$OUT"/*.sh "$OUT"/homehook/*.sh "$OUT"/homehook/homekey.py
