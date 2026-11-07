# Android TV

The same lift as the Windows build, on a different host. Windows runs it on
pcrecomp's `native32`, which hands every Windows call to the real Windows.
Android has no Windows, so `src/portable/` runs it on pcrecomp's
`win32hle`, which answers those calls itself in C on SDL2: windows and
dialogs, GDI, DirectDraw, DirectSound. SDL2 gives the screen, sound,
controllers and the remote. The APK is built for `armeabi-v7a` (32-bit ARM,
what most Google TV and Android TV devices run apps as).

**Status: set up, not yet built into an APK or run on a device.** The host
and the whole `win32hle` layer pass a compile check for `armeabi-v7a` and
`arm64-v8a` against the NDK's headers, but the APK has not been linked and
the game has not been started on Android yet. Expect the first runs to stop on Windows
calls the game makes that `win32hle` does not answer yet (see *When it
stops*, below).

## What you need

- Your own Monster Truck Madness 2 disc or ISO, and the lifted code made from
  it (`src/recomp/gen/`). The lift is the same as for Windows: steps 1 to 3 of
  the README's *Step by step* (on Windows or Linux, Python 3 + `capstone`).
- Android Studio, or the Android SDK command-line tools, with **NDK
  27.2.12479018** (r27c) and **CMake 3.22.1** (SDK Manager, *SDK Tools*).
- `git`, to fetch SDL2.
- An Android TV / Google TV device on Android 7.0 or newer with *Developer
  options* and *USB* or *network debugging* on, and `adb`.

## Build

From the repository root, with pcrecomp beside it as for Windows:

```
android/fetch-deps.sh                 # SDL2 2.30.9 and SDL2_ttf 2.22.0 into android/deps
cd android
./gradlew assembleRelease             # gradlew.bat on Windows
```

GitHub Actions (`.github/workflows/android.yml`) runs this build on every
push that touches the Android or host code, with `mtm.noLift=ON` since the
lift is never in the repository. Its APK is a build check, not the game.

That builds two APKs, one per version of the game. Each one compiles its own
lift, so give each version you build its lift first (`run_lift.py`,
`run_lift.py --demo`):

| APK | App | Needs |
|---|---|---|
| `app/build/outputs/apk/full/release/app-full-release.apk` | Monster Truck Madness 2 (`com.mtm2.recomp`) | `src/recomp/gen` |
| `app/build/outputs/apk/demo/release/app-demo-release.apk` | Monster Truck Madness 2 Demo (`com.mtm2.recomp.demo`) | `src/recomp/gen-demo` |

`./gradlew assembleFullRelease` or `./gradlew assembleDemoRelease` builds
just one. They are separate apps with their own folders, so both can be on
the TV at once. Both are signed with the debug key so they install as they
are.

Compiling the lift (2.46M lines of C) takes a while. Two settings help, in
`android/gradle.properties` or with `-P` on the command line:

- `mtm.genOpt=-O1` compiles the lift faster.
- `mtm.noLift=ON` builds without the lift at all. The APK installs and
  starts, then says it has no game code. Use it to check the toolchain and the
  packaging before the long build.

`mtm.abis=armeabi-v7a,arm64-v8a` adds a 64-bit build. Both run the same
code; on 64-bit the game's 32-bit memory is one 4 GB block instead of
addresses taken as they are.

## The banner

The TV home screen shows a 640x360 banner. The built-in one is plain. To use
your own box art, run this before building:

```
py -3 tools/make_banner.py boxart.jpg --top 12
py -3 tools/make_banner.py --demo demoart.jpg     # the demo app's, if you want it different
```

Without a banner of its own, the demo app uses yours. Without either, it
uses the built-in one with a DEMO ribbon.

It also makes the Xbox tiles (docs/xbox.md). It crops a 16:9 piece from the top of the picture, which on the box front
is the title, above the ratings and publisher logos. It writes the crop to
`android/banner/`, and the build uses it in place of the built-in banner.
That folder is gitignored, so the artwork stays out of the repository.

## Install and copy the game

```
adb install -r app/build/outputs/apk/full/release/app-full-release.apk
adb install -r app/build/outputs/apk/demo/release/app-demo-release.apk
```

