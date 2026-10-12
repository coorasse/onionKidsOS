# Development

How KidsOS is built and tested. For installing and using it, see the [README](../README.md).

KidsOS replaces the user interface of OnionOS on the Miyoo Mini Plus and reuses everything else. On the
Miyoo Mini every "OS" is a payload on the SD card that the stock firmware boots into: Onion's boot script
(`.tmp_update/runtime.sh`) starts its MainUI, runs the game MainUI chose, shows the GameSwitcher, repeats.
MainUI is a closed Miyoo binary, so KidsOS puts a shell shim in its place that starts the kids launcher;
the launcher writes the game command exactly like MainUI would. A MinUI pak exists too but is secondary.

The same C code runs on macOS for development, in a window or headless for screenshots.

## Layout

```
src/
  main.c               state machine (kid UI, resume, playing), scripted screenshots
  kid_ui.c             the carousel
  resume_ui.c          continue or start over, shown when the game has an auto save state
  osd.c                big volume indicator, drawn over any screen after a volume key
  library.c            scans Roms/ (Onion or MinUI layout), reads Onion's favourites or kids/enabled.txt
  gfx.c                drawing helpers on SDL surfaces (rounded rects, scaling, text)
  platform.h           what a platform must provide (video, input, launching a game)
  platform_desktop.c   macOS/Linux implementation with SDL2
  platform_onion.c     Miyoo Mini Plus with OnionOS (SDL 1.2, 32 bit screen)
  platform_miyoomini.c Miyoo Mini Plus with MinUI (SDL 1.2, 16 bit screen)
onion/                 device-side scripts: launch.sh, game.sh, install.sh (shims MainUI/GameSwitcher, kids keymap,
                       boot hook), startup.sh (the boot hook), uninstall.sh
pak/                   MinUI pak launch script
sdcard/, sdcard-onion/ fake SD cards for development (empty roms + generated placeholder art)
tools/build_dist.rb    builds the KidsOS zip from the official Onion release (make dist)
tools/test_onion_install.rb  boots Onion's runtime.sh in Docker against a fake card (make test-onion, make test-dist)
tools/sevenzip.rb      7z for the two scripts above (local binary or Alpine container)
tools/make_covers.py   regenerates the fake SD cards
tools/fetch_covers.rb  downloads box art for the real card from thumbnails.libretro.com
tools/fb2png.py        turns a /dev/fb0 dump from the device into a PNG
docs/                  this file and the README screenshots
```

Both SD layouts are detected automatically:

- Onion: `Emu/<TAG>/config.json` describes each system (rom folder, box art folder, extensions);
  roms in `Roms/<TAG>/`, box art in `Roms/<TAG>/Imgs/<game>.png`.
- MinUI: `Roms/<System Name> (<TAG>)/<game>.<ext>`, box art in `.res/<game>.<ext>.png`.

Multi-disc PlayStation games live in a subfolder with an `.m3u`. On Onion the list of games visible to the
kid is Onion's own favourites file, `Roms/favourite.json`, in the order the favourites were added. On MinUI
it is a plain text file, `kids/enabled.txt`, one path per line relative to `Roms/`, edited from a computer.

## Desktop build

