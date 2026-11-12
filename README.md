# ScummVM for PlayStation 4

An unofficial native PS4 port of **ScummVM 2026.4.0**, built with OpenOrbis,
controller and USB mouse input, and a Vulkan/OpenGNM graphics backend.

**[Download RC2](https://github.com/NichlasEk/ScummVM-PS4/releases/tag/v2026.4.0-rc2)**

Install **ScummVM-PS4-2026.4.0-RC2.pkg**. The application appears as
**ScummVM PS4** on the console. No separate engine-data folder is required.
Games and MT-32 ROMs are not included; provide your own legally obtained files.

## Install and update

1. Install the PKG using your homebrew-enabled PS4's package installer.
2. Put each game's files in its own folder, for example
   `/data/scummvm/games/monkey/`, or on USB storage.
3. Open ScummVM, choose **Add Game**, and select that folder.
4. Leave the packaged support data in place. A custom **Extra Path** is optional;
   the application also finds its bundled engine data at `/app0/data`.

RC2 retains title ID **SCVM00265** and raises the internal application version
to **0.43**. Back up settings and saves before updating.
They remain at `/data/scummvm-2026-vulkan/scummvm.ini` and
`/data/scummvm-2026-vulkan/saves/`. Older port variants have different title IDs.

### Game folders and Add Game

For internal storage, put each game in its own folder under
`/data/scummvm/games/`, for example:

```text
/data/scummvm/games/monkey/
/data/scummvm/games/tlj/
```

Choose **Add Game → Internal storage – games → your game folder → Choose**.
For USB storage, game folders can be at the drive's root or inside subfolders;
select **USB 0** or **USB 1** in the file browser. If you upload games while
the browser is open, close and reopen it to refresh the listing.

You do not need to change **Extra Path** to add games. Leave it set to
`/app0/data` for the bundled support files. The **Bundled games** entry points
to `/app0/games` inside the application package, not your uploaded game folders.

### Reset settings and the game list

Close ScummVM, then rename the following file as a backup (or delete it):

```text
/data/scummvm-2026-vulkan/scummvm.ini
```

Restart ScummVM to recreate the default configuration with an empty game list.
This resets settings and registered games; it does not delete game files or
save files. Saves remain in `/data/scummvm-2026-vulkan/saves/`.

### Lure of the Temptress: outdated lure.dat

RC2 requires **lure.dat version 1.32** and already includes it in `/app0/data`.
An older copy in the game's folder can override the bundled file and cause:

```text
Incorrect version of the 'lure.dat' engine data file found. Expected 1.32 but got 1.29.
```

Rename or remove the outdated `lure.dat` from the **game folder**, so ScummVM
uses the bundled version. If you keep a separate copy in a custom **Extra Path**,
update it to version 1.32 or remove that outdated copy too. Keep the packaged
file in `/app0/data` intact.

This does not require a new RC2 package or a code fix. Reinstalling RC2 or
resetting `scummvm.ini` will not remove an old file left in a game folder.

## Controls

- Left stick: mouse pointer; hold R1 for finer movement.
- Cross: left click/select. Circle: right click.
- Options: ScummVM menu. Touchpad click: virtual keyboard.
- USB mouse input is enabled. Games can override controls; use the keymapper
  for engine-specific bindings.

## What RC2 contains

RC2 restores The Longest Journey's face and clothing textures and includes the
committed Jönssonligan/Mjolner fixes through 0.42 for dialogue audio, helper
windows and saves, including Swedish save names. RC1's packaged-data fallback,
MT-32 pacing, 2D filters and Grim/Monkey Island rendering fixes remain included.

The exact TLJ executable accepted in shadPS4 is packaged unchanged. Host graphics
regressions pass, clean source replay matches 161 patched files, and all 241
extracted PKG payloads match staging. Adding games from internal storage has
also been confirmed on a physical PS4 with the unchanged RC2 package. This
does not establish full gameplay compatibility or physical USB compatibility.

This is a **release candidate**, not a full-game compatibility guarantee.
The graphics bridge implements a subset of OpenGL. Performance, save/load and
later-game behavior vary by engine; PICE, SOTC and Director work remains
experimental. See [release notes](docs/releases/rc2/RELEASE-NOTES.md).

## Source and building

This repository contains the port, patches, tests and build recipes. The release
also includes prepared ScummVM source, patched dependency sources, runtime source
supplement, notices and checksums. Read [SOURCES.md](docs/releases/rc2/SOURCES.md)
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
