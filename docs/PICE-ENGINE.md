# Prisoner of Ice engine development

Current continuation: [playthrough and regression ledger](PICE-PLAYTHROUGH.md).

Latest: [Hamsun inventory, medal use and Nordic text](PICE-HAMSUN-INPUT.md).

Latest: [medal pickup and sleeping-quarters tasks](PICE-MEDAL-RUNTIME.md).

Latest: [bunk figures and selected-item cursor](PICE-BUNK-RUNTIME.md).

Latest: [drawer objects, hover inventory and music recovery](PICE-DRAWER-RUNTIME.md).

Latest: [scene depth, hover feedback and death recovery](PICE-RECOVERY-AND-LAYERS.md).

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

## Language resource-name index (2026-09-19)

The original ICE640 code provides concrete anchors for further analysis:
object 2 offset 0x23f9c opens KRO containers, compares the Burp signature,
reads an 8-byte header and computes count * 20 for its directory. Its caller
at 0x188e8 opens three resource archives. Object 2 offset 0x18661 applies a
chained XOR transform to a length-prefixed block, starting with 0x5a and
using each decoded byte as the next key. Applying that transform to the
local E_KLANG.BIN yields a structurally consistent resource-name table.

The English CD file is 561 bytes: uint32 length 556, 556 encoded bytes, one
zero trailer. Decoded content is uint32 count 46 followed by 46 records of
8-byte zero-padded ASCII names and uint32 archive base indices. Full eight
character names need not contain a terminator. Bases are increasing and
within the 717-entry E_KLANG archive. Examples: E_M02R00 -> 0,
E_M02R01 -> 8, E_X02R24 -> 714. These are resource-group names, not proof
that room scripts or a room interpreter have been decoded.

Object 2 offset 0x1b998 searches eight-byte names in 12-byte records and
adds a signed local index to the matched uint32 base. Our initial resolver
accepts nonnegative indices and rejects crossing the next group's base or
archive end; this is deliberately stricter than the original addition.
Do not infer runtime data addresses directly from unrelocated instruction
operands: the executable inspector still does not apply LE fixups.

The original new `ResourceNames` decoder has synthetic tests for chained
encoding, lookup, every truncated prefix, out-of-range indices, malformed
padding, duplicate names, reversed bases and clearing stale state. The engine
loads E_KLANG.BIN when available and reports every resolved base for comparison
with the independent `scripts/pice/audit-language-index.py` oracle. Missing
BIN files leave the existing resource preview working; malformed present
indices are rejected. No original game bytes or executable extracts are tracked.

Guest verification completed: all 46 name/base pairs exactly match the host
oracle, encoded FNV a67ddbaf matches, all parser self-tests pass, and animation
0 still boots. Evidence: artifacts/pice/language-index-guest.log and
language-index-oracle.json. This checkpoint adds a resource lookup building
block; no room-script execution or gameplay is implemented yet.

## Script-bank and execution checkpoint

`inspect-executable.py --fixups` now decodes the observed internal LE fixup
variants into source-object/source-offset and target-object/target-offset
metadata. It does not apply them or export executable bytes. Tests cover
ordinary and negative cross-page source offsets and invalid encodings.
ICE640 has 9,815 fixup records; ICE320 has 9,821. One target in each file is
outside its object's virtual extent and is marked as such rather than treated
as a dereferenceable pointer. External/chained/additive/list encodings remain
unsupported and fail explicitly.

This resolves a crucial ambiguity: the resource-name tables referenced from
code offsets 0x1ba88 and 0x1ba9f target **object 1**, not object 3. Object 1
contains data as well as its executable flag. Chained-XOR blocks at object 1
0xd60 (352 bytes after its length) and 0xed0 (304 bytes) decode to 29 graphics
and 25 sound group names. Examples: SA02 -> 0, SB02 -> 815, SM02 -> 1138,
SOBJECT -> 1306. Metadata was inspected locally; no game table bytes are added
to the repository.

