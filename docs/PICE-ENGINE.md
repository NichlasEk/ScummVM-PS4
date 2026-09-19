# Prisoner of Ice engine development

## Objective and current limits

The goal is a native ScummVM engine for Prisoner of Ice, using the PS4 build
in shadPS4 as the first runtime testbench. This is **not a playable engine yet**.
The first implementation is a resource preview: archive reader, indexed image
and palette decoder, original WAV playback, and input to browse resources.
It does not run the original EXE or use DOSBox. It does not implement scripts,
scene logic, character animation, hotspots, inventory, puzzles, movies or saves.

The user remembered an open engine but subsequently reported that its author
had taken the work down. No usable full reimplementation has been located.
ScummVM's development compatibility list checked 2026-09-19 does not list the
game. Upstream source-tree revision checked:
`8bb23229ebd7edc8691b04b25b01c82cca90d3a0`.

## Source and testbench

- Maintained engine: `engines/pice/` (GPL-3.0-or-later).
- `scripts/pice/build.sh`: isolated ScummVM 2.9.1 PS4 build, PICE engine only.
- `scripts/pice/run-emulator.sh [EU_CD_DIRECTORY]`: runs it with original
  game data externally mounted at `/mnt/usb0`. Defaults to the user-provided
  `build/scummvm-dott-pkg/games/pice/cd-eur` directory.
- `scripts/pice/audit-resources.py`: independent Python/zlib format audit.
- Source/build/stage: `.tools/scummvm-pice-src`, `build/scummvm-pice`,
  `build/scummvm-pice-pkg`.
- Title ID: SCVM00003, separate from the stable SCVM00002 port.
- Output: `dist/pice-experimental/ScummVM-PS4-PICE-preview.pkg`.
- Emulator profile: `.tools/xdg-pice/shadPS4`; evidence: `artifacts/pice/`.

The experimental binary auto-starts the preview using `/mnt/usb0`. That mount
is supplied by the test script and is not a general console release flow.
The maintained PS4 patch and ABI correction also apply to the isolated build.
Game data and decoded assets are never committed or included in the package.

## Formats confirmed from the supplied files

KRO header: ASCII `Burp`, followed by little-endian uint32 resource count.
Each directory entry is 20 bytes: unpacked size, packed size, reserved zero,
payload offset, flags. Flag 0 is stored, flag 4 is raw Deflate. Entries in the
supplied archives are in offset order. Validate counts, ranges, overlap, flags,
allocation limits and decoded lengths before using data.

The independent audit decoded all 3,743 resources in four European archives:
KSVGA 1,338; KVGA 1,335; E_KLANG 717; KSOUND 353. Some entries are empty or
contain dummy placeholders. Original resource IDs are zero-based.

Simple indexed images: little-endian uint16 width/height followed by exactly
width*height bytes. A neighboring resource often contains 768 RGB palette
bytes. SVGA scene backgrounds are commonly 640x316 or 640x317. Resource 815
is the first simple SVGA image picked by the preview; 816 is its palette.

Many E_KLANG resources are standard 8-bit mono PCM WAV at 22050 Hz. Some later
resources start with an `EDITLS` header; those wrappers are not decoded yet.
The preview currently targets the supplied English European CD using exact
MD5-prefix and file-size detection. US/language variants need explicit tests.

A separate animation-like format starts with six uint32 values, commonly
beginning with 24; its meaning and frame codec still require investigation.
The supplied ICE.PCK is a 154-byte options file, not the main game program.
Movies use a .MUX container which has not yet been decoded.

## Reused code and references

ScummVM already provides Deflate, WAV playback, graphics, events and file I/O;
the new engine uses those APIs directly. No GameExtractor source was copied.
Its KRO reader independently confirmed the archive interpretation after the
initial local parse:
https://github.com/wattostudios/GameExtractor/blob/1d3201e2fff3ba3a7fd0f72b12cb9d0738a89e6a/src/org/watto/ge/plugin/archive/Plugin_KRO_BURP.java

Compatibility source: https://www.scummvm.org/compatibility/

## Next work toward an actual engine

1. Completed: visually and hash-verified PS4 resource playback baseline (see below).
2. Decode animation resources and MUX movies with bounds checks and fixtures.
3. Locate scene/script tables and reverse engineer opcode semantics from the
   original executables and data. Record evidence for each opcode; do not
   guess gameplay behavior or disguise hardcoded scenes as an interpreter.
