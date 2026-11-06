#!/bin/sh
# The helper's boot link (the native version's: helper.sh copies it to the helper's folder as boot.sh): the helper is started.
# What a full start opens is the TV's last input, which the launcher names when it comes to the front (tools/lastinput.sh).
exec sh /media/developer/launcher-homehook/apply.sh >/dev/null 2>&1