Requirements: `brew install sdl2 sdl2_image sdl2_ttf` (Docker is not needed for the desktop build).
`sdl2_mixer` is optional: with it the desktop build plays the menu music and click from `build/sound/`
(`bgm.mp3` and `change.wav` copied from the device's `/customer/app/sound/`); without it, silence.

```
make            # builds build/kidsos
make run        # opens the launcher in a 640x480 window using ./sdcard
make shots      # headless run with scripted input, saves PNGs in shots/ (MinUI fixture)
make shots-onion# same on the Onion fixture, PNGs in shots/onion/
make shots-resume # the continue / start over screen, PNGs in shots/resume/
make covers     # regenerates both fake SD cards
make fetch-covers SD=/Volumes/ONIONOS  # downloads missing box art into Roms/<TAG>/Imgs/ of the real card
```

Keys in the window: arrows = d-pad, `X` = A, `Z` = B, `S` = X, `A` = Y, `Q`/`W` = L1/R1,
`Enter` = START, `Tab` = SELECT, `Esc` = MENU, `=`/`-` = volume. `KIDS_ROOT=<dir>` points the launcher at
another SD layout, for example the real Onion card while it is mounted: `KIDS_ROOT=/Volumes/ONIONOS make run`.

Games really run on the desktop when RetroArch for macOS is installed (`brew install --cask retroarch`):
the launcher reads the libretro core name from Onion's `Emu/<TAG>/launch.sh` and starts
`/Applications/RetroArch.app` with the same core from `~/Library/Application Support/RetroArch/cores/`
(download them from RetroArch's Online Updater, or from buildbot.libretro.com). Cores without a macOS build
fall back to an equivalent one (`mednafen_supafaust` to `snes9x`, `pcsx_rearmed` to `swanstation`, ...).
Without RetroArch or the core, the launcher shows a stand-in "playing" screen instead.

Keyboard mapping in the window:

| Miyoo | Key            |
|-------|----------------|
| D-pad | Arrow keys     |
| A     | X              |
| B     | Z              |
| X     | S              |
| Y     | A              |
| L1/R1 | Q / W          |
| START | Return         |
| SELECT| Right Shift or Tab |
| MENU  | Escape         |

`KIDS_ROOT` points the launcher at another SD card root, `KIDS_FONT` at another TTF font.

### Scripted screenshots

`./build/kidsos --script "right,right,shot:kid,hold:select+start:1500,shot:hold,a,shot:playing" --shots shots`
runs without a window. Tokens: a button name presses it for one frame, `hold:<btn>+<btn>:<ms>` keeps
buttons down, `wait:<ms>` idles and `shot:<name>` writes `shots/<name>.png`.

## Device build

Everything for the device is cross-compiled inside MinUI's toolchain container (Docker must be
running; the first run clones the toolchain repo and builds the image, which takes a few minutes).

### OnionOS over Wi-Fi (recommended, no card reader needed)

On the device: Apps > Tweaks > Network, connect to Wi-Fi and enable SSH. Note the IP shown there.
Onion's SSH is root without password.

```
make deploy MIYOO=192.168.1.42   # cross-compile, copy /mnt/SDCARD/.kidsos, install the shims
make device-restart MIYOO=...    # leave MainUI and start the kids launcher right now
make device-shot MIYOO=...       # real screenshot of the device into shots/device.png
make device-log MIYOO=...        # launcher log and the tail of Onion's runtime log
make undeploy MIYOO=...          # restore Onion's binaries and remove the launcher
```

`install.sh` never edits `runtime.sh`. Onion starts its UI from binaries in `.tmp_update/bin`, so
the installer renames each of them to `<name>.real` and puts a five-line shell shim in its place:

1. `MainUI-<model>-<mode>` runs `.kidsos/launch.sh` while kids mode is active. The launcher writes
   the game command to `/tmp/cmd_to_run.sh` exactly like MainUI does and exits, so `launch_game` and
   everything after it is untouched Onion.
2. `gameSwitcher` exits at once when Onion runs it after a game, so the switcher never appears; it
   drops the pending command like the real one does, otherwise Onion would relaunch the game. The
   overlay variant (`gameSwitcher --overlay`, started by Onion's keymon on top of a paused RetroArch)
   is passed through to the real binary, because only that one resumes the game.

Onion's keymon decides what MENU does inside a game and by default a single press opens that overlay.
`install.sh` therefore writes a kids `keymap.json` (`.tmp_update/config/`): a single MENU press saves
and quits the game, so Onion comes straight back to the kid UI; long and double press do nothing. The
MainUI actions are kept and the original file comes back with `uninstall.sh`. Changing the MENU
button in Apps > Tweaks overrides this until the next `make deploy`.

Inside RetroArch's own menu, "Close Content" does not quit RetroArch on Onion (`quit_on_close_content`
is off): it leaves an empty RetroArch behind. Use "Quit RetroArch" or the MENU button instead.

Holding SELECT + START in the kid UI creates `/tmp/kidsos_exit`; the shims then run the real binaries
until the next reboot. If the launcher ever crashes, `launch.sh` sets the same flag so the parent is
never locked out. Onion updates overwrite `.tmp_update/bin`: run `make deploy` again afterwards, it
also replaces shims from an older version of this installer.

Continue or start over works through Onion's own flags: the launcher touches `/tmp/force_auto_load_state`
or `/tmp/reset_game` next to the game command and `runtime.sh` turns them into RetroArch's
`savestate_auto_load`. The auto save state is the one RetroArch writes when the game is left with MENU,
in `Saves/CurrentProfile/states/<core>/<rom>.state.auto`; `install.sh` switches on
`savestate_thumbnail_enable` in `retroarch.cfg` so the continue tile can show the picture RetroArch
saves next to it. `uninstall.sh` switches it off again.

The volume keys are handled by Onion's keymon, which also paints a thin bar at the screen edge that
flickers under a UI that redraws. The launcher reads the keys itself from `/dev/input/event0`, shows its
own indicator, and `launch.sh` sets keymon's bar width (`config/display/meterWidth`) to 0 while the
launcher runs and restores it on exit, so games keep Onion's bar. The value is counted from the keys like
keymon does it and re-read from `system.json` once the keys have been quiet for a few seconds.

The main loop only draws when something moved or a button changed; idle, the launcher uses no CPU.

The menu music and the selection click are the stock Miyoo ones, read from the firmware partition
(`/customer/app/sound/bgm.mp3` and `change.wav`); Onion's copies on the card are dummies. They are played
through Onion's own `libSDL_mixer`, loaded at run time, and follow Onion's theme music volume and mute
setting (Tweaks > Appearance), so the parent can turn them off without touching the launcher.

If the screen stays black after a game:
 hold POWER until the device turns off, start it again and run
`make device-log` once it is on Wi-Fi. `launch.sh` keeps the log of the previous run as
`.kidsos/kidsos.log.1`, so what happened before the restart is still there.

`make test-onion` boots Onion's real `runtime.sh` in a BusyBox container against a fake SD card with
the shims installed and checks the whole sequence (kid UI, game launch, GameSwitcher overlay during the
game, GameSwitcher skipped afterwards, exit to Onion, keymap, stale shim update, uninstall) without the
device. It clones Onion into `../reference/Onion` on first use.

### OnionOS with the card in the Mac

`make deploy-card SD=/Volumes/ONIONOS` does the same as `make deploy` without the device: it copies
`.kidsos` to the card and runs `install.sh` in a BusyBox container with the card mounted as
`/mnt/SDCARD`. `make recover SD=...` undoes the shims from the Mac.

### The zip parents install

`make dist` builds `build/KidsOS-<version>-onion<onion version>.zip`, which is the official Onion release
zip with the kids layer added to `miyoo/app/.tmp_update/onion.pak`, the 7z archive that Onion's installer
unpacks over the card at the first boot and at every update. The layer is `.kidsos/` (the same folder
`make deploy` sends) plus `.tmp_update/startup/kidsos.sh`, a hook Onion runs at every boot
(`runtime.sh`, "startup scripts"): it calls `.kidsos/install.sh`, which is idempotent, so the shims are
installed at the first boot and come back by themselves after an Onion update wiped `.tmp_update/bin`.
Onion's own scripts and its installer are not modified; only files are added to an archive.

Why not a fork of Onion: MainUI is a closed Miyoo binary that Onion patches in hex, so a fork could not
change the UI either, it would have to replace it exactly like the shims do; Onion's build is 2 GB of blobs
plus a RetroArch compile, and its last stable release is from June 2024. Rebasing on a new Onion release is
a version and checksum bump in the Makefile (`ONION_VERSION`, `ONION_SHA256`; run `make dist` with
`ONION_SHA256=-` once to print the checksum of the new zip).

The zip is 7z-edited with `7zz`/`7z` when installed (`brew install sevenzip`), otherwise in an Alpine
container. `make test-dist` boots the `runtime.sh` from the zip's `onion.pak` in BusyBox: the hook must
install the shims at the first boot, change nothing at the second, and reinstall them after a simulated
Onion update (the installer stub's wipe of `.tmp_update` except `config/` and `onionVersion/`).

`.github/workflows/release.yml` runs `make dist` and `make test-dist` when a `v*` tag is pushed and attaches
the zip to a GitHub pre-release.

Not touched on an Onion update (verified in Onion's `install.sh`): `Roms/` with `favourite.json`,
`Saves/`, `.tmp_update/config/` (so the kids keymap and its backup survive), `.kidsos/`. Onion's OTA update
app installs plain Onion and so removes the hook; the README tells parents not to use it.

### MinUI

```
make miyoomini   # builds build/miyoomini/kidsos.elf (ARM, SDL 1.2)
make pak         # assembles build/pak/Tools/miyoomini/Kids.pak and build/KidsOS-miyoomini.zip
```

Copy `build/pak/Tools` onto the root of a MinUI card; start it from Tools, or autostart it with
`.userdata/miyoomini/auto.sh` containing `/mnt/SDCARD/Tools/miyoomini/Kids.pak/launch.sh`.
MinUI has no Wi-Fi stack, so this route needs a card reader.

## Roadmap

1. Desktop prototype of the kid carousel. Done. (The parent menu was replaced by Onion's favourites.)
2. Device platform layers for Onion and MinUI, Docker cross-build, Wi-Fi deploy. Done, confirmed on hardware.
3. Distribution zip built on the official Onion release, boot hook, Docker test. Done, zip not yet booted on
   a freshly formatted card in a real device.
4. Empty state of the kid UI when there are no favourites yet (a hint for the parent instead of nothing).
5. Parent settings in kids/settings.txt: play time limit, volume cap.
6. Nicer kid UI: a short "press A" animation, optional system badge on tiles.