4. Implement scheduler, room transitions, hotspots, character movement,
   inventory and dialogue. Validate the opening playable sequence on PS4.
5. Add state serialization and save/load, then extend through the full game
   and other language editions. A full playthrough is required before calling
   the game supported.

## Intervening PS4 regression

The user reported jerky 8x input and a CE-34878-0 crash while adding a game.
This is being handled first in the regular port, documented in
[PS4-1.02-REGRESSION.md](PS4-1.02-REGRESSION.md). PICE detection exposed a
confirmed SDK stat ABI error: a 45,874,014-byte KRO was reported as 89,598 bytes
(block count). Correcting mode_t to 16 bits restores the kernel stat layout.

## Verified preview baseline (2026-09-19)

The isolated PS4 binary boots in shadPS4, passes parser self-tests, detects the
English European CD with correct file sizes and displays original background
815. Its pixel FNV is 2881fe4d, palette 816 FNV fc170f55, and speech resource 0
FNV 9d300c17 (27,419 bytes, 22,050 Hz); these match the independent Python
oracle. Screenshot: artifacts/pice/first-background.png. Runtime log:
artifacts/pice/emulator.log. Audio decoding/mixer submission is verified;
there is no audio capture confirming audible playback. This remains a resource
preview with no game interpreter.

## Sprite/sequence checkpoint

The independent scripts/pice/audit-sprites.py validates 24,535 RLE sprites
in 734 KSVGA resources; 15 different header variants are explicitly excluded.
The new PS4 sprite decoder passed its synthetic tests in shadPS4 and resource
0 bitmap 0 produced pixel FNV 1a3dd53e, mask FNV b6a10fcc, palette 4f3aa5cc,
matching the host oracle. The resource has 104 composition frames and 16
sequences; the preview selects sequence 8 with 12 steps.

Per bitmap: uint16 LE width/height. Each row starts with a byte span count;
each span has skip, groups-of-four, remainder (0..3), then literal pixels.
A final byte skips the rest of the row. A zero uint16 tail is optional.
Transparency is a separate mask; color index 0 may be opaque.

Frame records: two-byte header then 8-byte component records. Sequence records
also have a two-byte header and 8-byte steps referencing composition frames.
The preview currently accepts component flag pair (1,1) only. Signed offsets
and duration fields are parsed; their complete semantics still need EXE
confirmation. Playback uses a provisional 20 ms per duration unit. This does
not establish original timing, anchor semantics, or gameplay support.

The first frame's guest hashes are verified. Two shadPS4 screenshots now
confirm visible animation progression in sequence 8 (steps 7 and 11):
`artifacts/pice/animation-step-07.png` and `animation-step-11.png`.
These local evidence files contain original game imagery and are not tracked.

The guest parser self-tests now also construct an original synthetic complete
sprite container, test every truncated prefix, corrupt header/table fields,
check invalid image/frame references and signed offsets, and verify that failed
loads clear previously loaded state. Correctly sized empty sequences are
rejected, avoiding modulo-zero/indexing when cycling animation sequences.
This covers the supported container variant, not all observed variants.

## Executable mapping checkpoint (2026-09-19)

`scripts/pice/inspect-executable.py` finds the embedded MZ/LE executable through
its relative e_lfanew pointer and maps LE object pages to physical file offsets.
It outputs JSON metadata and hashes only, never executable bytes. Its original
synthetic tests cover embedded headers, reversed physical pages, truncated
files, invalid tables/entry points and unsupported page encoding:

```sh
python3 tests/pice/test_executable.py
python3 scripts/pice/inspect-executable.py /path/to/ICE640.EXE
```

Both local European ICE320.EXE and ICE640.EXE parse successfully. ICE640 has
embedded MZ at 0x25d24, LE at 0x285dc, and physical page data at 0x3b524.
There are 81 pages and three objects: 16-bit executable, 32-bit executable,
and 32-bit writable data. The entry is object 2 offset 0x24db0, physical
file offset 0x6a2d4. The stored page end matches the file size exactly.
Inspection at this entry finds a jump over Watcom runtime identification into
startup code; this validates the mapping, not discovery of the game loop.

Format reference: [Open Watcom LE/LX definitions](https://github.com/open-watcom/open-watcom-v2/blob/master/bld/watcom/h/exeflat.h).
The inspector is independently written and does not apply relocations/fixups.
Next: use these mappings to trace archive readers and identify room/script
structures. No opcode semantics or playable scene have been established yet.
