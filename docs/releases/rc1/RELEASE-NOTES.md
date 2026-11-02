# ScummVM PS4 2026.4.0 RC1

Download `ScummVM-PS4-2026.4.0-RC1.pkg` and install it with your PS4 homebrew
package installer. The console name is **ScummVM PS4**.

## Identity

- Title ID: `SCVM00265`
- Content ID: `IV0000-SCVM00265_00-SCUMMVM2026VK001`
- Internal APP_VER/VERSION: `0.32`
- Size: 250,216,448 bytes
- SHA-256: `51942ffe2384da7872c4381c7d21f06db17bf50d18066a596fef8168373a42e8`

RC1 preserves the program and resources of community-tested 0.32. Only the
package's SFO TITLE changes from “ScummVM 2026.4.0 Vulkan Test” to “ScummVM PS4”.
The original 0.32 package hash is
`9097e7e16cf9b3eea1d57b2ec7e0a3f3c6485dd73b7c37ace3d47fd868c9734e`.
The program (eboot.bin) hash is unchanged:
`03942cc9b742f52fcd11bb479ecc5affbf906702274be0a6a33468e5c67e9a34`.

## Included changes

- Bundled engine data is found at `/app0/data` even with a custom Extra Path.
  No manual PKG extraction or extra data download is required.
- Corrected SDL audio pacing for MT-32 synthesis.
- Thirty selectable 2D filter presets and Vulkan upload-memory reuse.
- Grim save-screen/readback and Escape from Monkey Island rendering fixes.
- Director/Jönssonligan audio, video and input-service improvements.

Supply your own games and MT-32 ROMs. Settings and saves remain in
`/data/scummvm-2026-vulkan/`. The package uses the same title and version as 0.32;
installers may report an already installed version. Back up saves/configuration
before replacing an installation.

## Validation and limits

PKG hashes/signatures pass; all 241 files match staging. Compared with original
0.32, exactly one payload and one SFO field changed. The prepared-source patch
replay matches all 156 modified files. Host tests exercise resource reuse,
frame ownership, Director input and all 30 filter presets.

The maintainer identifies 0.32 as the latest community-tested program and
reports no recent new bug reports. The renamed PKG itself has not yet received
a physical installation/start confirmation. Host and emulator checks do not
establish physical-console behavior or complete-game compatibility.

The graphics bridge is incomplete; individual engines can have graphical,
performance or save/load limitations. PICE, SOTC and Director remain
experimental. Existing evidence for games generally covers selected scenes,
not full playthroughs. No new gameplay behavior is claimed for the rename.

Prepared source and dependency/runtime sources accompany this release. See
SOURCES.md for build instructions and the explicitly inferred musl provenance.
