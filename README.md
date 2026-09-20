# ScummVM for PlayStation 4

ScummVM 2.9.1 with the SCUMM engine (including v7/v8), cross-compiled for PS4 and boot-tested
in shadPS4 0.18.0. The real launcher is visible and the pointer can be
controlled with keyboard-emulated sticks or a PC mouse. The user has confirmed DOTT running on a real PS4. The latest pointer/storage
changes still need hardware testing; save/load validation remains pending.

## Install on PS4 and add games

The default package contains **no games**. It has the ScummVM icon, a 8× faster
PS4 pointer, and storage shortcuts for USB and internal game directories.
See [standalone install and game setup](docs/STANDALONE-PS4.md).

## Run ScummVM in the emulator

```sh
./scripts/run-emulator.sh
```

The script prepares pinned ScummVM sources, applies the PS4 patch, builds the
PS4 executable and PKG, installs the verified emulator release if needed,
and boots ScummVM. The initial build takes several minutes; later builds
are incremental. No session timeout by default. For a bounded test, use
`EMULATOR_TIMEOUT=120 ./scripts/run-emulator.sh`.

- **F7**: capture/release PC mouse movement.
- **Left/right mouse button**: left/right click.
- **WASD**: left stick / pointer when mouse capture is off.
- **N/B**: Cross/Circle, corresponding to left/right click in the launcher.
- **F8**: reload input bindings after changing sensitivity.
- **F12**: save a screenshot.

Mouse amplification is **3×**. Adjust `mouse_movement_params` in
`.tools/xdg/shadPS4/input_config/SCVM00002.ini`; the middle value controls
amplification. Existing bindings are preserved on subsequent launches.

The list is initially empty because no commercial game data is bundled.
The emulator mounts `build/scummvm-pkg` as `/app0`; game folders can be put
under `build/scummvm-pkg/games` and selected with **Add Game**. This does not
add them to the hardware PKG automatically.

## Build and artifacts

Requirements: the OpenOrbis SDK with its SDL port at
`/opt/openorbis/OpenOrbis/PS4Toolchain`, host `clang++`, `ld.lld`, LLVM archive
tools, `make`, Python 3, Git, curl, and unzip. Set `OO_PS4_TOOLCHAIN` to use
another installation with the same SDK layout.

```sh
./scripts/build-scummvm.sh
```

- Package: `dist/ScummVM-PS4-2.9.1.pkg`
- Guest executable: `build/scummvm-pkg/eboot.bin`
- Guest settings/saves/logs: `.tools/xdg/shadPS4/data/scummvm/`
- Build logs/evidence: `artifacts/scummvm/`
- Emulator run logs: `artifacts/emulator/`
- Screenshots: `.tools/xdg/shadPS4/screenshots/`

Source preparation uses the pinned clean revision from the earlier checkout
at `/home/developer/ps4-scummvm/scummvm` when available, otherwise GitHub. The
old checkout is read-only reference material. The portable changes live in
`patches/scummvm/ps4.patch`; generated sources/builds stay ignored by Git.

See [ScummVM validation and remaining work](docs/SCUMMVM.md).

## Bundle your DOTT CD data for a PS4 test

```sh
./scripts/build-scummvm.sh
./scripts/package-scummvm.sh --with-dott /path/to/dott-data
```

The directory must contain `tentacle.000`, `tentacle.001`, and `monster.sou`.
The result is `dist/ScummVM-PS4-2.9.1-DOTT.pkg`, with DOTT preselected on a
fresh installation. Existing settings and saves are preserved. See
[DOTT hardware-test package](docs/DOTT-PS4-TEST.md) for verification and setup.

## Diagnostic app

The smaller hardware diagnostic is still available separately:

```sh
./scripts/run-emulator.sh --diagnostic
./scripts/build-diagnostic.sh
```

Its package is under `diagnostic/dist-openorbis/`, title ID `SCVM00001`.
Three green bars indicate initialized video/audio/controller. Cross changes
the background and plays a tone; Circle exits. On hardware the log is
`/data/scummvm-ps4-diagnostic.log`. See [diagnostic evidence](docs/EMULATOR.md).

Project code is GPL-3.0-or-later; generated packages also include separately
licensed OpenOrbis/SDL components. ScummVM retains its upstream license notices.
