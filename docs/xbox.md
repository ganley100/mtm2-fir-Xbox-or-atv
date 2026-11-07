# Xbox Dev Mode

Xbox Dev Mode runs UWP apps, and only 64-bit ones. The Windows build can't run
there: it hands the game's calls to real Windows from a 32-bit process. So the
Xbox build uses the same portable host as Android (`src/portable/`, on
pcrecomp's `win32hle` and SDL2), built as a UWP x64 app:

- **SDL2** is built for UWP with MSVC. Its UWP backend is C++/CX, and it
  provides the window, controllers and sound.
- **The game, the host and `win32hle`** are built with clang-cl. `win32hle`
  is written against POSIX (threads, `mmap`, `dirent`, ...). A UWP app has
  none of that, so `src/xbox/compat/` supplies it from the Win32 calls an app
  may make.
- **The package** is an `.msix` signed with a self-signed certificate, which
  you install through the Xbox Device Portal.

Both versions build: the full game (`Monster Truck Madness 2`) and the 1998
demo (`Monster Truck Madness 2 Demo`), as two separate apps.

**Status: the demo races on this host; not yet run on an Xbox.** The 1998
demo runs on the same portable host, built for Linux x64 with the same
64-bit memory layout as the Xbox build. It goes from its title menu
through Driver Check-in and GO into a race, and drives at full speed. The
lists on Driver Check-in (name, skill, truck) are not drawn yet. Nothing
has been started on a console yet. GitHub Actions
(`.github/workflows/xbox.yml`) builds and packages both versions without the
lift, as a check of the toolchain and the packaging. Nothing has been started
on a console yet.

## What you need

- A Windows 10/11 PC with Visual Studio 2022 (Community is fine), with
  *Desktop development with C++*, *Universal Windows Platform development* and
  *C++ Clang tools for Windows*, plus `git` and Python 3 with `capstone`.
- pcrecomp beside this repository, as for every build.
- Your own game files and their lift, made as for Windows (README, *Step by
  step* 1–3; `--demo` for the demo).
- An Xbox One or Series console in Dev Mode (the *Xbox Dev Mode* app, with a
  one-time Partner Center developer registration), with the Device Portal
  turned on in the Dev Home app.

## The demo, step by step

On a Windows PC with the tools above, in this repository:

```
py -3 tools\gamefiles.py --demo MtM2trial.EXE      # the trial's installer, as downloaded
py -3 tools\catalog.py --demo                      # about 2 minutes
py -3 run_lift.py --demo                           # about 1 minute
xbox\build.ps1 -Variant demo                       # SDL2 once, then the game (a while)
xbox\package.ps1 -Variant demo -Game game-demo
```

`gamefiles.py` unpacks the installer without running it (Windows' own
`expand.exe` does this) and lays the files out in `game-demo\`.

Then install `xbox\out\MTM2Recomp.Demo_0.1.0.0_x64.msix` as below, and set
the app's type to **Game**.

### Or in GitHub Actions, encrypted

`.github/workflows/xbox-demo.yml` does the same steps on a GitHub Windows
runner, which works in this public repository's free minutes. The package
has the demo inside, so the run uploads it only inside a 7-Zip archive
encrypted with your password (AES-256, file names hidden). Anyone can see the
run and download the archive, but without the password it can't be opened.
Nothing of the game is committed, and the runner is thrown away after.

1. Add two repository secrets (Settings → Secrets and variables → Actions):
   - `MTM2_TRIAL_URL`: a direct download link to `MtM2trial.EXE`. It is
     checked against the trial's SHA-256 before it is used.
   - `MTM2_ARTIFACT_PASSWORD`: the archive's password, 12 characters or more.
2. Actions → *Xbox demo (encrypted)* → *Run workflow* (the workflow has to be
   on the default branch for the button to show).
3. Download the artifact `mtm2-xbox-demo-encrypted-...` within a day (GitHub
   deletes it after that) and open
   `mtm2-xbox-demo.7z` with 7-Zip and your password. It contains the
   `.msix`, `mtm2recomp.cer` and `Microsoft.VCLibs.x64.14.00.appx`; install
   them as below.

## Build and package

From a PowerShell in the repository:

```
xbox\build.ps1                         # the full game: src\recomp\gen
xbox\package.ps1 -Game game            # its .msix, with your game\ folder inside

xbox\build.ps1 -Variant demo           # the demo: src\recomp\gen-demo
xbox\package.ps1 -Variant demo -Game game-demo
```

`build.ps1` fetches SDL2 2.30.9 and SDL2_ttf 2.22.0 into `xbox\deps` and
builds SDL2 for UWP once. It then builds `xbox\build\<variant>\mtm\mtm.exe`.
`-NoLift` builds without the lift, as CI does, to check the setup quickly.

`package.ps1` writes three files to `xbox\out\`:

| File | What |
|---|---|
| `MTM2Recomp_0.1.0.0_x64.msix` (or `MTM2Recomp.Demo_...`) | the app |
| `mtm2recomp.cer` | the certificate it is signed with (made once, kept in `xbox\cert\`) |
| `Microsoft.VCLibs.x64.14.00.appx` | the C++ runtime the app needs |

With `-Game`, the package carries your game files, read-only. Whatever the
game writes (its settings, saves) goes to the app's own `LocalState\game`
folder instead. A package with the game inside is yours alone; don't share it.

The tiles and splash screen are plain built-in ones unless you make your own
from box art. The same command makes the Android TV banner too:

```
py -3 tools\make_banner.py boxart.jpg --top 30
py -3 tools\make_banner.py --demo boxart-demo.jpg --top 30
```

That writes `xbox\Assets-user\` (and `Assets-user-demo\`), which
`package.ps1` uses in place of the built-in tiles. The folders are gitignored.

For the Liberation fonts (the menus' text), put `LiberationSans-*.ttf` from the
[Liberation fonts](https://github.com/liberationfonts/liberation-fonts) release
in `xbox\fonts\` before packaging. CI does this itself.

## Install

1. On the Xbox, in Dev Home, note the Device Portal address
   (`https://<xbox-ip>:11443`) and open it in a browser on your PC.
2. *Home* → *Add*. Choose the `.msix` and tick the box for a certificate,
   giving `mtm2recomp.cer`. As a dependency, add
   `Microsoft.VCLibs.x64.14.00.appx`. Then *Start*.
3. The app shows in Dev Home's list. Select it, *View details*, and set its
   type to **Game**: an App gets far less memory and GPU time.

### The game files without packaging them

You can also install the package without `-Game` and upload the files to the
app's own folder. In the Device Portal, open *File explorer* →
`LocalAppData` → `MTM2Recomp_...` → `LocalState`, make a folder `game`, and
upload into it what `tools\gamefiles.py` made: `Monster.exe`, the `.POD`
files, `system\` and `Shared\Videos\`. That is about 85 files across a few
folders through a web page, so packaging with `-Game` is usually quicker.

The app looks in `LocalState\game` first, then in the package's own `game\`.

## Controls

The controller works as on Windows and Android, with the same
`controller.ini`: RT throttle, LT brake/reverse, left stick or D-pad to steer,
A for ENTER, B and START for ESC, and View (Back) to switch cursor mode for the
menus. The B button is kept for the game, so it doesn't close the app. To
leave, press the Xbox button. A `controller.ini` in the game folder changes
the mapping.

## When it doesn't start

The app writes its log to `LocalState\mtm.log`. Download it with the Device
Portal's *File explorer*. A Windows call the game makes that `win32hle` does
not answer shows there as `unimplemented: <name>`, the same as on Android.

## What is different from Windows

As on Android: the Smacker intros are skipped, DirectInput has no devices,
and the display-mode change is only logged (docs/android.md). On top of that:

- **No networking.** `wsock32.c` is left out. MTM2 doesn't import Winsock, and
  its DirectPlay calls are stubs anyway.
- **File mappings are copies.** Writes to a shared mapping don't reach the
  file. The single-player game isn't known to use them.
- **Memory.** The game's 32-bit memory is one 4 GB block of address space,
  committed only as it is used, as on any 64-bit host.

## How it fits together

| File | What |
|---|---|
| `src/xbox/compat/` | POSIX for `win32hle` on UWP: `xbox_compat.h` (force-included), `pthread.h`, `dirent.h`, `sys/mman.h`, ... and `compat.c`. Host paths are `"/C:/..."`; the current folder is kept here; `struct stat` has `st_mtim` |
| `src/xbox/xbox_main.c` | `WinMain` into SDL's WinRT backend; finds the game, opens the log, sets SDL's hints |
| `CMakeLists.txt` | `MTM_XBOX`: drops `wsock32.c`, rewrites `kernel32_ext.c`'s one `tm_gmtoff`, links an app container against `WindowsApp.lib` and the Store CRT |
| `xbox/CMakeLists.txt` | the prebuilt UWP SDL2, SDL2_ttf with its FreeType, and the game |
| `xbox/build.ps1`, `xbox/package.ps1` | the two build steps, then the signed `.msix` |
| `xbox/AppxManifest.xml.in`, `xbox/Assets*/` | the manifest and the tiles, per version |