The SM02..SM12 range, KSVGA IDs 1138..1305, contains 155 script banks and 13
five-byte `dummy` placeholders. There are 1,499 script entry points. A bank
starts with uint16 N followed by N-1 uint16 offsets. The first word N is also
the first entry's offset: all offsets are measured in **16-bit words from the
resource start**, not bytes. Original code evidence: object 2 0x1de reads
N-1; 0x21e loads table[index], doubles it and adds the resource base. Each
scheduler bank has 50 task slots. The new parser validates these bounds and
allows aliased entry addresses; it does not presume code regions cannot jump
between entries or attempt to validate every instruction yet.

Command dispatch: object 2 0x21ae reads a 16-bit opcode through the current
instruction pointer and calls the table at object 3 offset 0x11d9. Confirmed
pure subset implemented with bounded recursion and transactional PC updates:

| Opcode | Operation | Handler in object 2 |
| --- | --- | --- |
| 7 | signed inequality | 0x2700 |
| 8 | signed less-than | 0x271e |
| 9 | signed greater-than | 0x273c |
| 10 | signed less-or-equal | 0x275a |
| 11 | signed greater-or-equal | 0x2778 |
| 12, 13 | eager logical AND, OR | 0x2799, 0x27c4 |
| 14, 15, 16 | addition, subtraction, multiplication (low 16 bits) | 0x27eb, 0x2805, 0x2821 |
| 20 | following 16-bit literal | 0x2916 |

The small explicitly stepped ScriptRunner additionally handles 3 (start entry;
0x23a2 -> 0x21e) and 31 (stop entry; 0x23c8 -> 0x2a1). Task state here is only
PC and active/inactive. Original teardown of actor/audio resources, waits,
scheduler ordering, branching, variable state, movie playback and scene
operations are not implemented. Unsupported commands stop with kUnsupported;
invalid input stops with kInvalid. They are never silently skipped.

Synthetic guest tests cover every implemented binary operation, signed values,
wraparound, nested expressions, truncated operands, recursion limits, malformed
entry tables, starting/stopping two tasks and faulting on unknown commands.
The actual resource 1138 entry 0 begins with stop-entry and literal zero. The
preview verifies decoding that argument and stepping the task until it stops
itself. This is real but very limited script execution, not a playable scene.
The next entry starts with unsupported opcode 138 (movie-related evidence
still needs investigation); no attempt is made to fake its effects.

Independent corpus oracle: `scripts/pice/audit-scripts.py /path/KSVGA.KRO`.
It emits only entry metadata and hashes, not script bytes.

Verification completed in shadPS4: all 155 bank entry counts and FNV hashes
match the independent oracle; 1,499 entries and 13 placeholders are reported.
Guest synthetic tests pass, original stop-self reaches byte offset 10 and
animation 0 still starts. Evidence: artifacts/pice/script-runner-guest.log and
script-index-oracle.json. No physical PS4 verification of this PICE checkpoint.

### Variable and control-flow checkpoint

The runner now models the two signed 16-bit variable tables (A: 250 entries,
B: 300), their reads (5/19) and assignments (25/26). The observed initial
state is A[0]=-1, remaining A entries zero, and B entries -1. Original handlers:
0x288f/0x28ee, 0x2852/0x28b7; B initialization 0x783, save/load sizes
0xbe8d/0xcf49. Assignment commits only after a complete valid RHS.

Opcode 4 (0x2640) jumps by a signed word displacement relative to the end of
its instruction. Opcode 21 (0x2935) evaluates a condition then either skips
or takes the following jump. Destinations must be aligned code addresses,
not entry-table bytes. An explicit instruction budget yields without killing
the task; backwards loops can resume safely. Waits and scheduler ordering
remain unimplemented.

Guest tests exercise signed reads, table boundaries, transactional assignment,
both conditional paths, backward jumps, invalid destinations and resumable
budget exhaustion. All pass in shadPS4. Independent, fresh-state probes of
all 1,499 original entries with a 64-instruction budget report 407 stops,
989 unsupported commands, 102 invalid/unsupported expressions and one budget
exhaustion; 415 probes changed PC. These are isolated prefix probes, not an
original game scheduler or a playable scene. Evidence:
`artifacts/pice/control-flow-guest.log`. Animation and speech preview still run.

