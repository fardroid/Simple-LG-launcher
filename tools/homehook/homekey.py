#!/usr/bin/env python3
# Opens the launcher the moment the Home button goes down, and keeps LG Home from reaching the screen.
#
# The remote's keys can be read from /dev/input/event* (several readers may do it at once, so the TV keeps working
# as usual). The Home key is seen as soon as it is pressed, long before the TV has started LG Home (that takes
# 0.4 to 1 s). The launcher is asked for at once (a few tenths of a second, it is kept in memory), and a few quick
# requests to close LG Home follow, so that its start is cancelled before it comes to the front. If it does get through,
# watch.sh (which listens to the TV's "app in front" reports) still opens the launcher, as before.
#
# LG Home is also started without the Home key, above all when the TV is switched on (it wakes from standby and starts LG Home).
# The TV announces every start of an app (getAppLifeEvents): a "splash" event about a second before the app reaches the screen.
# The same happens on such an event for LG Home, so it is cancelled before it shows. Starts in the background (preloads) show no
# splash and are left alone.
#
# LG's own "start with Home" is switched off (homeAutoLaunch), so LG Home does not start at all when the TV comes back from standby;
# the TV shows its last input instead (the tuner: black without an aerial). The helper follows the TV's power state and opens the
# launcher as soon as the TV is back on, whatever was on screen before, except an HDMI input (a console that switched the TV on).
# Once just after a full start of the TV, LG Home or the tuner in front is replaced by the launcher.
#
# Started by apply.sh next to watch.sh; stopped with it. Needs python3 (the TV has it). Writes to /tmp/launcher-homehook.log.

import errno
import os
import re
import select
import struct
import subprocess
import threading
import time

LAUNCHER_ID = os.environ.get('LAUNCHER_ID', 'tld.my.simplelauncher')
LAUNCHER = os.environ.get('LAUNCHER', '/media/developer/apps/usr/palm/applications/' + LAUNCHER_ID)
PIDFILE = os.environ.get('KEYPIDFILE', '/tmp/launcher-homekey.pid')
LOG = os.environ.get('LOG', '/tmp/launcher-homehook.log')
ALLOW = os.environ.get('ALLOW', '/tmp/launcher-allow-stock-home')   # exists while the user asked for LG's own Home
TAKEOVER = os.environ.get('TAKEOVER', '/media/developer/launcher-takeover/enabled')   # the launcher is the TV's Home itself (tools/takeover.sh)
HOME_APP = 'com.webos.app.home'
HOME_CODES = (1198, 102)          # the Magic Remote's Home key, and the standard KEY_HOME (a keyboard)
EVENT = '@llHHi'                  # struct input_event: time (2 longs), type, code, value
EVSIZE = struct.calcsize(EVENT)
CLOSE_AT = (0.05, 0.10, 0.16, 0.24, 0.34, 0.46, 0.60, 0.78, 1.0, 1.3)   # seconds after the key: ask for LG Home to be closed
LUNA = 'luna://com.webos.applicationManager/'
LIFE_EVENTS = 'luna://com.webos.service.applicationmanager/getAppLifeEvents'
POWER = 'luna://com.webos.service.tvpower/power/getPowerState'
TAKE_OVER = (HOME_APP, 'com.webos.app.livetv')      # in front after the TV comes on: the launcher takes their place

last_press = [0.0]               # when the launcher was last asked for (the key and an LG Home start can both report one press)


def log(text):
    try:
        with open(LOG, 'a') as f:
            f.write('%s homekey: %s\n' % (time.strftime('%H:%M:%S'), text))
    except OSError:
        pass


