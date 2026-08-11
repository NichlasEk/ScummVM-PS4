# ScummVM for PlayStation 4

Clean continuation of the earlier experiments in
`/home/developer/ps4-scummvm`. The old directory is reference material and is
not modified by this project.

The first milestone is deliberately smaller than ScummVM: prove the complete
OpenOrbis path on real hardware with visible graphics, audio, controller input,
persistent logging, a valid `eboot.bin`, and an installable PKG.

## Build the diagnostic PKG

Requirements:

- OpenOrbis with SDL2, currently expected at `/opt/pacbrew/ps4/openorbis`
- `make`

Build and verify:

```sh
./scripts/build-diagnostic.sh
```

Override the toolchain location when needed:

```sh
OO_PS4_TOOLCHAIN=/path/to/openorbis ./scripts/build-diagnostic.sh
```

The resulting package is written to:

```text
diagnostic/dist/IV0000-SCVM00001_00-SCUMMVMDIAG00001.pkg
```

## Test on PS4

Install the package on a homebrew-enabled PS4 and launch **ScummVM PS4
Diagnostic**.

- Three green horizontal bars: video, audio, and controller initialized.
- A red middle bar: audio initialization failed.
- A red lower bar: no controller was opened.
- A short tone plays at startup.
- Cross changes the background and restarts the tone.
- Circle exits cleanly.
- Startup and input events are written to
  `/data/scummvm-ps4-diagnostic.log`.

Record whether the app launches, the bar colors, whether the tone is audible,
whether Cross changes the background, and whether Circle exits. That result
determines which subsystem to fix before integrating ScummVM.

## Project stages

1. Boot and validate the diagnostic PKG on physical hardware.
2. Add a minimal ScummVM checkout with only the SCUMM engine.
3. Introduce a PS4 platform layer for paths, saves, lifecycle, and controller
   pointer mapping.
4. Package a freeware ScummVM demo and validate gameplay and saves.
5. Expand engines and optional codecs incrementally.

Project-specific code is licensed under GPL-3.0-or-later to remain compatible
with upstream ScummVM. Generated packages also contain separately licensed
OpenOrbis and SDL components supplied by the installed toolchain.
