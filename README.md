# ScummVM for PlayStation 4

ScummVM 2.9.1 with all engines supported by the current PS4 build dependencies,
including SCUMM v7/v8 and Humongous Entertainment, cross-compiled for PS4 and boot-tested
in shadPS4 0.18.0. The real launcher is visible and the pointer can be
controlled with keyboard-emulated sticks or a PC mouse. The user has confirmed DOTT running on a real PS4. The latest pointer/storage
changes still need hardware testing; save/load validation remains pending.

## ScummVM 2026.4.0 hardware-renderer tests

Startup checks in shadPS4 0.18.0 (2026-10-15), using the experimental Vulkan
port. Checked boxes cover only the behavior described, not full-game or physical
PS4 compatibility. See [fixes and test evidence](experiments/scummvm-2026.4.0/README.md#3d-engine-validation-2026-10-15).

- [x] Myst III: Exile: menu, New Game, intro and opening panorama; user-confirmed
  after framebuffer-copy and vertex-offset fixes. Save/load and later areas pending.
- [x] Grim Fandango: correctly scaled intro and first office scene; user-confirmed
  movement after D-pad mapping and vertex-offset fixes. Offscreen depth/stencil,
  later areas and save/load remain unverified or incomplete.
- [x] The Longest Journey: menu and opening cabin dialogue render in shadPS4;
  missing actor parts restored by the unbound-texture fix. Gameplay, save/load
  and performance validation remain pending.

Additional local startup checks with the same build (details and evidence in the
[experiment notes](experiments/scummvm-2026.4.0/README.md#additional-local-game-visual-checks-2026-10-15)):

- [x] Day of the Tentacle: animated intro visible; gameplay not verified.
- [x] Broken Sword 2.5: menu, pointer and confirmation dialog visible; gameplay pending.
- [x] Buried in Time: logos, title animation and credits visible.
- [x] Prisoner of Ice: startup movie visible; first room and skipping pending.
- [x] Shadow of the Comet: intro pictures and dialogue visible.
- [x] Thimbleweed Park: color-only FBO support enabled in candidate 0.06; tutorial visible and user-confirmed running in shadPS4. Physical PS4 validation pending.
- [ ] Penumbra: Overture: intro/settings visible, but scene startup crashes in `NewtonCreateBox`.

These are visual startup checks, not full-game compatibility results.

## Combined build with the local engines

The experimental Vulkan build includes our Prisoner of Ice and Shadow of the
Comet engines, the Jönssonligan Director 7 extension, Watchmaker/Penumbra work,
and FLAC, VP8/VP9, AC-3 and MT-32 support. See [full build, activation and
validation limits](docs/FULL-BUILD.md) for the build commands and per-game notes.

## Install on PS4 and add games

The default package contains **no games**. It has the ScummVM icon, a 8× faster
PS4 pointer, and storage shortcuts for USB and internal game directories.
See [standalone install and game setup](docs/STANDALONE-PS4.md).
See [compiled engines and limitations](docs/ENGINE-SUPPORT.md) for the engine
inventory. Compiled support is not a full PS4 gameplay compatibility claim.

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
External prebuilt PS4 modules are not included in Git; see the
[module placeholders and setup instructions](external/ps4-runtime/README.md).
