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
implemented. Frame previews are supported; game-driven animation sequences, scripts, audio
playback and saves are not yet implemented.

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

## VA2 frame preview

The viewer now overlays figures from A00–A09 on the selected background.
Original indexed pixels and an explicit transparency mask preserve opaque
palette index zero. The base position is a preview anchor, not scene-derived. Sequence movement is
now applied relative to that anchor (see executable findings below).
Triangle (C in the emulator) toggles the overlay; R1/U selects the next figure
and L1/Q selects the previous one. Background and palette controls are unchanged.

Frames advance in resource order every 150 ms for inspection. This is **not**
the original animation-sequence interpreter: sequence selection, timing,
placement and actor state still require reverse engineering.

The VA2 header has four little-endian section offsets. The second section has
32-bit relative frame offsets and a two-byte sentinel. Supported frame records
start with width in 16-pixel units and byte height. Each scanline holds a run
count followed by transparent skips and literal runs (four-byte groups plus
0–3 remaining pixels), then a final skip to the padded width. Observed optional
trailers are 0000 and 0080; their semantics remain unknown. Exact 0000 frames
are empty frames used by hidden/dummy objects.

```sh
python3 scripts/sotc/audit-va2.py build/sotc-DOS/disc-files/SHADOW > artifacts/sotc/va2-audit.json
```

The audit uses the engine's actual C++ decoder directly from original archives.
All 615 VA2 resource directories parse, containing 6220 frame records. 5422
records decode, including explicit empty frames; 798 remain unsupported, with
many larger images apparently using another compression layer. These are shown
as unsupported rather than displaying partial output. Tests cover transparent
skips, opaque colour zero, empty frames, truncated input and oversized runs.

Validation: rebuilt the PS4 package, booted the overlay in shadPS4, confirmed
figure selection in the log, and received user confirmation of the rendering.
No original resources are included in the source or package.

## Sequence and pose preview

The viewer now follows section 3 sequence records into section 1 poses, and
composites supported pose layers instead of cycling through unrelated frames.
L3/X selects the previous sequence and R3/M the next. A resource's first
multi-step sequence is selected automatically. Resources with empty sequence
records retain raw-frame browsing.

Sequence records have a header byte followed by an 8-bit entry count, then
8-byte steps: little-endian pose reference, a render-control word, and signed
X/Y movement fields. Pose records use an analogous header and
8-byte layer descriptors. The supported layer mode is 01 01, followed by a
frame reference (high bit treated as horizontal mirroring) and signed X/Y
placement. Other layer modes and additional record payloads are rejected as
unsupported, without partially drawing a pose. Empty poses are valid.

Placement is bottom-relative around a fixed preview anchor, inferred from the
observed offsets. This still needs original-runtime comparison. The viewer
**still uses 150 ms per step**: original cadence, special render modes,
script-selected sequences and loop rules are not yet implemented. Movement
is now verified against the executable and applied. Preview sequences loop
for inspection only, resetting their displacement each cycle.

The extended `audit-va2.py` checks sequence boundaries, pose references and
out-of-range steps, plus known Stubbs walking sequences and mirrored poses.
Across 615 resources it reads 3229 sequence records and 23096 steps. 16 steps
reference poses outside the available table and remain unsupported; these may
need special semantics or additional resource context. Frame decoding coverage
remains 5422/6220. Decoding a sequence record does not imply every pose/frame
it references is supported.

PS4 validation: rebuilt SCVM00004, booted in shadPS4, captured Stubbs walking
sequence 5, and verified the R3/M input changes sequence 4 to 5. Screenshot:
`artifacts/sotc/ps4-sequence.png`; log: `artifacts/sotc/sequence-live.log`.
The preview is not yet an interactive game or an original-speed animation VM.

## Executable-backed movement and control-word correction

Inspected the user's European DOS SHADOW.EXE (SHA256
`26d6fc54a27e0bf17ea34a355aa9bf087c8f13dfb5d613d5d1d535190da9ab5b`).
The MZ load image begins at file offset 0x1c00. Offsets below are relative to
that load image, **not** file offsets or relocated runtime addresses.

- 0x6e4c–0x6ec2: lookup via the VA2 section table followed by its record table.
- 0x1662–0x166b: sequence count is read as a byte at record +1; the earlier
  big-endian-word interpretation was only accidentally equivalent for the
  observed zero first byte. Nonzero header flags remain unsupported.
- 0xc0de–0xc0fa: step +2 is split into a high two-bit render mode and low
  14-bit parameter. It must not be treated as a plain millisecond/tick delay.
- 0xc11e–0xc141: add step +4 and +6 into X/Y, inclusively through the selected
  step. Additions use 16-bit words. The preview now reproduces these prefix
  sums and their wrapping, rather than adding a delta on each redraw.
- 0xc143–0xc254: four render-mode branches. Mode 1 references the following
  pose and maintains a substep counter. Modes 2/3 pass the parameter to other
  rendering routines. Their full effects have not been implemented; the
  preview explicitly marks nonzero modes unsupported instead of silently
  treating them as mode 0.
- 0x136cf–0x136e8: PIT divisor programming exists, but its relationship to
  animation cadence has not been established. It is not evidence for 150 ms.

Reproduce the read-only inspection (requires objdump; rejects unknown EXE
revisions before applying these offsets):

```sh
python3 scripts/sotc/inspect-executable.py build/sotc-DOS/disc-files/SHADOW.EXE > artifacts/sotc/sequence-routines.txt
```

The extended animation audit verifies inclusive displacement, negative values,
16-bit wrapping, truncated sequence input, index bounds and restart without
accumulated drift. Stubbs sequence 5 has X positions relative to its anchor:
1, 7, 10, 14, 16, 19, 20, 26, 28, 32, 35, 38. There are 168 valid steps using
special render modes across the current resources; they remain unsupported.

Rebuilt the PS4 package and verified sequence 5 visually in shadPS4 at steps
3 and 8: `artifacts/sotc/ps4-motion-0.png` and `ps4-motion-1.png`. This is still
sequence inspection, not navigation, collision detection or gameplay.
