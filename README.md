# Monster Truck Madness — Static Recompilation

Static recompilation of **Monster Truck Madness 2** (Microsoft / Terminal
Reality, 1998) from its shipping Win32 binary to native C, on the
[pcrecomp](https://github.com/sp00nznet/pcrecomp) toolchain. Monster Truck
Madness 1 (1996) is next, once MTM2 races.

Part of the pcrecomp collection and its shared house style; siblings from the
same engine house are [fury3](https://github.com/sp00nznet/fury3) and
[hellbender](https://github.com/sp00nznet/hellbender).

## Status: alpha — a race drives

`Monster.exe` is lifted whole (22,279 functions, 0 lift errors, 2.46M lines of
C) and runs on pcrecomp's `native32` host. It plays both intro videos, goes
through the title menu and Driver Check-in, and starts a race on the
software renderer, windowed, and the truck drives: throttle and steering by
scripted keys, through the gears and down the track. A whole race has not been
finished yet.

| Milestone | State |
|---|---|
| Catalog + lift, 0 errors | done |
| Boots: CRT, WinMain, window, `.pod` mounts, `monster.ini` | done |
| Smacker intro videos (MS logo, TRI logo) | done |
| Title menu (MFC + GDI) | done |
| Driver Check-in (3D truck preview), GO, "Before You Begin" | done |
| A race on the software renderer (windowed, GDI) | drives; not finished yet |
| Hardware renderers (`TRID3D.DLL`, Glide) | not started |
| Android TV (`armeabi-v7a`) on pcrecomp's `win32hle` | set up, not yet run on a device ([docs/android.md](docs/android.md)) |

Needs pcrecomp [#81](https://github.com/sp00nznet/pcrecomp/pull/81) (thunk
tables in `disasm32`); without it the run faults on the first call through a
thunk the catalog missed.

## Screenshots

From a headless recording (`--headless --record`):

| Intro | Title menu |
|---|---|
| ![intro](docs/img/intro.png) | ![title](docs/img/title.png) |
| **Driver Check-in** | **Racing (cockpit view, third gear)** |
| ![check-in](docs/img/checkin.png) | ![race](docs/img/race.png) |

## Getting Started

You need your own Monster Truck Madness 2 disc or an ISO of it. Nothing from
the game is in this repo, and nothing is downloaded.

### Step by step

Prerequisites: Windows 10/11, Python 3.10+ with `capstone` (`py -3 -m pip
install capstone`), Visual Studio 2022 or its Build Tools with the C++ x86
tools, CMake 3.20+, Ninja, and `ffmpeg` on PATH for `--record`.

1. Clone this repo and pcrecomp side by side, pcrecomp on a commit that has
   #81:
   ```
   git clone https://github.com/sp00nznet/mtm
   git clone https://github.com/sp00nznet/pcrecomp
   ```
2. Copy the game off your disc into `game\`:
   ```
   py -3 tools\gamefiles.py "Monster Truck Madness 2.iso"
   ```
   It prints `85 files in ...\game`.
3. Catalog and lift (about 2 and 1 minutes):
   ```
   py -3 tools\catalog.py
   py -3 run_lift.py
   ```
   The lift ends with `lifted 22279   errors 0`.
4. Build: `build.cmd` (MSVC x86 + Ninja) gives `build\mtm.exe`.
5. Run it headless with a recording. This clicks Driver Check-in, then GO,
   then ENTER for "Before You Begin"; the race starts at about 80 s, and UP
   holds the throttle from 85 s:
   ```
   build\mtm.exe --run --headless --watchdog 125 --record out.mp4 --click 560,433@60000 --click 570,430@68000 --key ENTER@78000 --key UP@85000+38000
   ```

### The 1998 demo

Microsoft's free demo of Monster Truck Madness 2 is a separate, smaller
`Monster.exe`, built the same way next to the full game. Every step takes
`--demo` and keeps the demo's files apart (`game-demo\`, `work\demo\`,
`src\recomp\gen-demo\`; see `tools/variant.py`):

1. Run the demo's installer on any Windows PC, then copy what it installed
   (give `--exe NAME` if its program is not called `Monster.exe`):
   ```
   py -3 tools\gamefiles.py --demo "C:\path\the\demo\installed\to"
   ```
2. `py -3 tools\catalog.py --demo`, then `py -3 run_lift.py --demo`.
3. Build it into its own folder:
   `set BUILD_DIR=build-demo& set CMAKE_ARGS=-DMTM_VARIANT=demo& build.cmd`.
   `build-demo\mtm.exe --run` reads `game-demo\`.

The demo has not been lifted or run yet.

Use `py` rather than `python`: the Microsoft Store alias answers `python` and
does nothing. A Quick start (`Setup.cmd`) is not written yet; see
[ROADMAP.md](ROADMAP.md).

## Usage

```
mtm [--run] [--headless] [--record out.mp4] [--frames N] [--shot N:file.bmp]
    [--key NAME@MS[+HOLD]] [--click X,Y@MS] [--game game] [--watchdog S]
    [--pad controller.ini] [--no-pad]
    [--native-trace] [--callbacks] [--ddraw-trace]
```

Without `--run` it only maps and binds the image (a dry run). `--headless`
draws nothing on screen; `--record` pipes frames to ffmpeg at 30 fps.

## Controller

An Xbox-style controller (anything XInput sees: Xbox 360, One and Series
pads, and most others in XInput mode) works in a normal windowed run. The
game itself only reads the keyboard and mouse, so the host turns each
button, trigger and stick direction into a key, as set in
[`config/controller.ini`](config/controller.ini). The build copies that file
beside `mtm.exe`, which is where it is read from (`--pad FILE` picks another,
`--no-pad` turns the controller off; headless runs never use it).

Out of the box: RT throttle, LT brake/reverse, left stick or D-pad steer,
A is ENTER, B and START are ESC. **BACK** switches to cursor mode, where the
sticks move the mouse pointer over the game's window and A/B click, for the
title menu and Driver Check-in; BACK again goes back to driving. Gears,
camera and the other race keys are left free in the file to map to the
game's own key list.

## Android TV

The same lift also builds for Android TV / Google TV as an APK, on
pcrecomp's portable `win32hle` layer and SDL2 instead of Windows itself, with
the same controller mapping and the remote's D-pad, OK and back buttons.
It has not been run on a device yet; [docs/android.md](docs/android.md) has the
build, install and status.

## Building from source

`build.cmd` builds locally. Day to day the build runs on the
[recomp-netlab](https://github.com/sp00nznet/recomp-netlab) farm with
`netlab build mtm` (recipe `projects/mtm.env`).

[docs/architecture.md](docs/architecture.md) has the pipeline and what the
host changes about the game's world.

## What is not in this repo

The game, its data and anything derived from its binary: the function
catalog, the lifted C (`src/recomp/gen/`) and run logs are generated
locally into gitignored folders and never committed.

## License

The code and documentation here are MIT. [LICENSE](LICENSE) spells out that
the grant stops at our own work and does not reach the game or anything
lifted from it. Monster Truck Madness © 1996 and Monster Truck Madness 2 ©
1998, Microsoft / Terminal Reality.