### Dialogue milestone: "KAA NAAMA FTAH'N CTHULHU!"

The preview now resolves E_T02 local 6, text key 1070 from the user's English
CD data. Its directory identifies E_M02R03 and local voice 7, which resolve
through E_KLANG.BIN to voice resource 86. Both the displayed sentence and
voice selection are read from the game resources, not embedded in the engine.
The startup selection is explicit testbench code; it is not a script-triggered
scene and the walking sprite is unrelated to the speaker. Repeat line replays
the dialogue; Space/middle-click still browse voice samples. Native keyboard C
also repeats the dialogue (host emulator key mapping may intercept it).

Dialogue decoder evidence in ICE640 object 2:
- 0x1d08a and 0x1d0f2 derive directory count from (firstOffset - 10) / 11.
- Directory records are seven-byte keys plus uint32 offsets. The directory is
  followed by the eight-byte voice group, a zero and the first voice flag.
- Lookup returns entry+2; entry[-1] enables voice and entry holds its signed
  local index (0x4499, 0x44b9).
- Relative uint16 page offsets start at entry+2. The first offset / 2 gives
  page count (0x42ae). Each page begins with a line count and terminated lines
  (0x4555, 0x42b5).

The bounded decoder validates directory geometry, offsets, page order, line
counts and string termination. Synthetic tests cover missing keys, silent
lines, multiple pages, truncation, invalid offsets and unterminated text.
Page timing, actor ownership, glyph encoding beyond this ASCII test sentence
and full dialogue scheduling are not implemented yet.

Voice 86 uses the observed 20-byte EDITLS:05/03/95_1.0 header. A uint32 span
at byte 20 includes its own four bytes; the embedded RIFF/WAVE starts at 24.
All 35 EDITLS language resources in this corpus have this same envelope and
span relation. The decoder checks both lengths and passes only the bounded
WAVE payload to ScummVM. Extra wrapper chunks are not interpreted; this does
not implement lip synchronization. Synthetic tests reject bad lengths,
unsupported wrapper versions and non-WAVE payloads.

Verification in shadPS4:
- Guest parser tests PASS after rebuilding.
- Text resource 639 FNV 055378b1; voice 86 FNV 8cb20528, 74,808 decoded bytes.
- WAVE offset 24, length 74,729, 22,050 Hz, approximately 3.387 seconds.
- Screenshot shows the decoded sentence; captured output correlates with the
  source PCM at 0.906 (48 kHz output resampled against 22.05 kHz source).
- User explicitly confirmed hearing the line.

Ignored evidence: artifacts/pice/dialogue-guest.log, dialogue-output.wav,
quote-candidates/86.wav and the emulator screenshot
SCVM00003_20260919_211344_294_game_000000.png. Commercial text/audio remains
only in local game data and ignored evidence, not in committed assets.

### First script-driven room graphics and cooperative waits

The preview now executes entry 0 of English EU script bank 1140, starting at
byte 62. The original instructions load B02 local 0 as the background and B02
local 1 as its palette. The runner calls a scene sink only after every operand
has decoded successfully. Unsupported resource groups/flags fail without
advancing the instruction; unknown commands are not skipped.

Implemented commands:
- 1 (handler 0x21d1): name string, local-index expression, flags expression.
- 27 (0x2264): name string, local-index expression.
- 22 (0x266e): signed wait duration, task state 2. A subsequent dispatcher visit
  subtracts elapsed ticks. Even the visit that expires a wait returns without
  executing the next instruction, as original 0x415 does. Zero/negative waits
  still yield; restarting a task clears its countdown. No wall-clock scheduler
  is attached to game tasks yet.
- 45 (0x26de): signed equality, verified through the LE dispatch-table fixup.

