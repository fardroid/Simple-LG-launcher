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
# keep_front (the 15 s after SAM is restarted): the native launcher opened by its id while SAM settles may not start, which
# leaves nothing in front (seen on webOS 10.3.2); then it is opened again, as when LG Home comes up.
sed -i.bak -e "s/^\(\t*\)\"\$LG_HOME\") luna-send -n 1 \"\$AM\/launch\"/\1''|\"\$LG_HOME\") luna-send -n 1 \"\$AM\/launch\"/" \
	-e "s/^\(\t*\)''|\"\$APP_ID\") ;;/\1\"\$APP_ID\") ;;/" "$OUT/defaulthome.sh"
rm -f "$OUT/defaulthome.sh.bak"
# SAM's restart: systemd waits up to 90 s for every process of SAM's group to end, and an app of LG's can ignore the request
# (AirPlay's LunaExecutable, seen on webOS 10.3.2, which left the TV with no app manager for those 90 s). Stopped without waiting,
# what is left after 4 s is ended the way systemd ends it after its 90 s, and SAM started again, with the services that stopped
# with it (the inputs' eim: without it Settings had no HDMI inputs, seen on webOS 10.3.2).
awk '
/^keepalive_apply \(\) \{/ {
	print "restart_sam () {"
	print "\t# the services running now: those that need SAM (eim, the inputs, and arccontroller after it) stop with it, and a start"
	print "\t# of SAM alone does not bring them back (a restart did); they are started again after it"
	print "\tbefore=$(systemctl list-units --type=service --state=active --no-legend 2>/dev/null | sed \"s/^[^a-zA-Z0-9]*//\" | cut -d\" \" -f1)"
	print "\tsystemctl stop --no-block sam || return 1"
	print "\ti=0"
	print "\twhile [ \"$i\" -lt 15 ]; do"
	print "\t\tcase \"$(systemctl is-active sam 2>/dev/null)\" in inactive|failed) break ;; esac"
	print "\t\tif [ \"$i\" -ge 4 ]; then"
	print "\t\t\tfor p in $(cat /sys/fs/cgroup/systemd/system.slice/sam.service/cgroup.procs 2>/dev/null); do"
	print "\t\t\t\tlog \"the app manager waits for $(tr \"\\0\" \" \" < /proc/$p/cmdline 2>/dev/null | cut -c1-80): ended\"; kill -9 \"$p\" 2>/dev/null"
	print "\t\t\tdone"
	print "\t\tfi"
	print "\t\tsleep 1; i=$((i + 1))"
	print "\tdone"
	print "\tsystemctl start sam"
	print "\tfor u in $before; do systemctl is-active --quiet \"$u\" || { systemctl start \"$u\" && log \"started $u again (it stopped with the app manager)\"; }; done"
	print "}"
	print ""
}
{ print }' "$OUT/defaulthome.sh" | sed -e 's/^\(\t*\)systemctl restart sam 2>\/dev\/null ||/\1restart_sam ||/' > "$OUT/defaulthome.sh.new"
mv -f "$OUT/defaulthome.sh.new" "$OUT/defaulthome.sh"
grep -q "restart_sam ||" "$OUT/defaulthome.sh" || { echo "gen-tools: systemctl restart sam not found in defaulthome.sh" >&2; exit 1; }
grep -q "''|\"\$LG_HOME\") luna-send" "$OUT/defaulthome.sh" || { echo "gen-tools: keep_front not found in defaulthome.sh" >&2; exit 1; }
chmod +x "$OUT"/*.sh "$OUT"/homehook/*.sh "$OUT"/homehook/homekey.py
