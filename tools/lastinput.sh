#!/bin/sh
# The TV's "last input" for the Swap and the Helper (the native version's own). At a full start the TV opens its last input
# (the general settings lastInputApp and physicalLastInputApp), whatever its "start with Home" says; the official way names the
# launcher there for good (defaulthome.sh). These two ways name Home (com.webos.app.home: the launcher with the Swap, LG Home
# that the helper replaces with the Helper) when the launcher comes to the front, as the TV itself names an input when it is
# opened: a full start opens the launcher when it was the last thing on the screen, and an input when that was. (Not the
# launcher's own id: started that way the native launcher never ran.) What was there before is kept and put back when the way
# is switched off.
#
#   sh lastinput.sh name <app id>   the launcher (as <app id>) named the last input, if it is not already
#   sh lastinput.sh restore         what was there before put back, if the launcher is still named
#
# This file is the native version's own (scripts/gen-tools.sh copies it with the id and folders of the build).

ORIG=/media/developer/launcher-lastinput.json
APP_ID=tld.my.simplelauncher
HOME_ID=com.webos.app.home
SETTINGS=luna://com.webos.settingsservice
LOG=/tmp/launcher-homehook.log

log () { echo "$(date '+%H:%M:%S') lastinput: $*" >> "$LOG"; }
now () { luna-send -n 1 "$SETTINGS/getSystemSettings" '{"category":"general","keys":["lastInputApp","physicalLastInputApp"]}' 2>/dev/null; }
setting () { echo "$2" | sed -n "s/.*\"$1\"[[:space:]]*:[[:space:]]*\"\([^\"]*\)\".*/\1/p"; }
set_both () {
	luna-send -n 1 "$SETTINGS/setSystemSettings" "{\"category\":\"general\",\"settings\":{\"lastInputApp\":\"$1\",\"physicalLastInputApp\":\"$2\"}}" 2>/dev/null | grep -q '"returnValue":true'
}
ours () { [ "$1" = "$APP_ID" ] || [ "$1" = "$HOME_ID" ]; }

case "$1" in
	name)
		[ -n "$2" ] || exit 1
		n=$(now)
		last=$(setting lastInputApp "$n"); phys=$(setting physicalLastInputApp "$n")
		[ "$last" = "$2" ] && [ "$phys" = "$2" ] && exit 0
		# the TV's own value, once (not one of ours)
		ours "$last" || printf '{"lastInputApp":"%s","physicalLastInputApp":"%s"}\n' "$last" "$phys" > "$ORIG"
		set_both "$2" "$2" && log "the launcher is the last input ($2; was $last)"
		;;
	restore)
		[ -s "$ORIG" ] || exit 0
		n=$(now)
		o=$(cat "$ORIG")
		if ours "$(setting lastInputApp "$n")" && [ -n "$(setting lastInputApp "$o")" ]; then
			set_both "$(setting lastInputApp "$o")" "$(setting physicalLastInputApp "$o")" && log "the last input put back ($(setting lastInputApp "$o"))"
		fi
		rm -f "$ORIG"
		;;
	*) echo "usage: sh $0 name <app id> | restore"; exit 1 ;;
esac
