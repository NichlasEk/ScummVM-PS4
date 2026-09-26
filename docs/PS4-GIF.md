# GIF on PS4

The build uses a pinned, SHA-256-verified giflib 6.1.3 decoder archive.
`build-ps4-gif.sh` builds the decoder and allocation helpers for Orbis;
the shared ScummVM configuration requires `USE_GIF`. Packages include the
giflib license.

The ScummVM GIF decoder patch handles interlaced rows, local palettes,
transparent entries and frame offsets. `test-ps4-gif-build.sh` prepares and
checks fixtures; `run-ps4-gif-smoke.sh` runs the PS4 decoder probe in shadPS4.

The recorded native test passed palette, interlaced, local-palette, transparent
and offset cases (3,072 samples each), and rejected truncated and invalid input.
Evidence: `artifacts/emulator/gif-smoke-result.txt`. This verifies the decoder,
not HPL1 gameplay or physical PS4 hardware.
