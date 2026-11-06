#!/bin/sh
# Collects what is needed to see why the launcher does not open when LG Home comes to the front.
# Run it on the TV over SSH and paste the output:   sh /media/developer/launcher-homehook/diagnose.sh
# (the same text is saved in /tmp/launcher-diagnose.txt)

HOOK=/media/developer/launcher-homehook
APP_QML=/usr/palm/applications/com.webos.app.home/qml
PIDFILE=/tmp/launcher-homehook-watch.pid

{
echo "== time";                       date
echo "== files";                      ls -l "$HOOK" 2>&1
echo "== version";                    cat "$HOOK/version" 2>&1
echo "== startup links";              ls -l /var/lib/webosbrew/init.d/ 2>&1
echo "== watcher"
if [ -f "$PIDFILE" ] && kill -0 "$(cat "$PIDFILE")" 2>/dev/null; then echo "running, pid $(cat "$PIDFILE")"; else echo "NOT running (pid file: $(cat "$PIDFILE" 2>&1))"; fi
echo "== Home key helper (homekey.py)"
KEYPID=/tmp/launcher-homekey.pid
if [ -f "$KEYPID" ] && kill -0 "$(cat "$KEYPID")" 2>/dev/null; then echo "running, pid $(cat "$KEYPID")"; else echo "NOT running"; fi
which python3 2>&1
echo "-- every watch.sh / luna-send subscription process (more than one watch.sh is a problem):"
ps 2>&1 | grep -E "[w]atch.sh|[g]etForegroundAppInfo"
echo "== do the scripts parse on this TV's shell?"
for f in watch.sh apply.sh stock.sh; do if sh -n "$HOOK/$f" 2>&1; then echo "$f: syntax OK"; else echo "$f: SYNTAX PROBLEM"; fi; done
echo "== does the watcher's decision logic work on this TV's shell? (should say WOULD OPEN, once)"
printf '#!/bin/sh\necho "WOULD OPEN: $*"\n' > /tmp/launcher-selftest-luna.sh; chmod +x /tmp/launcher-selftest-luna.sh
sed 's#>/dev/null 2>&1$##' "$HOOK/watch.sh" > /tmp/launcher-selftest-watch.sh
echo '{"returnValue":true,"appId":"com.webos.app.home","processId":"","windowId":""}' | NO_VERIFY=1 WATCH_STDIN=1 LUNA_SEND=/tmp/launcher-selftest-luna.sh LOG=/dev/null ALLOW=/tmp/launcher-selftest-allow sh /tmp/launcher-selftest-watch.sh 2>&1 || echo "(self-test failed)"
rm -f /tmp/launcher-selftest-luna.sh /tmp/launcher-selftest-watch.sh
echo "== has the launcher shown itself recently? (the launcher touches this file when it is on screen)"; ls -l /tmp/launcher-alive /tmp/launcher-launch-mark 2>&1
echo "== is a launcher process running? (pgrep -f)"; pgrep -f tld.my.simplelauncher 2>&1 | head -5 || echo "none found"
echo "== files in /tmp";                ls -l /tmp/launcher-* 2>&1
echo "== stand-in still mounted over LG Home's QML? (should be 'no')"; grep " $APP_QML " /proc/mounts || echo "no"
echo "== user asked for LG's own Home? (file exists = yes)"; ls -l /tmp/launcher-allow-stock-home 2>&1
echo "== LG Home process(es)";        ps 2>&1 | grep -i "[c]om.webos.app.home"
echo "== launcher installed?";        ls /media/developer/apps/usr/palm/applications/tld.my.simplelauncher 2>&1 | head -5
echo "== app in front right now";     luna-send -n 1 luna://com.webos.applicationManager/getForegroundAppInfo '{}' 2>&1
echo "== log (/tmp/launcher-homehook.log)";  tail -n 60 /tmp/launcher-homehook.log 2>&1
echo "== end"
} 2>&1 | tee /tmp/launcher-diagnose.txt
