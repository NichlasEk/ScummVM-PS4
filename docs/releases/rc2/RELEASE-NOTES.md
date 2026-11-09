# ScummVM PS4 2026.4.0 RC2

Install **ScummVM-PS4-2026.4.0-RC2.pkg**. No extra engine-data folder is needed.
The console application remains **ScummVM PS4** (title ID `SCVM00265`), with
internal APP_VER/VERSION **0.43**. Games and MT-32 ROMs are not included.

## Changes since RC1

- **The Longest Journey:** restored faces and clothing patterns. The graphics
  bridge now supports mirrored-repeat texture addressing, which the game's
  negative texture coordinates require.
- **Jönssonligan / Mjolner:** includes the committed fixes through 0.42 for
  helper-window identity, cursor teardown, refrigerator lists, installed voice
  folders, save discovery/replacement, linked AIFF padding and Swedish save names.
  Director compatibility remains experimental.

## Validation

The exact executable accepted by the maintainer in shadPS4 is packaged unchanged.
The opening TLJ cabin now displays faces and clothing details on all three actors.
The graphics frame/state/texture-cache/copy tests pass under ASan/UBSan. A fresh
source preparation matches all 161 patched files; PKG hashes/signatures pass and
all 241 extracted payloads match staging. These are selected-scene emulator and
package checks, not physical PS4 or full-playthrough validation.

## Installation and identity

Back up settings and saves before updating. Their paths remain
`/data/scummvm-2026-vulkan/scummvm.ini` and `/data/scummvm-2026-vulkan/saves/`.
The title/content IDs are unchanged from RC1; the internal version rises from
0.32 (RC1) and 0.42 (local candidates) to 0.43.

- Content ID: `IV0000-SCVM00265_00-SCUMMVM2026VK001`
- PKG size: 250,216,448 bytes
- PKG SHA-256: `9a3a74593a3112842403e6a16e42a57778173da24d1452bad1ad75c1f8ffbb3a`
- eboot.bin SHA-256: `cfb0008acb6c6f83e6f9dfa6341806853ec425a23b0d7613932c4c615d443f8b`

Prepared application source, dependency/runtime sources, notices and checksums
accompany this release. See SOURCES.md for build instructions and provenance
limits. The original RC1 release remains available.
