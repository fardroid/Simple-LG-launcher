#!/bin/sh
# Tests for tools/wakeguard.py (the official way's wake-up guard) with a fake luna-send.   Run: npm test
# Only what it does just after waking is tested here (its "boot" start); how it notices the wake-up needs a real standby.
G="$(cd "$(dirname "$0")/../tools" && pwd)/wakeguard.py"
T="$(mktemp -d)"; fail=0
check () { if [ "$2" = "$3" ]; then echo "ok   $1"; else echo "FAIL $1 (got '$2', wanted '$3')"; fail=1; fi; }
command -v python3 >/dev/null 2>&1 || { echo "skipped: no python3"; exit 0; }

# The fake luna-send: the app in front is in $T/front; launchDefaultApp puts the launcher there (or does nothing with $T/stuck).
cat > "$T/luna-send" <<EOF
#!/bin/sh
uri=\$3
echo "\$uri" >> "$T/calls"
case "\$uri" in
	*getForegroundAppInfo) echo "{\"returnValue\":true,\"appId\":\"\$(cat "$T/front")\"}" ;;
	*launchDefaultApp) [ -f "$T/stuck" ] || echo tld.my.simplelauncher > "$T/front"; echo '{"returnValue":true}' ;;
	*getPowerState) echo '{"returnValue":true,"state":"Active"}' ;;
	*) echo '{"returnValue":true}' ;;
esac
EOF
chmod +x "$T/luna-send"

run () {    # app in front -> what the log says
	echo "$1" > "$T/front"; rm -f "$T/log" "$T/calls" "$T/pid"
	LUNA_SEND="$T/luna-send" LAST="$T/last" LOG="$T/log" PIDFILE="$T/pid" FLAG="$T/no-flag" python3 "$G" boot
	sed -n 's/^[0-9:]* wakeguard: //p' "$T/log" | grep -v '^started\|^the official way is off' | head -n 1
}

check "LG Home in front after waking: the launcher is opened" "$(run com.webos.app.home)" "start-up: the launcher is open"
check "the launcher already in front: nothing to do" "$(run tld.my.simplelauncher)" ""
check "and Home is not asked for" "$(cat "$T/calls" 2>/dev/null | grep -c launchDefaultApp)" "0"
check "another app in front (a console on HDMI, an app button on the remote): left alone" "$(run com.webos.app.hdmi2)" "start-up: com.webos.app.hdmi2 is in front, left alone"
touch "$T/stuck"
check "the launcher does not come: asked three times, then said so" "$(run com.webos.app.home)" "start-up: the launcher did not open"
check "three asks" "$(grep -c launchDefaultApp "$T/calls")" "3"
rm -f "$T/stuck"
echo $$ > "$T/pid"
check "a second copy does not start while one runs" "$(echo com.webos.app.home > "$T/front"; rm -f "$T/log"; LUNA_SEND="$T/luna-send" LAST="$T/last" LOG="$T/log" PIDFILE="$T/pid" FLAG="$T/no-flag" python3 "$G" boot; [ -f "$T/log" ] && echo ran || echo quiet)" "quiet"

rm -rf "$T"
[ "$fail" = 0 ] && echo "
all passed" || echo "
FAILED"
exit $fail
