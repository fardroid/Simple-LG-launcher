#!/bin/sh
# Tests for src/homehook/*.sh using fake folders, a fake luna-send and a fake clock.   Run: npm test
S="$(cd "$(dirname "$0")/../src/homehook" && pwd)"
T="$(mktemp -d)"; fail=0
check () { if [ "$2" = "$3" ]; then echo "ok   $1"; else echo "FAIL $1 (got '$2', wanted '$3')"; fail=1; fi; }
mkdir -p "$T/hook" "$T/launcher"
echo '{}' > "$T/launcher/appinfo.json"
echo '#!/bin/sh' > "$T/hook/watch.sh"

# ---- apply.sh ----
runapply () { DRY_RUN=1 APP_QML="$T/qml" HOOK="$T/hook" LAUNCHER="$T/launcher" PIDFILE="$T/pid" LOG="$T/log" sh "$S/apply.sh"; }
check "apply starts the watcher when everything is in place" "$(runapply)" "applied"
mv "$T/launcher/appinfo.json" "$T/launcher/x"
check "apply refuses when the launcher is not installed" "$(runapply)" "launcher-missing"
mv "$T/launcher/x" "$T/launcher/appinfo.json"
mv "$T/hook/watch.sh" "$T/hook/x"
check "apply refuses when watch.sh is missing" "$(runapply)" "files-missing"
mv "$T/hook/x" "$T/hook/watch.sh"
echo "$$" > "$T/pid"
check "apply recognises a watcher that is already running" "$(runapply)" "applied"
check "and says so in the log" "$(grep -c 'already running' "$T/log")" "1"
rm -f "$T/pid"
check "stock.sh reports itself done" "$(DRY_RUN=1 ALLOW="$T/allow" sh "$S/stock.sh")" "stock"
check "stock.sh asks the watcher to stand down" "$([ -f "$T/allow" ] && echo yes)" "yes"
rm -f "$T/allow"
for f in watch.sh apply.sh stock.sh diagnose.sh; do check "$f is valid shell" "$(sh -n "$S/$f" && echo ok)" "ok"; done

# ---- watch.sh: when does it open the launcher? ----
cat > "$T/luna.sh" <<'LUNA'
#!/bin/sh
case "$*" in
  *getForegroundAppInfo*) [ -f "$FRONT_FILE" ] && cat "$FRONT_FILE" ;;
  *launch*) printf '%s\n' "$*" >> "$LAUNCH_LOG" ;;
esac
LUNA
chmod +x "$T/luna.sh"
rm -f "$T/launches" "$T/allow"
watch () { NO_VERIFY=1 WATCH_STDIN=1 LUNA_SEND="$T/luna.sh" LAUNCH_LOG="$T/launches" ALLOW="$T/allow" MARK="$T/mark" VLOCK="$T/vlock" VFIX="$T/vfix" LAUNCHER="$T/launcher" LOG="$T/log" sh "$S/watch.sh"; }
launches () { if [ -f "$T/launches" ]; then wc -l < "$T/launches" | tr -d ' '; else echo 0; fi; }
fresh () { rm -f "$T/launches"; }
HOME_REPORT='{"returnValue":true,"appId":"com.webos.app.home","processId":"","windowId":""}'
APP_REPORT='{"returnValue":true,"appId":"youtube.leanback.v4","processId":"123","windowId":"4"}'
LAUNCHER_REPORT='{"returnValue":true,"appId":"tld.my.simplelauncher","processId":"","windowId":""}'
SUB_HOME='{"returnValue":true,"subscribed":true,"appId":"com.webos.app.home","processId":"","windowId":""}'

fresh; printf '%s\n' "TIME 1000" "$APP_REPORT" | watch
check "another app in front: nothing happens" "$(launches)" "0"
fresh; printf '%s\n' "TIME 1000" '{"returnValue":true,"subscribed":true}' | watch
check "a report without an app is ignored" "$(launches)" "0"
fresh; printf '%s\n' "TIME 1000" "$HOME_REPORT" | watch
check "LG Home in front: the launcher is opened" "$(launches)" "1"
check "and it is the launcher that is opened" "$(grep -c 'tld.my.simplelauncher' "$T/launches")" "1"
check "and the log says so, after the launch" "$(grep -c 'opened the launcher' "$T/log")" "1"
fresh; printf '%s\n' "TIME 1000" "$HOME_REPORT" "$HOME_REPORT" "$HOME_REPORT" | watch
check "LG Home reported again at once: not opened again" "$(launches)" "1"
fresh; printf '%s\n' "TIME 1000" "$HOME_REPORT" "TIME 1010" "$HOME_REPORT" | watch
check "LG Home still reported 10 s later (the first hand-over was lost): opened again" "$(launches)" "2"
fresh; printf '%s\n' "TIME 1000" "$HOME_REPORT" "$APP_REPORT" "$HOME_REPORT" | watch
check "LG Home, another app, LG Home again: opened both times" "$(launches)" "2"
fresh; printf '%s\n' "TIME 1000" "$SUB_HOME" | watch
check "a subscription report with extra fields works too" "$(launches)" "1"
fresh; printf '%s\n' "TIME 1000" "$LAUNCHER_REPORT" | watch
check "the launcher reported in front: nothing is opened by that alone" "$(launches)" "0"

