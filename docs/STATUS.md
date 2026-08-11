# Status

## 2026-08-11: clean baseline

The previous work in `/home/developer/ps4-scummvm` established two useful facts:

- ScummVM 2.9.1 can compile and link as a PS4-targeted x86-64 ELF.
- One generated package launched on firmware 9.60 with GoldHEN 2.4b17.3, but
  displayed a black screen. Later package variants regressed to "Cannot start
  application".

This repository starts with a smaller diagnostic application so packaging,
video, audio, input, and persistent logging can be tested independently.

Current title identity:

- Title ID: `SCVM00001`
- Content ID: `IV0000-SCVM00001_00-SCUMMVMDIAG00001`

The copied runtime modules are intentionally generated into `diagnostic/build`
from the installed OpenOrbis samples. They are not maintained as source files
in this repository.

## Gate for ScummVM integration

Do not add the upstream ScummVM tree until the diagnostic package both launches
and shows the video status bar on physical hardware. If it launches, capture
the other two status bars, audible tone, controller response, clean exit, and
the contents of `/data/scummvm-ps4-diagnostic.log`.

