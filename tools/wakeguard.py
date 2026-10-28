#!/usr/bin/env python3
# The wake-up guard of the official way (tools/defaulthome.sh). With the launcher chosen as the TV's Home app, the Home key and
# Back out of the last app open it, but waking the TV from standby does not: the TV's start-up (bootd, with LG's "start with
# Home" on) launches LG Home by name, with {"boot": true}, whatever app is Home. LG Home then reopens the app that was in front when
# the TV was switched off (YouTube, Netflix…), which is what people expect, so that is left alone. But when that app was the
# launcher (or LG Home itself), LG Home stays on the screen, and the Home key does nothing while it is there. Then this opens the
# launcher.
#
# How it knows the TV woke up: in standby the TV is suspended, so the wall clock jumps ahead of the monotonic clock (which stops
# while suspended). On a TV that stays awake in standby it follows the power state instead ("Active" again after anything else),
# asked only for a minute after the TV wrote down the app in front (it does on every change of app, and when it is switched off).
# Which app was in front before the TV slept is read from that same file, every few seconds while awake (a file read, no call):
# after waking the file already names LG Home.
#
# Started by defaulthome.sh (enable, and at every full start through its boot link); ends by itself when the official way is off.
# Writes to /tmp/launcher-homehook.log. Needs python3 (the TV has it).

import json
import os
import signal
import subprocess
import sys
import time

LAUNCHER_ID = os.environ.get('LAUNCHER_ID', 'tld.my.simplelauncher')
LG_HOME = 'com.webos.app.home'
FLAG = os.environ.get('FLAG', '/media/developer/launcher-defaulthome/enabled')
LAST = os.environ.get('LAST', '/var/luna/preferences/last_foreground_app_id.json')
PIDFILE = os.environ.get('PIDFILE', '/tmp/launcher-wakeguard.pid')
LOG = os.environ.get('LOG', '/tmp/launcher-homehook.log')
LUNA = os.environ.get('LUNA_SEND', 'luna-send')
AM = 'luna://com.webos.service.applicationmanager'

TICK = 2              # seconds between looks
SUSPEND_GAP = 4       # the wall clock ran this much ahead: the TV was asleep
WATCH_POWER = 60      # after the last app was written down, follow the power state this long
AFTER_WAKE = 25       # after waking, look for LG Home in front this long


def log(text):
    try:
        with open(LOG, 'a') as f:
            f.write(time.strftime('%H:%M:%S') + ' wakeguard: ' + text + '\n')
    except OSError:
        pass


def luna(uri, payload='{}'):
    try:
        out = subprocess.run([LUNA, '-n', '1', uri, payload], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=10).stdout
        return json.loads(out.decode('utf-8', 'replace') or '{}')
    except (OSError, ValueError, subprocess.SubprocessError):
        return {}


def front():
    return luna(AM + '/getForegroundAppInfo').get('appId', '')


def powered():
    return luna('luna://com.webos.service.tvpower/power/getPowerState').get('state', '')


def last_app():
    try:
        with open(LAST) as f:
            return json.load(f).get('appId', '')
    except (OSError, ValueError):
        return ''


def mtime():
    try:
        return os.stat(LAST).st_mtime
    except OSError:
        return 0


def open_launcher(why):
    """Home, the way the Home key opens it (the launcher is the Home app); checked, and asked again up to three times."""
    for attempt in range(3):
        luna(AM + '/launchDefaultApp', '{"category":"home"}')
        for _ in range(4):
            time.sleep(1)
            app = front()
            if app == LAUNCHER_ID:
                log('%s: the launcher is open%s' % (why, '' if attempt == 0 else ' (try %d)' % (attempt + 1)))
                return
            if app and app != LG_HOME:
                log('%s: %s came to the front, left alone' % (why, app))
                return
    log('%s: the launcher did not open' % why)


def after_wake(why, last):
    """Just after waking: the launcher instead of LG Home, when the TV was switched off on the launcher (`last`: the app that was
    in front before the TV slept)."""
    if last not in (LAUNCHER_ID, LG_HOME, ''):
        log('%s: switched off on %s, LG Home reopens it' % (why, last))
        return
    end = time.monotonic() + AFTER_WAKE
    while time.monotonic() < end:
        app = front()
        if app == LG_HOME:
            open_launcher(why)
            return
        if app and app != LAUNCHER_ID:
            log('%s: %s is in front, left alone' % (why, app))
            return
        time.sleep(1)


def running_already():
    try:
        with open(PIDFILE) as f:
            pid = int(f.read().strip() or 0)
        if pid and pid != os.getpid():
            os.kill(pid, 0)
            return True
    except (OSError, ValueError):
        pass
    return False


def main():
    if running_already():
        return
    # outlive whatever started it (an ssh session, the launcher's shell): no hang-up, a session of its own
    signal.signal(signal.SIGHUP, signal.SIG_IGN)
    try:
        os.setsid()
    except OSError:
        pass
    with open(PIDFILE, 'w') as f:
        f.write(str(os.getpid()))
    log('started (pid %d)' % os.getpid())
    # The app in front, as the TV keeps writing it down (on every change of app, not only when it is switched off), LG Home
    # left out: after waking it is LG Home that is in front, and the app before it is the one to go back to.
    before = last_app()
    # a full start of the TV shows LG Home too
    if len(sys.argv) > 1 and sys.argv[1] == 'boot':
        after_wake('start-up', before)
    wall, mono = time.time(), time.monotonic()
    seen = mtime()
    watch_until = 0.0
    state = powered()
    while os.path.exists(FLAG):
        time.sleep(TICK)
        now_wall, now_mono = time.time(), time.monotonic()
        slept = (now_wall - wall) - (now_mono - mono)
        wall, mono = now_wall, now_mono
        changed = mtime()
        if changed != seen:
            seen = changed
            watch_until = now_mono + WATCH_POWER
        if slept > SUSPEND_GAP:
            watch_until = 0.0
            after_wake('woke up after %d s asleep' % slept, before)
            state = 'Active'
            continue
        if now_mono < watch_until:
            was, state = state, powered()
            if state == 'Active' and was and was != 'Active':
                watch_until = 0.0
                after_wake('woke up (%s before)' % was, before)
                continue
        app = last_app()
        if app and app != LG_HOME:
            before = app
    log('the official way is off: stopped')
    try:
        os.remove(PIDFILE)
    except OSError:
        pass


if __name__ == '__main__':
    main()