def luna(method, payload):
    try:
        subprocess.Popen(['luna-send', '-n', '1', LUNA + method, payload], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except OSError:
        pass


def wanted():
    """True when the launcher should take the Home key: it is installed, and LG's own Home was not asked for."""
    if not os.path.isfile(LAUNCHER + '/appinfo.json') or os.path.exists(TAKEOVER):
        return False
    try:
        return time.time() - os.path.getmtime(ALLOW) > 60 if os.path.exists(ALLOW) else True
    except OSError:
        return True


def pressed(source):
    """Opens the launcher and closes LG Home a few times over the next second, so that its start is cancelled."""
    if not wanted():
        return
    now = time.time()
    if now - last_press[0] < 2.0:      # already done for this press
        return
    last_press[0] = now
    luna('launch', '{"id":"%s"}' % LAUNCHER_ID)
    log('Home pressed (%s): launcher asked for' % source)

    def close_home():
        last = 0.0
        for at in CLOSE_AT:
            time.sleep(at - last)
            last = at
            if os.path.exists(ALLOW):
                return
            luna('closeByAppId', '{"id":"%s"}' % HOME_APP)

    threading.Thread(target=close_home, daemon=True).start()


def home_starting(line):
    """True for the TV's announcement that LG Home is about to come to the screen."""
    if '"appId":"%s"' % HOME_APP not in line or 'preload' in line:
        return False
    return '"event":"splash"' in line or ('"event":"launch"' in line and '"reason":"boot"' in line)


def watch_starts():
    """Follows the TV's reports of apps starting, and takes over when LG Home is about to show."""
    while True:
        try:
            proc = subprocess.Popen(['luna-send', '-i', LIFE_EVENTS, '{"subscribe":true}'], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, universal_newlines=True)
            for line in proc.stdout:
                if home_starting(line):
                    pressed('LG Home starting')
            proc.wait()
        except OSError:
            pass
        time.sleep(2.0)


def foreground():
    """The app in front ('' when none or unknown)."""
    try:
        out = subprocess.run(['luna-send', '-n', '1', LUNA + 'getForegroundAppInfo', '{}'], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, universal_newlines=True, timeout=5).stdout
    except (OSError, subprocess.SubprocessError):
        return ''
    found = re.search(r'"appId"\s*:\s*"([^"]*)"', out)
    return found.group(1) if found else ''


def open_if_idle(why):
    """Opens the launcher when LG Home or the TV tuner is in front (not when nothing is: just after a wake-up the TV reports
    nothing in front for a moment)."""
    if not wanted():
        return False
    front = foreground()
    if front in TAKE_OVER:
        pressed('%s, %s in front' % (why, front))
        return True
    return False


def watch_power():
    """Follows the TV's power state and opens the launcher when the TV comes back on."""
    awake = True
    while True:
        try:
            proc = subprocess.Popen(['luna-send', '-i', POWER, '{"subscribe":true}'], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, universal_newlines=True)
            for line in proc.stdout:
                found = re.search(r'"state"\s*:\s*"([^"]*)"', line)
                if not found:
                    continue
                state = found.group(1)
                if state == 'Screen Off' or 'Screen Saver' in state:
                    continue                  # the picture off with the sound on (music): not a standby
                off = state != 'Active' or any(word in line for word in ('Power Off', 'Suspend', 'Standby', 'Reboot'))
                if off:
                    awake = False
                elif not awake:
                    awake = True
                    threading.Thread(target=woke_up, daemon=True).start()
            proc.wait()
        except OSError:
            pass
        time.sleep(2.0)


def woke_up():
    """The TV is back on: the launcher, unless an HDMI input is in front. Checked a few times, as apps come back one by one."""
    for wait in (0.2, 1.5, 3.0):
        time.sleep(wait)
        if not wanted():
            return
        front = foreground()
        if front == LAUNCHER_ID or front.startswith('com.webos.app.hdmi'):
            return
        last_press[0] = 0.0                    # a fresh request each time
        pressed('TV switched on, %s in front' % (front or 'nothing'))


def after_boot():
    """Just after a full start of the TV (the helper starts some 40 s in): the TV may be showing its tuner."""
    try:
        with open('/proc/uptime') as f:
            up = float(f.read().split()[0])
    except (OSError, ValueError):
        return
    if up < 300:
        time.sleep(2.0)
        open_if_idle('TV started')


def main():
    try:
        with open(PIDFILE, 'w') as f:
            f.write(str(os.getpid()))
    except OSError:
        pass
    log('started (pid %d)' % os.getpid())
    threading.Thread(target=watch_starts, daemon=True).start()
    threading.Thread(target=watch_power, daemon=True).start()
    threading.Thread(target=after_boot, daemon=True).start()
    fds = {}
    last_scan = 0.0
    while True:
        now = time.time()
        if now - last_scan > 3.0:               # remotes that reconnect appear as new devices
            last_scan = now
            try:
                names = sorted(n for n in os.listdir('/dev/input') if n.startswith('event'))
            except OSError:
                names = []
            for name in names:
                if name in fds.values():
                    continue
                try:
                    fds[os.open('/dev/input/' + name, os.O_RDONLY | os.O_NONBLOCK)] = name
                except OSError:
                    pass
        if not fds:
            time.sleep(1.0)
            continue
        ready, _, _ = select.select(list(fds), [], [], 1.0)
        for fd in ready:
            try:
                data = os.read(fd, EVSIZE * 64)
            except OSError as error:
                if error.errno not in (errno.EAGAIN, errno.EINTR):
                    os.close(fd)
                    del fds[fd]
                continue
            for i in range(0, len(data) - EVSIZE + 1, EVSIZE):
                _s, _u, kind, code, value = struct.unpack(EVENT, data[i:i + EVSIZE])
                if kind == 1 and value == 1 and code in HOME_CODES:
                    pressed(fds[fd])             # one press can show up on several devices: pressed() takes the first


if __name__ == '__main__':
    main()