# the checks must not start each other (a loop of "asking again" took the screen back from apps being opened)
checks () { grep -c 'checking that it really showed itself' "$T/log"; }
: > "$T/log"; rm -f "$T/vfix" "$T/vlock"
printf '%s\n' "TIME 1000" "$HOME_REPORT" "TIME 1004" "$LAUNCHER_REPORT" | watch
check "the launcher reported in front just after the watcher opened it: no second check" "$(checks)" "0"
printf '%s\n' "TIME 1100" "$LAUNCHER_REPORT" | watch
check "the launcher reported in front on its own (e.g. a black screen at boot): it is checked" "$(checks)" "1"
echo 1200 > "$T/vfix"
printf '%s\n' "TIME 1205" "$LAUNCHER_REPORT" | watch
check "just after a check had to ask again: not checked again (no loop)" "$(checks)" "1"
printf '%s\n' "TIME 1300" "$LAUNCHER_REPORT" | watch
check "a while after that: checked again" "$(checks)" "2"
rm -f "$T/vfix"
# one check at a time (here the checks run for real, with the TV reporting nothing in front)
slowwatch () { WATCH_STDIN=1 VERIFY_WAIT=0 FRONT_FILE="$T/nofront" LUNA_SEND="$T/luna.sh" LAUNCH_LOG="$T/launches" ALLOW="$T/allow" MARK="$T/mark" ALIVE="$T/alive" VLOCK="$T/vlock" VFIX="$T/vfix" LAUNCHER="$T/launcher" LOG="$T/log" sh "$S/watch.sh"; }
echo 2000 > "$T/vlock"
printf '%s\n' "TIME 2010" "$LAUNCHER_REPORT" | slowwatch; sleep 1
check "a check started 10 s ago is still running: no second one" "$(cat "$T/vlock" 2>/dev/null)" "2000"
printf '%s\n' "TIME 2100" "$LAUNCHER_REPORT" | slowwatch; sleep 1
check "a lock left over from long ago is ignored" "$([ "$(cat "$T/vlock" 2>/dev/null)" != 2000 ] && echo replaced)" "replaced"
rm -f "$T/vlock"

# the user asked for LG's own Home (the LG Home tile)
fresh; echo 2000 > "$T/allow"
printf '%s\n' "TIME 2002" "$HOME_REPORT" | watch
check "while LG's own Home is wanted, the watcher stands down" "$(launches)" "0"
printf '%s\n' "TIME 2003" "$APP_REPORT" | watch
check "the request is kept for a few seconds after (the launcher is still in front)" "$([ -f "$T/allow" ] && echo kept)" "kept"
printf '%s\n' "TIME 2030" "$APP_REPORT" | watch
check "once LG Home has been left for a while, the request ends" "$([ -f "$T/allow" ] && echo kept || echo ended)" "ended"
printf '%s\n' "TIME 2040" "$HOME_REPORT" | watch
check "and the next time LG Home comes to the front it is taken over again" "$(launches)" "1"

# nothing to open
fresh; mv "$T/launcher/appinfo.json" "$T/launcher/x"
printf '%s\n' "TIME 3000" "$HOME_REPORT" | watch
check "launcher uninstalled: nothing is opened (LG Home stays)" "$(launches)" "0"
mv "$T/launcher/x" "$T/launcher/appinfo.json"

# ---- watch.sh: did the launcher really show itself? ----
verify () { VFIX="$T/vfix" VERIFY_WAIT=0 WATCH_VERIFY_ONLY=1 LUNA_SEND="$T/luna.sh" LAUNCH_LOG="$T/launches" FRONT_FILE="$T/front" ALLOW="$T/allow" ALIVE="$T/alive" MARK="$T/mark" LAUNCHER="$T/launcher" LOG="$T/log" sh "$S/watch.sh"; }
rm -f "$T/allow"
fresh; echo "$HOME_REPORT" > "$T/front"; touch -t 203012312359 "$T/alive"
verify
check "the launcher showed itself: no second attempt" "$(launches)" "0"
fresh; touch -t 200001010000 "$T/alive"
verify
check "LG Home still in front and the launcher never showed: asked again, three times, then it gives up" "$(launches)" "3"
fresh; echo "$LAUNCHER_REPORT" > "$T/front"
verify
check "the TV says the launcher is in front but it never showed (a black screen): asked again" "$(launches)" "3"
fresh; echo "$APP_REPORT" > "$T/front"
verify
check "another app is in front: left alone" "$(launches)" "0"
fresh; echo "$HOME_REPORT" > "$T/front"; echo 1 > "$T/allow"
verify
check "LG's own Home was asked for: left alone" "$(launches)" "0"
rm -f "$T/allow"

# ---- a second watcher refuses to start ----
cat > "$T/quiet.sh" <<'Q'
#!/bin/sh
exit 0
Q
chmod +x "$T/quiet.sh"
echo "$$" > "$T/pid"
timeout 10 env LUNA_SEND="$T/quiet.sh" PIDFILE="$T/pid" LOG="$T/log" LAUNCHER="$T/launcher" ALLOW="$T/allow" sh "$S/watch.sh"
check "a second watcher exits at once when one is already running" "$(grep -c 'another watcher is already running' "$T/log")" "1"
check "and leaves the first one's pid file alone" "$(cat "$T/pid")" "$$"

rm -rf "$T"
exit $fail