The string decoder follows 0x2121's NUL-inclusive even-byte padding and rejects
truncation, excessive length and unsupported non-ASCII bytes transactionally.
0x2187/0x1c88b strips the extension and appends an empty suffix in this edition.
The pointer at object 3 0x2609 targets its empty string at object 3 0x592.
The graphics prefix pointer 0x25e1 targets "S" at object 3 0x454. Both object
identities are confirmed by fixups, not assumed from raw pointer values.

This first scene sink deliberately supports only SB02 (resource IDs 815..881),
640x316 backgrounds, 768-byte palettes and background flags zero. It resolves
the known group span explicitly; generalized EXE-derived graphics name loading
is still pending. The preview keeps its existing 640x400 display layout;
original viewport placement and rendering flags are not yet reproduced.

shadPS4 results:
- All guest parser tests PASS, including both resource commands, extension
  stripping, missing sinks, rejected flags, alignment truncation, waits,
  restart/overshoot handling and equality.
- Script background resource 815 FNV 2881fe4d and palette 816 FNV fc170f55.
- Stops at byte 86, opcode 78, after the two original resource commands.
- Isolated original-entry probes now report 420 stopped, 951 unsupported,
  71 invalid/unsupported expressions, 4 budget yields and 53 waiting; 500
  probes changed PC. These counts are not a measure of playable game coverage.
- Screenshot SCVM00003_20260919_212506_289_game_000000.png shows the room with
  an explicit stopped-script status. Log: artifacts/pice/scene-timing-guest.log.

The chant remains a separate selectable dialogue demonstration, unrelated to
this script's current state. Room actors, foreground elements and interaction
are absent; the background alone is not a complete scene. Clicking the title
still opens the earlier animation browser.

Next continuation: opcode 78 dispatches to 0x33fa. It reads a name/index, loads
resource type 2 into handle 0x1446 and calls 0xb5f6 on its payload. In bank 1140
this is B02 local 12, KSVGA resource 827. Decode that room data and its consumers
before implementing its effects. Subsequent commands include 30 and 47; their
actor/audio/coordinate semantics still require investigation. Original clock
helper 0x1e9e4 scales the IRQ counter (incremented at 0x276e8) by the timer
reload and 100 / 0x1234dc, useful evidence for future scheduler timing.

### Opening-room runtime continuation

The background-only checkpoint above has been superseded. See
[PICE-OPENING-RUNTIME.md](PICE-OPENING-RUNTIME.md) for the initialized room,
script-driven entrance and captain's dialogue, concurrent actor walks, guest
tests, remaining limitations and the exact next opcode. The full game is
still not playable.

## 2026-09-20: native checkpoint restoration and post-intro progress

See [PICE-SAVE-RUNTIME.md](PICE-SAVE-RUNTIME.md) for autosave, F5/F7 checkpoint
controls, fresh-process restoration with music, and the next input/death-flow
continuation. The engine is still experimental and not a complete playable game.

Latest interaction milestone: [native input and inventory](PICE-INPUT-RUNTIME.md).

Fire-puzzle continuation: [dynamic navigation and extinguishing](PICE-FIRE-RUNTIME.md).

## Object responses after the fire (2026-09-20)

Chest use/examine now complete through opcode 111, including text-only pages.
Common object-response fallback is also implemented.
See [PICE-OBJECT-DIALOGUE.md](PICE-OBJECT-DIALOGUE.md) for evidence and limitations.

## Toward the bridge (2026-09-20)

Post-fire exit reaches room 7. Item-specific actions (81) are implemented;
room initialization still needs entry placement/animation (79) and random
expressions (38). See [PICE-BRIDGE-RUNTIME.md](PICE-BRIDGE-RUNTIME.md).

### Room entry continuation

Source-room entry animations (79) and positive-bound random expressions (38)
are implemented. The emulator traverses the arrival/middle scene and reaches
Driscoll's next dialogue, with a version-6 autosave. Remaining stops are exit
registration 80 and ordinary dialogue 52; details are in PICE-BRIDGE-RUNTIME.md.

Ordinary speech (52) now advances the Driscoll exchange to opcode 68. Dialogue
subtitles wrap at a larger size. `scripts/pice/check-driscoll.py` automates
replay and stop reporting from the local room-7 autosave.
