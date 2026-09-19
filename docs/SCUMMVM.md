# Real ScummVM port: emulator milestone, 2026-09-19

The actual ScummVM launcher boots in shadPS4 0.18.0. This is the PS4-targeted
SELF from the same staging directory used to produce its installable PKG.
It is not the diagnostic application or a native Linux ScummVM executable.

## Baseline and build

ScummVM 2.9.1, revision `9d9b9e93108a276c551aeffa390169ccc5148e15`.
SCUMM is enabled; optional engines, external codecs, networking integrations,
and desktop MIDI drivers are disabled for this first port. It uses the
built-in theme, so no compressed theme/font dependency is needed to boot.

The source is prepared under `.tools/scummvm-src`, with the tracked patch at
`patches/scummvm/ps4.patch`. The previous checkout remains untouched. Build
and packaging scripts are under `scripts/`. Packaging checks ELF imports to
reject accidental host dependencies. The SDK GP4 generator has a fixed sample
directory tree, so the packaging script derives the tree from actual files.

## PS4 adaptation

- Writable config, log and saves live at `/data/scummvm/`; first-launch config
  is seeded only when absent, preserving user changes thereafter.
- Window position is (0,0), sized to the actual display. The SDL surface backend
  presents a scaled software framebuffer and redraws the full backing image.
- Filesystem existence/readability checks use `stat`, because shadPS4's `access`
  stub falsely reported missing controller data as present and caused startup
  to abort. `/app0` is treated as read-only.
- Stick positions come from the already-open PS4 pad. The SDK SDL joystick
  implementation drops neutral events; explicit neutral positions stop cursor
  drift. Options/L3/R3/Touchpad use the PS4 button indices.
- PC mouse maps to the emulated left stick. F7 captures/releases movement;
  left/right button map to Cross/Circle. F8 reloads the profile. Amplification
  was raised from 1 to 3 after the user confirmed mouse operation but found it
  too slow. That final sensitivity preference still needs user assessment.

## Verified evidence

- Cross-compile and link succeeded; only expected PS4 shared-library imports.
- SELF creation, GP4 generation and PKG build succeeded.
- The actual ScummVM version header, Add Game and Global Options are visible.
- Keyboard-emulated sticks move the pointer. User confirmed interface operation
  and then confirmed PC mouse operation.
- The launcher records `ScummVM 2.9.1` and `Using joystick: Dualshock4` in its
  persistent log.
- Shell syntax, patch reverse-application checks and `git diff --check` passed.

Evidence: `artifacts/scummvm/launcher.png`, `launcher-mouse.png`,
`interactive.log`, `build.log`, `package.log`. Screenshots are native shadPS4
captures. The early startup failure is retained in `first-boot.log`.

Current artifacts:

- `build/scummvm-pkg/eboot.bin`
  SHA-256 `a9bd03b713742a3db9a82166620dcde38488115e6e1f5c56591398caa43c1849`
- `dist/IV0000-SCVM00002_00-SCUMMVMPS4000001.pkg`
  SHA-256 `a04ce151899df63aaadfdd3b6a9931b3050210a7ad75aa6283f0714af440a6fe`

Package timestamps may change the PKG hash when repackaging an identical ELF.

## Remaining work

No game data is bundled and no SCUMM gameplay/save round-trip has been verified.
Physical DualShock, PS4 package installation and hardware behavior are untested.
The emulator still logs harmless-looking stubs and missing optional GUI assets;
this milestone verifies startup and interaction, not complete compatibility.
Rotation and additional engines/codecs are outside the verified configuration.

Next: test a legally obtained SCUMM game/demo in the emulator, verify audio and
save/load, then try the same port on the user's PS4. Do not describe the full
port as finished until gameplay and hardware tests have passed.
