# Changelog

Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow [SemVer](https://semver.org/).

## [Unreleased]

### Added
- Monster Truck Madness 2 lifted whole with pcrecomp (22,279 functions, 0 lift
  errors) and run on the `native32` host: it boots, plays the Smacker intros
  and reaches the title menu. Needs pcrecomp #81.
- `tools/gamefiles.py` (game files off the disc or ISO), `tools/catalog.py`,
  `run_lift.py`, the host (`src/runtime/`), and a netlab recipe (`projects/mtm.env`).
- Headless recording of the GDI front end and the windowed race, read back
  with `PrintWindow` while the UI thread is idle, with the active dialog drawn
  over it.
- Headless runs cloak every top-level window (the front end's dialogs too),
  and `--key`/`--click` reach a modal dialog and the MFC child controls under
  the point. With these, a scripted run reaches a race on the software
  renderer.
- The race can be driven with `--key`: keys go to the game thread's focus
  window (the race's view inside the MDI frame), as real keys do, and
  `GetKeyState` sees the keys `--key` holds.

- Controller support: an XInput pad drives the race and, in cursor mode
  (BACK), moves the mouse over the menus. The mapping is
  `config/controller.ini`, copied beside `mtm.exe` by the build; `--pad FILE`
  and `--no-pad` on the command line.

- Android TV build (not yet run on a device): the portable host
  `src/portable/` runs the lift on pcrecomp's `win32hle` (SDL2), with shims
  for what MTM2 needs that it lacks (Smacker calls answer 0 so the intros
  are skipped, a device-less DirectInput, a logged `ChangeDisplaySettingsA`).
  `android/` is the Gradle project (`armeabi-v7a`, TV launcher and banner).
  `CMakeLists.txt` picks the host with `MTM_HOST`; `MTM_NO_LIFT` builds
  without the lift to check a toolchain. docs/android.md.
- The controller mapping is shared code (`src/pad/pad_map.c`): XInput on
  Windows, SDL game controllers and the TV remote on Android.

### Fixed
- Headless, a dialog that opens off the main window ("Before You Begin" opened
  on another monitor) is centred on it, so the recording shows it where it
  belongs.
- The game's `ChangeDisplaySettingsA`, fetched with `GetProcAddress`, no
  longer changes the real desktop's mode. It was switching it over and over
  during headless runs.
