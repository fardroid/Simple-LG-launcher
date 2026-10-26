#!/bin/sh
# Finds which TV setting changes when you flip a switch, e.g. LG's hidden "LG logo" option on the No Signal menu.
# Run on the TV as root over SSH:      sh /tmp/watch-tv-settings.sh
# Then flip the setting on the TV (a few times, off and on) and press Ctrl+C when done.
# Everything is also saved in /tmp/tv-settings-watch.log -- copy that file back (scp root@TV_IP:/tmp/tv-settings-watch.log .)
# It only READS settings and file dates. It changes nothing.

LUNA=${LUNA_SEND:-luna-send}
LOG=${LOG:-/tmp/tv-settings-watch.log}
WORK=${WORK:-/tmp/tv-settings-watch}
EVERY=${EVERY:-2}
CATS="general option picture sound network time lock hotelMode commercial broadcast channel input"
DIRS=${DIRS:-"/var /mnt/lg /home/root /media/developer/temp"}
mkdir -p "$WORK"; : > "$LOG"

say() { echo "$*" | tee -a "$LOG"; }

# One setting category as one value per line, so a change shows up as exactly the line that changed.
snap_cat() {
	"$LUNA" -n 1 -f luna://com.webos.settingsservice/getSystemSettings "{\"category\":\"$1\"}" 2>&1 | tr ',{}' '\n\n\n' | sed '/^$/d'
}

say "== Watching TV settings. Started: $(date)"
say "== TV: $(cat /var/run/nyx/os_info.json 2>/dev/null | tr -d '\n' | head -c 400)"
WORKING=""
for c in $CATS; do
	snap_cat "$c" > "$WORK/$c.old"
	if grep -q '"returnValue":false' "$WORK/$c.old" || [ ! -s "$WORK/$c.old" ]; then
		say "   category $c: not readable ($(head -c 120 "$WORK/$c.old" | tr '\n' ' '))"
	else
		say "   category $c: ok ($(wc -l < "$WORK/$c.old") values)"
		WORKING="$WORKING $c"
	fi
done
touch "$WORK/stamp"
say "== Now flip the setting on the TV. Changes are printed below as they happen. Ctrl+C to stop."

while true; do
	sleep "$EVERY"
	NOW=$(date '+%H:%M:%S')
	for c in $WORKING; do
		snap_cat "$c" > "$WORK/$c.new"
		if ! cmp -s "$WORK/$c.old" "$WORK/$c.new"; then
			say "[$NOW] SETTING CHANGED in category '$c':"
			grep -vxFf "$WORK/$c.old" "$WORK/$c.new" | sed 's/^/      now:    /' | tee -a "$LOG"
			grep -vxFf "$WORK/$c.new" "$WORK/$c.old" | sed 's/^/      before: /' | tee -a "$LOG"
		fi
		mv "$WORK/$c.new" "$WORK/$c.old"
	done
	# Files that were written since the last look (settings are often stored in a file or database).
	CH=$(for d in $DIRS; do [ -d "$d" ] && find "$d" -xdev -type f -newer "$WORK/stamp" 2>/dev/null; done | grep -v -e '/tmp-' -e '\.log$' -e 'tv-settings-watch' | head -40)
	touch "$WORK/stamp"
	if [ -n "$CH" ]; then say "[$NOW] files written:"; echo "$CH" | sed 's/^/      /' | tee -a "$LOG"; fi
done
