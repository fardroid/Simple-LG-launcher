# Open the launcher instead of LG Home (experimental)

Source of the "Open this launcher instead of LG Home" setting. The launcher writes these files to the TV itself
(`/media/developer/launcher-homehook`); you never need to copy them by hand.

**Nothing is replaced, mounted or modified.** A small shell script (`watch.sh`) runs in the background and watches which app the TV
reports as being in front. When that becomes LG Home (Home button, leaving an app with Back, boot), it asks the TV to open the launcher.

- `watch.sh`: the watcher. Uses the TV's "app in front" subscription; if that keeps ending at once, it asks once a second instead.
  After opening the launcher it **checks that the launcher really showed itself** (the launcher touches `/tmp/launcher-alive` whenever it is on screen)
  and asks again, up to three times, if not. It also does this when the TV reports the launcher in front, because the TV can report an app in front
  after its process has been closed, which is a black screen.
- `apply.sh`: starts the watcher if it isn't running (and takes off a stand-in that version 1.5 mounted over LG Home, if one is left).
- `stock.sh`: "I want LG's own Home now": the watcher stands down until LG Home has been left again. The launcher's "LG Home" tile runs it.
- `diagnose.sh`: collects what is needed to see why something isn't working.

## The two switches (Settings → Options)

- **Open this launcher instead of LG Home**: installs the scripts and starts the watcher. Off: stops it and deletes everything.
- **Start that helper at boot**: a link in `/var/lib/webosbrew/init.d` (`48-launcher-homehook`). You can switch this off on its own:
  the helper then starts when the launcher opens instead of at boot.

## Reversing it

Any one of these works:

1. In the launcher: **Settings → Options → Open this launcher instead of LG Home → OK** (stops it and deletes everything it added).
   To remove only the start-at-boot script, switch **Start that helper at boot** off.
2. Over SSH:
   ```
   rm -f /var/lib/webosbrew/init.d/48-launcher-homehook          # the start-at-boot script
   kill "$(cat /tmp/launcher-homehook-watch.pid)"                # stop the helper now
   rm -rf /media/developer/launcher-homehook                     # delete its files
   ```
3. A reboot stops the helper. If the boot link is still there it starts again a few seconds after boot, so also remove the link (above).

**Before uninstalling the launcher, turn the setting off.** (If you forget, nothing breaks: the helper checks the launcher is installed
before opening it, and otherwise leaves LG Home alone.)

## Seeing what is happening, and sharing the logs

Do this **while the problem is happening** (for example with the black screen showing; SSH still works then):

1. Open an SSH session to the TV (`ssh root@TV_IP`, or your SSH app).
2. Run:
   ```
   sh /media/developer/launcher-homehook/diagnose.sh
   ```
   It prints everything needed: the files, whether the helper is running (and how many copies), whether the scripts parse on the TV's shell,
   a self-test of the helper's decision on the TV's own shell, the app in front, and the helper's log.
3. Share it: **select all the output and copy it into your message**, or take screenshots of it, or copy the saved file to your PC:
   ```
   scp root@TV_IP:/tmp/launcher-diagnose.txt .
   ```
   and attach `launcher-diagnose.txt`.

To watch the helper live while you open an app and press Back: in one SSH window run `tail -f /tmp/launcher-homehook.log`, then use the TV.
Lines look like `watcher: started`, `watcher: LG Home is in front: opened the launcher`, `watcher: the launcher has not shown itself: opening it again`,
`launcher: first screen NNN ms after the page started` and `launcher: visible` / `launcher: hidden`.

**To get going again right away** (if the helper has stopped): `sh /media/developer/launcher-homehook/apply.sh` starts it if it isn't running.
If it says it is running but the diagnose output shows it isn't, run `rm -f /tmp/launcher-homehook-watch.pid` first.

## Notes
- The "LG Home" tile in the launcher lets LG's own Home show: it runs `stock.sh`, then opens LG Home.
- While the launcher is open it also ends LG's home app to free memory (Settings → Options → Close LG Home when this opens); the TV starts it again only when needed.