Start it once from the TV's home screen. It makes its game folder and then
says Monster.exe is missing. Copy the game in: the `game` folder that
`tools/gamefiles.py` made, with `Monster.exe`, the `.POD` files, `system\`
and the videos:

```
adb push game/. /sdcard/Android/data/com.mtm2.recomp/files/game/
adb push game-demo/. /sdcard/Android/data/com.mtm2.recomp.demo/files/game/
```

Optional, in the same folder:

- `controller.ini`, to change the controller mapping (copy
  `config/controller.ini` and edit it; the same file as on Windows).
- `fonts/` with the Liberation fonts (`LiberationSans-Regular.ttf`,
  `LiberationSans-Bold.ttf`, ...). The front end's text is drawn in Arial and
  MS Sans Serif, and Liberation has the same metrics. Without them it uses
  Android's Roboto, which is a little wider.

## Controls

| | |
|---|---|
| Gamepad | as on Windows: RT throttle, LT brake/reverse, left stick or D-pad steer, A is ENTER, B and START are ESC, BACK switches cursor mode |
| Remote D-pad | arrow keys |
| Remote centre / OK | ENTER |
| Remote back | ESC |

Cursor mode moves the game's mouse pointer with the sticks, for the title
menu and Driver Check-in, which are mouse-driven. Android only draws a
pointer when a mouse is present, so without one (an air-mouse remote or a USB
or Bluetooth mouse) you can't see where it is yet. A pointer drawn over the
picture is the next step.

## What is different from Windows

- **Intro videos are skipped.** `smackw32.dll` (RAD Smacker) is Windows x86
  code and `win32hle` has no Smacker player, so every Smacker call answers
  0, and `SmackOpen`'s 0 means "no video". A player on ffmpeg (which decodes
  Smacker) would bring them back.
- **DirectInput** is an object with no devices. The game creates it and never
  uses it, because the race reads the keyboard.
- **`ChangeDisplaySettingsA`** is only logged. The screen is SDL's and always
  full screen, with the picture scaled to fit.
- Every other Windows call that `win32hle` does not answer is bound to a stub
  that stops the game with its name (below). Monster.exe imports DirectPlay,
  IMM32, MSACM32, WINSPOOL and comdlg32, which `win32hle` mostly lacks;
  whether the single-player game calls them is what the first runs will show.

## When it stops

```
adb logcat -s SDL SDL/APP
```

The host logs as it goes (`[mtm]`, `[pad]`, `[display]`, `[dinput]`, and
`win32hle`'s own). A missing Windows call shows as `unimplemented: <name>`:
that name is the next shim to write, in `src/portable/mtm_hle.c` for
something only this game needs, or in pcrecomp's `win32hle` for a general
Windows call.

The quickest place to work on these is likely a Linux PC. The same host
should build there, though that is untested too (`cmake -B build-linux -DPCRECOMP=../pcrecomp && cmake --build build-linux`,
with SDL2 and SDL2_ttf installed; `-DCMAKE_C_FLAGS=-m32` and the i386 SDL2
packages for the 32-bit layout the TV uses) and runs with
`build-linux/mtm --game game --windowed`.

## How it fits together

| File | What |
|---|---|
| `src/portable/mtm_hle.c` | The host: finds the game, MTM2's own shims (Smacker, DirectInput, the display mode), boots `Monster.exe` on `win32hle` |
| `src/portable/pad_sdl.c` | SDL game controllers and the remote, through the shared mapping |
| `src/pad/pad_map.c` | `controller.ini` and drive and cursor mode, shared with the Windows host |
| `src/portable/arm32/recomp_types.h` | On 32-bit ARM, 64-bit and floating-point accesses go through packed structs. x86 does them at any address; ARMv7's `VLDR`/`LDRD` would fault off a 4-byte boundary |
| `CMakeLists.txt` | `MTM_HOST=win32hle` builds `win32hle` and the host. It also compiles a copy of `gdidc.c` that looks for fonts in the game folder and `/system/fonts` |
| `android/` | The Gradle project: `MtmActivity` (SDL's activity), the TV manifest and banner, and the native build around SDL2, SDL2_ttf and the CMakeLists above |
