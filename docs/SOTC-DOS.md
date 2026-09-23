# Shadow of the Comet: initial DOS investigation

Inspected 2026-09-23. A separate resource-preview engine now exists; this is not a playable game.

## Local evidence

The user-supplied European multilingual CD is a single MODE1/2352 track,
ISO9660 volume SHADOWCD. Main-game files are in SHADOW/, with SHADOW.EXE
at the root. MUSEE/ is separate bonus content; DEMO/INDARK2/ must not be
mistaken for the main game.

The original disc is unchanged. Extracted files reside in the ignored
build/sotc-DOS/disc-files/ directory. No game resources belong in the engine
or a distributed package.

| Area | PICE DOS | Observed SOTC DOS |
| --- | --- | --- |
| Archives | KRO, Burp directory, headerless zlib path | 23 PAK archives, little-endian offset tables |
| Visual assets | PICE sprite/resource decoding | A00–A09.PAK animations, D00–D09.PAK scenery; VA2 and RAW names |
| Text and scripts | PICE resource/script machinery | R00–R09.CC4 and five additional CC4 files; semantics unverified |
| Font | PICE resource fonts | CTU.PFN inside RES.PAK |
| Sound | PICE voice/music resources | VOC names in SMP.PAK, ADD names in MUS.PAK, separate NAR banks |

All 23 main-game PAK directories have been parsed with resource bounds checked.
Example: D00.PAK contains F00.RAW with a declared unpacked size of 64000 bytes,
now verified as a 320×200 indexed background. The original CTU.PAL palette
renders it in the PS4 viewer; scripted palette changes are still unimplemented. A00.PAK includes STUBBS0.VA2; MUS.PAK includes
SHADOW1.ADD; SMP.PAK includes C27.VOC.

PAK entry headers contain an additional-descriptor size, packed and unpacked
sizes, method byte, flags byte, metadata length, metadata and payload.
Observed methods are 0 and 1, with flags 0 or 6.
This agrees with the layout in
[FITD's PAK reader](https://github.com/jmimu/AITD_PakEdit/blob/main/fitd/pak.cpp),
which dispatches method 1 to PAK_explode. The new bounded decoder has now decoded all 884 entries across the 23 archives;
all compressed entries match both FITD and Info-ZIP byte for byte. ZIP implode
must not be confused with PKWARE DCL or PICE's headerless zlib path.

Reproduce the directory audit:

```sh
python3 scripts/sotc/inspect-pak.py build/sotc-DOS/disc-files/SHADOW > artifacts/sotc/pak-index.json
```

## Recommended engine boundary

Use a separate sotc engine. Reuse ScummVM graphics, mixer, input, storage and
the existing PS4 build infrastructure. Reuse specific low-level decoders only
after format verification; do not assume PICE opcodes, navigation, sprites or
save states apply. Shared archive conventions do not prove a shared script VM.

Implementation milestones:

1. Bounds-checked PAK reader and decompression, validated against original
   images, font and sound; decode palettes and VA2 animation frames.
2. Separate detector and resource-viewer engine showing one original room.
   Give it isolated build outputs, configuration and save files.
3. Map CC4 scripts, text encoding/language tables, actors and navigation;
   reproduce one interactive scene before expanding coverage.
4. Implement music, speech, menus and saves; then test progression on the
   actual PS4 build under shadPS4 and on hardware.

Not established: CC4 opcode meanings, language-file mapping, complete NAR
layout, ADD music decoding, animation semantics, or later-chapter behavior.

## Resource-preview engine

`engines/sotc/` contains a separate ScummVM detector, bounds-checked PAK reader,
ZIP-implode decoder and background viewer. The decoder handles both observed
flag variants (0 and 6); invalid flags and truncated streams are rejected.
The preview loads original resources at runtime, including the four palettes
from RES.PAK. Palette selection is manual: CC4-driven palette changes are not
implemented. No character animation, scripts, audio playback or saves yet.

```sh
scripts/sotc/build.sh
scripts/sotc/run-emulator.sh build/sotc-DOS/disc-files/SHADOW
python3 scripts/sotc/audit-resources.py build/sotc-DOS/disc-files/SHADOW > artifacts/sotc/resource-audit.json
```

The audit compiles the actual engine decoder and compares each compressed entry
with the system `unzip` decoder. It also checks truncated input and unsupported
flags. Reports and extracted resources remain ignored.

- Left/right or D-pad left/right: previous/next background.
- Up/down or D-pad up/down: next/previous D00–D09 scenery archive.
- P on a native desktop, or Square (V in the emulator profile): next palette.
- Circle/right click (B in the emulator profile): previous background.
- Touchpad (Space in the emulator profile), or Escape on native desktop: exit viewer.
- Desktop left click advances and right click goes back; F7 releases mouse capture.

PS4 title ID: SCVM00004. Build/source/stage directories use `scummvm-sotc`;
package: `dist/sotc-experimental/ScummVM-PS4-SOTC-preview.pkg`.
The emulator uses `.tools/xdg-sotc` and separate config/cache directories.
On USB, place original files in `scummvm/games/sotc/SHADOW/` (or directly in
`scummvm/games/sotc/`). Without those files the package opens the launcher.
The package contains engine code and ScummVM support files, not game data.

Validation: PS4 package built and booted in shadPS4 as SCVM00004. Original
F00.RAW was captured on screen; click-forward/back and palette switching were
exercised, and the user confirmed graphics work. Right click initially mapped
to Exit; it now maps to previous image. The viewer contains 155 full-size RAW
backgrounds across D00–D09. Physical PS4 hardware has not been tested yet.
