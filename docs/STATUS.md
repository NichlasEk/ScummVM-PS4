# Status

## 2026-09-19: physical PS4 confirmed; standalone revision

The user confirms that the DOTT bundle runs on their real PS4. Their feedback
was slow pointer movement. Version 1.01 multiplies virtual pointer speed by
four, replaces the SDK icon with the upstream ScummVM icon, creates an internal
game directory and presents USB/internal storage in the root browser. The
default package excludes all game data. Hardware retesting of these changes
and save/load/audio checks remain pending. See [standalone setup](STANDALONE-PS4.md).

## 2026-09-19: DOTT bundled for hardware testing

The user demonstrated the DOTT intro running in shadPS4. A local PKG now
bundles their full CD game data and a first-launch game entry. Package
signatures/digests and extracted contents were verified, and the extracted
executable boots with DOTT preselected. Hardware test remains pending.
See [DOTT-PS4-TEST.md](DOTT-PS4-TEST.md) for the artifact and SHA-256.


## 2026-09-19: ScummVM launcher boots in shadPS4

The user clarified that the milestone is the real ScummVM application in the
emulator. This supersedes the original requirement to wait for physical
hardware before integrating ScummVM. ScummVM 2.9.1 now builds as a PS4 SELF
and PKG and displays its launcher in shadPS4. PC mouse mapping is installed
with 3x amplification. See [SCUMMVM.md](SCUMMVM.md) for evidence and next work.

`./scripts/run-emulator.sh` now starts ScummVM. Use `--diagnostic` for the bars.


## 2026-09-19: emulator diagnostic passed

shadPS4 0.18.0 boots the diagnostic with visible green video/audio/controller
bars. Cross changes the background and plays the verified tone; Circle reaches
guest shutdown. See [EMULATOR.md](EMULATOR.md) for evidence, build changes,
artifact hashes, and remaining emulator limitations.

Use `./scripts/run-emulator.sh --diagnostic`. Diagnostic packages are under
`diagnostic/dist-openorbis/`. Physical PS4 validation is still pending.

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

## Original hardware gate (superseded for emulator integration)

Do not add the upstream ScummVM tree until the diagnostic package both launches
and shows the video status bar on physical hardware. If it launches, capture
the other two status bars, audible tone, controller response, clean exit, and
the contents of `/data/scummvm-ps4-diagnostic.log`.
