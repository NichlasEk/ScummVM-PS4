# ScummVM for PlayStation 4

An unofficial native PS4 port of **ScummVM 2026.4.0**, built with OpenOrbis,
controller and USB mouse input, and a Vulkan/OpenGNM graphics backend.

**[Download RC1](https://github.com/NichlasEk/ScummVM-PS4/releases/tag/v2026.4.0-rc1)**

Install **ScummVM-PS4-2026.4.0-RC1.pkg**. The application appears as
**ScummVM PS4** on the console. No separate engine-data folder is required.
Games and MT-32 ROMs are not included; provide your own legally obtained files.

## Install and update

1. Install the PKG using your homebrew-enabled PS4's package installer.
2. Put each game's files in its own folder, for example
   `/data/scummvm/games/monkey/`, or on USB storage.
3. Open ScummVM, choose **Add Game**, and select that folder.
4. Leave the packaged support data in place. A custom **Extra Path** is optional;
   the application also finds its bundled engine data at `/app0/data`.

RC1 retains title ID **SCVM00265** and internal application version **0.32**.
It is the 0.32 test package with only the console display name changed; it is
not a new program build. If 0.32 is already installed, your package installer
may report the same version. Back up settings and saves before replacing it.
They remain at `/data/scummvm-2026-vulkan/scummvm.ini` and
`/data/scummvm-2026-vulkan/saves/`. Older port variants have different title IDs.

## Controls

- Left stick: mouse pointer; hold R1 for finer movement.
- Cross: left click/select. Circle: right click.
- Options: ScummVM menu. Touchpad click: virtual keyboard.
- USB mouse input is enabled. Games can override controls; use the keymapper
  for engine-specific bindings.

## What RC1 contains

The community-tested 0.32 program includes the packaged-data fallback,
MT-32 audio pacing correction, selectable 2D filters, Grim/Monkey Island
rendering fixes and Jönssonligan compatibility work. The maintainer reports no
new community bug reports over the recent test period.

The renamed package passes digest/signature validation. All 241 packaged files
were compared against the original: only `sce_sys/param.sfo` differs, and only
its `TITLE` entry changed. Host regression tests cover graphics resource reuse,
frame ownership, Director input handling and the shader catalog. Physical
installation/start of the **renamed package** has not yet been confirmed.

This is a **release candidate**, not a full-game compatibility guarantee.
The graphics bridge implements a subset of OpenGL. Performance, save/load and
later-game behavior vary by engine; PICE, SOTC and Director work remains
experimental. See [release notes](docs/releases/rc1/RELEASE-NOTES.md).

## Source and building

This repository contains the port, patches, tests and build recipes. The release
also includes prepared ScummVM source, patched dependency sources, runtime source
supplement, notices and checksums. Read [SOURCES.md](docs/releases/rc1/SOURCES.md)
for the build order and provenance limitations. A fresh-machine or bit-identical
rebuild has not been established.

Older experiment reports are retained in `docs/` as historical evidence; their
versions and local artifact paths do not identify the current download.

## Report a problem

[Open an issue](https://github.com/NichlasEk/ScummVM-PS4/issues) with the RC version,
PS4 firmware, game edition/language, steps to reproduce and the relevant log
excerpt. Remove personal paths or other private information before posting.
Do not attach commercial game files or MT-32 ROMs.

ScummVM is developed by the [ScummVM project](https://www.scummvm.org/).
This community PS4 port is not an official ScummVM release. See `LICENSE`,
upstream copyright notices and the release's component notices.
