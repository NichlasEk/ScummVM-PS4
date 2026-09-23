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

The initial raw-frame mode advanced in resource order every 150 ms. Sequence
mode, pose placement and movement have since been added as described below;
original timing and actor state still require further work.

The VA2 header has four little-endian section offsets. The second section has
32-bit relative frame offsets; each frame has a two-byte orientation prefix
immediately before the indexed dimensions. Supported frame records
start with width in 16-pixel units and byte height. Each scanline holds a run
count followed by transparent skips and literal runs (four-byte groups plus
0–3 remaining pixels), then a final skip to the padded width. What initially looked like optional 0000/0080 trailers is the **following
frame's orientation prefix**, confirmed in the original blitter. Exact 0000 frames
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
for inspection only. Playback now stops at the last step to avoid snapping
back to the anchor; toggle the figure off/on to replay.

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

### Frame orientation correction after user testing

The user observed backwards walking. SHADOW.EXE load-image 0x14a31 XORs the
pose-reference flag with a word located **two bytes before the frame header**,
then tests bit 0x8000. Stubbs frames 9–20 have that bit set even when their pose
references are unmirrored. The frame decoder now normalizes this stored
orientation before pose mirroring, including the transparency mask. This fixes
the direction without special-casing any character or sequence.

At 0x149ff–0x14a06 the bottom anchor is converted with `y - height + 1`;
the preview now includes that final pixel. Tests verify mirrored opaque-zero
pixels and the mask. Sequence playback stops at its final step instead of
jumping back to its starting point. C/Triangle off and on replays it.

After validating prefix flags, coverage is 5420 supported / 800 unsupported
frames: two previously rendered records have additional prefix bits that are
now conservatively rejected. The earlier 5422 figure describes pre-validation
coverage. Rebuilt and captured corrected sequence 5 at steps 4 and 8 in
`artifacts/sotc/ps4-orientation-0.png` and `ps4-orientation-1.png`; the user
confirmed the corrected walking direction looks better.


## CC4 room scripts: bounded reader and instruction inspection

The engine now reads R00–R09.CC4 when selecting a resource bank. These have
LE32 outer offset tables and LE16 room-local script offset tables; the first
offset is also the table length. All ten supplied banks validate: **324 blocks,
1835 scripts**. A block is not necessarily a distinct playable room.

`cc4.h` provides non-owning, bounds-checked views; `script.h` decodes a small
verified opcode subset without executing it. The PS4 log includes `[sotc-cc4]`
counts and the first block's initial instruction prefix. Inspection stops at
control flow, task stop, unsupported instructions or bad/truncated input. It
never fills unknown opcodes with no-ops or guesses game-variable values.

Evidence from the same SHA256-checked DOS executable (load-image addresses):

- 0x8e8d: task initialization reads room-local LE16 script offsets.
- 0x8f7b: first offset / 2 is the script count; initializes tasks and runs task 0.
- 0xb2ec: byte opcode dispatch, 0..103, via a table of far pointers.
- 0x93ad: opcode 10 compares a LE16 variable reference with an expression;
  the false branch uses a signed relative word. 0x9057 bases that displacement
  at the operand itself. Expression token 11 references a variable, 12 carries
  a byte literal, 13 carries a word literal. Variable storage is not wired yet.
- 0x9f9f: opcode 2 sets the task-disabled flag without advancing its IP.
- 0xa057: opcode 3 is an unconditional relative jump.
- 0x9b79 / 0xc886 / 0xc8a3: opcode 58 selects a paired scenery resource,
  storing byte argument * 2; the following entry is the scenery animation.
- 0x9c62: opcode 22 reads a **byte** count and byte coordinate pairs, doubling
  X. Its gameplay meaning is not yet established; it is not labelled a
  walk polygon in the decoder.

The decoder currently supports instruction formats for opcodes
1, 2, 3, 4, 5, 10, 22, 27, 28, 58, 59, 80, 87, 88 and 96. Recognition of an
instruction format does not mean its game behavior has been implemented.
Branches outside an individual script view are conservatively rejected; this
is a current limitation, not proof that the original data would be invalid.

Run the shared C++ reader/decoder tests and resource audit:

```sh
python3 scripts/sotc/audit-cc4.py build/sotc-DOS/disc-files/SHADOW > artifacts/sotc/cc4-audit.json
```

The audit covers malformed directories, out-of-range entries, byte/word
operands, truncated commands, backward jumps and the correct displacement
base. It decodes **701 prefix instructions**, stopping at 345 control-flow/task
boundaries and 1490 unsupported instructions across the 1835 scripts. There
are no malformed/truncated stops in these prefixes. This is deliberately not
reported as gameplay coverage.

Next: recover the initial variable state and actual startup block, then actor
binding, scheduler/wait behavior and text commands. R00 block 0 starts by
comparing variable 34 against 5; it must not be assumed to be the new-game
entry point. The existing visual resource preview remains the current UI;
this milestone does not yet provide an interactive scripted scene.

PS4 package rebuilt successfully. Live shadPS4 verification in
`artifacts/sotc/cc4-live.log` reports R00: 84 blocks / 617 scripts, and decodes
block 0 / script 0 at PC 0 as opcode 10, next PC 9, false target 16. The
resource preview then draws F00.RAW with CTU.PAL as before.


## Original new-game snapshot (INIT.SAV)

New-game code at 0x238e selects `INIT.SAV`, then 0x2508 reads its header and
0x25ee calls the state loader. It does **not** initialize every script at PC 0.
The supplied save is 3966 bytes: `NED\0`, a 30-byte description, 30 bytes of
options, then 3902 bytes corresponding to DOS DS:0x2dee..0x3d2b. The payload
is XORed bytewise with `(index + 1) * 5` modulo 256 (0xe26a). No resource data,
DOS pointers, or proprietary executable bytes are embedded in the engine.

`startstate.h` decodes the supplied file with size/signature/bounds checks.
The startup snapshot points at bank 0, room block 0, background entry 0,
15 tasks and 10 draw-list entries. Original save-load 0xe12c rebases each task
using saved IP minus saved script base; the reader reproduces that difference
and validates it against the current script. It never dereferences saved DOS
segment values. Task 0 is already disabled; task 1 resumes at script-local
PC 14; other active tasks have animation/timer wait flags. Restarting task 0
would overwrite this intentionally prepared state.

`startup.cpp` now displays this **static snapshot**, with the original actor
resource slots, sequence steps, coordinates, clipping rectangles and saved
interleaving with foreground scenery. Slot type 0 loads Axx resources; type 1
uses RES entry 2 (the protagonist resource, established by 0xc96/0x6cb5).
The protagonist anchor is (220,151). Scene pose 0 contains nested pose
references: these are drawn in the saved order, not as frame indices.

The real scene exposed another VA2 boundary issue: a non-final record's extent
includes the following record's two-byte prefix. It is now excluded before
validating payload length; a nonzero following prefix must not invalidate the
previous pose. WEST0 pose 52 exercises this, and a synthetic regression test
covers it. Frame audit remains 5420 supported / 800 unsupported.

Current snapshot renders 6 of 10 draw-list entries: three people and three
foreground layers. Four are explicitly unsupported: the seagull animation's
special render mode, two bird poses with vector primitives, and the car's
mixed nested/frame/vector pose. These must be implemented before claiming a
complete original start scene. The snapshot does not run scripts, dialogue,
movement, timing, interaction or save/load gameplay yet.

Options on the controller (Return in shadPS4, F8 for native keyboard input)
toggles snapshot/resource viewer. Existing resource controls remain available
in that viewer. Touchpad exits. Missing or unsupported INIT.SAV falls back to
the resource viewer. The scene uses the existing CTU palette for this release.

Validation:

```sh
python3 scripts/sotc/audit-startstate.py build/sotc-DOS/disc-files/SHADOW > artifacts/sotc/startstate-audit.json
python3 scripts/sotc/audit-va2.py build/sotc-DOS/disc-files/SHADOW > artifacts/sotc/startup-va2-audit.json
bash scripts/sotc/build.sh
```

The start-state tests cover short/oversized/wrong-signature files, out-of-range
reads, invalid task indices, truncated script tables and saved IP underflow,
plus all 15 actual rebased task PCs. Built and launched SCVM00004 in shadPS4;
`artifacts/sotc/startup-live.log` records every task/actor and the 6/4 rendering
result. Screenshots `startup-scene.png` and `startup-toggle.png` verify the
snapshot and working return to resource preview. No physical PS4 test yet.

Continue with variable-sized pose layers (0x10c41: four-byte layer header,
then coordinate-count * 4 bytes; type 0 nested pose, type 1 frame, type 6
polyline via 0x10f82/0x14d58). Then implement scheduler wait bits and resume the
saved task PCs. Do not silently clear their waits or reset the scripts.


## Variable-length poses and vector drawing

The startup renderer now supports all **10/10 draw-list entries** in the
supplied INIT.SAV. The resource called VOITURE is visibly a horse-drawn
carriage, not a motor car. Two small birds use polylines; the seagull uses
filled, optionally outlined polygons and a morph between matching poses.
The seagull is above the screen in this saved snapshot; the host audit also
renders it at a visible diagnostic anchor without changing the game state.

`pose.h` validates variable-length commands: four header bytes followed by
coordinate-count * four bytes. `poserenderer.h` implements nested poses
(type 0), frame references (1), polygons (4) and polylines (6). Unsupported
commands fail transactionally: the caller's image is only replaced after the
complete pose succeeds. Recursion depth, command and raster-work budgets
prevent cycles or pathological input from hanging the renderer.

Executable evidence:

- 0x10c41 consumes the variable-size command stream and adds the anchor.
- 0x10f82 draws consecutive coordinate pairs using the header's high byte
  as the line color.
- 0x10efe fills polygons with the high byte, then draws a closed outline
  with the low byte unless it is 0xff.
- 0xc143 prepares a current/next pose transition; the factor is the saved
  substep * 256 divided by the low 14-bit parameter (zero coerced to one).
- 0x10eb9 interpolates signed coordinates with an eight-bit fractional part.
  Negative products round down, not toward zero. The renderer supports the
  verified equal-topology vector case; arbitrary pose merging and modes 2/3
  remain unsupported.

Line/polygon rasterization currently uses integer Bresenham/even-odd filling.
Colors, command structure and interpolation come from the original code, but
pixel-exact edge rounding against DOS output has **not** been established.
10/10 describes this snapshot's command support, not whole-game coverage.

```sh
python3 scripts/sotc/audit-poses.py build/sotc-DOS/disc-files/SHADOW > artifacts/sotc/poses-audit.json
```

The audit compiles the actual renderer with a host Array-container shim. It
checks line endpoints and clipping, nested translations, recursion cycles,
transactional failure after a valid layer, truncation, polygon fill/outline,
negative fractional interpolation and mismatched morph topology. Supplied
VOITURE, BIRDS poses 0/4 and SEAGULLS pose 0→1 all render successfully.
The PS4 build runs in shadPS4 with `10 drawn, 0 unsupported`; evidence:
`artifacts/sotc/vector-live.log` and `vector-scene.png`. Existing resource
preview remains accessible with Options/Return. Still a static snapshot,
not a running game or a physical PS4 validation.

Scheduler continuation evidence (not implemented yet): 0xb008 decrements the
saved timer at task +0x0a on a scheduler visit and clears flag 0x02 at zero.
0xaf5a clears flag 0x04 when actor step (+0x0e) equals step-count (+0x12)-1.
Do not substitute wall-clock milliseconds or clear these flags at load.
Original task 1 resumes on opcode 40 at script-local PC 14: 0xa9fc reads four
byte coordinates (doubling X), stores the rectangle at task +0x0e..+0x14,
and rewinds five bytes/yields when predicate 0xa904 is false. Trace that
predicate next before implementing its behavior.


## First bounded script scheduler

`scriptmachine.h` now restores task PCs/actor bindings/wait flags/timers from
INIT.SAV and owns a bounded copy of the current CC4 block. No saved pointers
are followed, and task 0 is not automatically restarted. One scheduler visit
runs each task 1..N-1, matching 0xb31c. The scene remains a static snapshot:
this is controlled VM stepping, not a playable room yet.

Implemented command behavior:

- 2: disable current task, retain IP.
- 3: signed jump relative to the displacement operand.
- 9: set timer, advance IP, yield (even for zero delay).
- 10: compare and branch using signed comparisons / bit tests from 0x906b.
  Saved variables 0/34 and in-range saved variable banks are readable;
  runtime variables and actor aliases explicitly stop as unknown.
- 21: bind task to actor.
- 27/28: restart or disable another task, with original scheduler ordering.
- 39/40: wait until the player's footprint leaves/enters a stored rectangle.
  Coordinates use byte X * 2 and byte Y; edge contact counts as overlap.

Timer flag 0x02 decrements once per scheduler visit and clears at zero.
Animation flag 0x04 clears when the externally supplied actor step equals
step-count minus one. Actual animation advancement and original tick cadence
are not connected yet. Other wait types stop explicitly. Every task has an
instruction budget, so a cyclic jump cannot lock the emulator. Unknown
commands retain their PC; there are no guessed operand skips.

0xa904 uses **actor 0's footprint**, not the mouse cursor: left/right are
X minus/plus actor byte +0x15; top is Y minus byte +0x16; bottom is Y.
0x134ac performs inclusive signed rectangle overlap. Initial task 1 waits on
opcode 40 at local PC 14 for rectangle (110,154)..(126,199). The original
player at (220,151) is outside it, so remaining there is correct behavior.

Before event/random support, the first real scheduling pass produced:

- six disabled tasks, as saved;
- five waiting tasks (one region and four animation waits);
- four explicit unsupported-command stops: task 5 opcode 66, tasks 8/14
  opcode 79, task 12 opcode 99. Saved one-tick timers have expired correctly.

The engine performs one visit at load. Cross / left click steps another
visit (N on the configured shadPS4 keyboard mapping). The status line reports
waiting/stopped counts; detailed `[sotc-vm]` logs include task, PC, opcode,
flags, result and executed-command count. This button is diagnostic input;
it does not move the player or dispatch game mouse events.

```sh
python3 scripts/sotc/audit-scriptmachine.py build/sotc-DOS/disc-files/SHADOW > artifacts/sotc/vm-audit.json
```

Tests use the real scheduler: timer countdown/yield, inclusive footprint
boundaries, leave-region waits, signed comparisons, bit tests, a cyclic-jump
budget, unsupported/invalid commands preserving PC, animation completion,
restart ordering, unknown wait flags and the supplied INIT.SAV's exact first
visit. CC4 decoder regression audit also passes. PS4 package built and run in
shadPS4; Cross/N verified tick 1→2 without changing blocked PCs. Evidence:
`artifacts/sotc/vm-live.log` and `vm-step.png`. No physical PS4 test.

Next blockers: opcode 66 checks runtime input event 0x60e2 plus a predicate;
opcode 79 invokes the original random-range helper and stores variable 33;
opcode 99 manages music. These are identified from their handlers but not yet
implemented. Actor movement/input, animation updates, audio and additional
commands are required before this can become gameplay.


### Event conditions and original random generator

Implemented opcodes 64 (pending event), 66 (pending event plus actor-0
footprint rectangle), and 79 (random range, shared runtime variable 33).
The EXE handlers at 0xa713/0xa734 consume the event only on success; an
absent event must not overwrite the task rectangle. The diagnostic step
button remains separate from game events. Actual input production still
needs binding; the VM exposes an explicit event setter for now.

Opcode 79 calls 0x7051, which uses Borland rand at 0x17b5f:
`state = state * 0x015a4e35 + 1` modulo 2^32, then
`((state >> 16) & 0x7fff) % range`. Ranges 0 and 1 return zero without
advancing the generator. **The preview uses explicit diagnostic seed 1**;
the original startup seed source has not been wired, so this is not a claim
of identical random choices across original and preview launches. Variable
33 is unknown until the first random command rather than inventing a value.

The supplied INIT.SAV now reaches seven waiting tasks and two unsupported
stops on its first pass: task 12 at PC 2/opcode 99 (music), and task 14 at
PC 46/opcode 77. Tasks 5 and 8 correctly return to their timer loops.
Tests cover repeatable RNG sequences/overflow, range 0/1, variable sharing,
event consumption/miss, preserving rectangles without an event, malformed
operands and the new exact INIT.SAV result. The CC4 decoder audit passes.
Evidence: `artifacts/sotc/vm-events-audit.json`, `cc4-events-audit.json`.

Next: decode opcode 77, then bind actual actor animation and music rather
than treating those commands as no-ops. Movement and input are still absent.

PS4 package built successfully and tested in shadPS4: first-pass results
match the native audit; Cross/N advances to tick 2 with tasks 5/8 executing
another timer loop. Screenshot `artifacts/sotc/events-step.png`, log
`artifacts/sotc/events-live.log`. No physical PS4 validation in this step.


### Original sound effects through the mixer

Opcode 77 is a sound-effect request, not an animation command. Handler
0x961d calls 0xd5ad for a sample index or 0xd56b for index 255 (stop).
0xd5ad checks the single sample channel before loading: an active effect
ignores another request rather than restarting or queuing it.

`SoundEffects` now loads the caller's `SMP.PAK` on demand, decodes the VOC
with ScummVM's existing unsigned-PCM decoder and plays it as SFX. Buffers are
owned by the audio stream, and shutdown stops the channel. ScummVM volume
and mute settings are synchronized on startup. The original per-effect
volume opcode 78 and music opcode 99 remain unimplemented; the supplied
INIT.SAV's effect volume is already at its maximum (127).

A missing/unavailable audio host, missing sample or failed decoder leaves
opcode 77 at its existing PC with `kSoundUnavailable`. A request succeeds
only when the host handles it, including the original busy-channel case.
The VM tests exercise stop/index dispatch, failed requests with PC retained,
retry and the actual initial task 14 sound request (sample 6).

All 94 supplied SMP.PAK effects pass the header, checksum, block bounds,
termination and unsigned-PCM format audit:

```sh
python3 scripts/sotc/audit-sounds.py artifacts/sotc/unpacked/SMP.PAK > artifacts/sotc/sound-audit.json
```

PS4 build verified in shadPS4: `sample=6 name=C7M.VOC rate=8000 ... active=1`;
task 14 proceeds to its five-tick timer. The screen shows eight waiting tasks
and one unsupported command (music 99), including after Cross/N steps to
tick 2. Evidence: `artifacts/sotc/sound-live.log`, `sound-step.png`,
`vm-sound-audit.json`, `cc4-sound-audit.json`. This verifies decoding and mixer
activation, not a listening-quality comparison or physical PS4 test.
Scene animation is still frozen and scheduler stepping is still diagnostic.
No original sample bytes are tracked or included in the package.


### ADLM music playback

Opcode 99 now requests music from the user's `MUS.PAK`; 255 stops it, and
requests while music is active are ignored, matching the handler at 0x9506.
Missing/invalid files retain the script PC with `kSoundUnavailable`.
Music advances in real time while the scene scheduler remains manually stepped.
Initial task 12 plays entry 0 (`SHADOW1.ADD`) and returns to its one-tick timer.
The first pass now has nine waiting tasks and zero unsupported stops. This
is still a frozen scene, not a claim that the game is playable.

ADD files use the `ADLM` format. The header holds eleven stream offsets at
+8, an instrument-table offset at +52, and OPL rhythm/melodic parameters at
+60/+61. Instruments are 13 bytes. Commands 0..5 respectively end, set delay,
set note, select instrument, set attenuation and set pitch bend; bit 7 yields
until the next timing interval. The terminal 0x80 may be the very last byte.
All supplied songs use this subset. Streams, instruments, pitch lookup bounds
and non-yielding runs are validated before playback.

`adlm.h` ports the event/render behavior read from the supplied ADLIBM.COM
(offsets here include the COM load address 0x100): interpreter 0x979..0xa4d,
render/cache 0xaad..0xbbd, instrument registers 0x8b4, level 0x774, note and
rhythm handling 0x7b4/0x843, song setup 0xd9d. Pitch and operator tables are
read from the user's **ADLIBM.COM**, alongside the PAK files. This file is
required for this music implementation; none of its tables or executable
bytes are embedded in the engine. Only the known IFGM ADLIB layout is supported.

`Music` supplies its own mono PCM stream around ScummVM's DOSBox OPL2
emulator (MAME fallback when DOSBox OPL is disabled). Timing is 60 Hz using
sample counts with a fractional remainder; the original executable sets 60
at startup (0xb7b). Output uses `kMusicSoundType`, so ScummVM music volume and
mute scale final PCM separately from sound effects. All synthesis and register
writes occur in the audio callback, with a mutex protecting song replacement;
the mixer is never called while that mutex is held. Natural endings preserve
release tails. Stop requests cancel pending starts and key off the chip.

Validation uses the *actual original driver* as an oracle, executing its
routines in test-only Unicorn and comparing every OPL register write against
the C++ player, tick by tick. All 15 songs reach their end with identical
register traces, including the two rhythm-mode songs. Truncated data, missing
terminators, bad offsets and invalid driver tables are also rejected.

```sh
# Test-only dependency, installed in an ignored virtualenv; not in the package.
.tools/sotc-audit-env/bin/python scripts/sotc/audit-music.py \
  artifacts/sotc/unpacked/MUS.PAK \
  build/sotc-DOS/disc-files/SHADOW/ADLIBM.COM > artifacts/sotc/music-audit.json
```

PS4 package built and run in shadPS4. Final evidence: `music-final-live.log`
shows track 0 and the first audio tick; `music-step.png` shows tick 2 with
nine waiting/zero stopped. A capture of only shadPS4's PulseAudio stream
confirms nonzero music PCM (peak 8565, RMS about 1682); `music-output-audit.json`
records the measurement. The first PipeWire node capture was inconclusive;
the successful check used `parec --monitor-stream` for the emulator specifically.
User also confirmed hearing music. No physical PS4 test or bit-exact rendered
waveform comparison against original hardware is claimed.

Next: advance actors and bind input, then run the scheduler continuously with
original scene timing. Later script commands may still expose additional stops.

### Scene animation and script completion waits

The startup scene now retains its background, resources and per-actor animation
state. Each diagnostic frame follows the original order at 0x8d07..0x8d1c:
script pass, scene render, then actor frame advancement. Rendering starts from
the clean background and preserves the saved foreground/actor ordering.
The frame/subframe transition arithmetic (0xc1c9 plus 0x7afc) is in
`animation.h`. Normal frame parameters are not mistaken for delays; mode-1
interpolation substeps follow the original reset/advance thresholds. Modes
outside the proven subset cause an explicit pause. The special actor-10 idle
controller is not yet implemented; the initial saved actor-10 state is static.

Script opcodes 87/88 now start the bound actor's sequence through a scene host;
87 yields until that actor reaches its final frame. Sequence selection follows
0x7857: reset frame/count/advance gate, preserve the existing interpolation
substep. Bad sequences or missing scene bindings retain the script PC.
Opcode 41 hides non-player actors (0x9e9e); hidden actors stop rendering and
advancing. Their original resource bytes remain cached rather than freeing
shared allocations. The player exception is preserved.

The preview now runs at an explicitly provisional **10 diagnostic frames per
second**, not a claim of original game pacing. Triangle (C in shadPS4) toggles
run/pause, Cross (N) pauses and steps one frame, Options changes views as before.
An unsupported script command or animation/render failure pauses automatically;
no unknown command is skipped. Music continues on its independent audio clock.

Validation:

- 384 render/update arithmetic cases match the original EXE executed in Unicorn,
  including frame wrap, gates and interpolation substeps. Graphics calls and
  sequence lookup are stubbed in that focused test, not the arithmetic itself.
- The shared VM tests cover missing/failing animation hosts, 87's yield and
  completion, 88's continuation, and 41's player exception/hide dispatch.
- Pose rendering and CC4 decoder regression audits pass.
- PS4 package tested in shadPS4: West switches from sequence 8 to sequence 1;
  both small bird actors finish and are hidden; the seagull moves into view.
  Automatic preview pauses at tick 13, task 10 PC 13/opcode 90, with zero
  unsupported animation formats. `animation-final.png` records that scene;
  `animation-final-live.log` records the script and sequence transitions.

```sh
.tools/sotc-audit-env/bin/python scripts/sotc/audit-animation.py \
  build/sotc-DOS/disc-files/SHADOW.EXE
python3 scripts/sotc/audit-scriptmachine.py build/sotc-DOS/disc-files/SHADOW
```

Next is actor initialization/binding (90), resource replacement (91), and the
following script commands. Movement, player input, dynamic draw-list updates,
and original scene pacing are still required for a playable scene. No physical
PS4 test was performed for this milestone.

### Actor restart and resource binding

Opcodes 90 (reset/bind actor), 91 (bind A-bank resource) and 7 (absolute byte
coordinates, X doubled) now execute. The verified original handlers are
0xaf28/0x78c8, 0x968b/0x6d73 and 0xa3ab/0x87e3. Reset preserves the player-zero
exception and initializes the non-player position, footprint, visibility,
sequence state and full-scene clipping. Resource slots and clipping are now
mutable scene state. Resources are loaded from the user's current A-bank;
shared bytes remain cached, with eviction restricted to slots without visible
users. Original allocator addresses/refcounts are not reproduced.

Missing hosts/files and invalid actor indices retain the failing PC. Tests
cover these failures, successful dispatch, player reset and truncated operands.
The original reset/position routines were also executed for three actor IDs in
Unicorn to check defaults, alongside the existing 384 animation comparisons.

The PS4 build in shadPS4 now passes tick 13, restarts small birds and advances
both West and Derleth's animation sequences. At tick 59 all nine active scripts
are waiting normally, with no stopped script. Rendering deliberately pauses:
actor 4 has become visible but is absent from INIT.SAV's fixed draw list. This
is now reported instead of silently omitting it. Evidence:
`artifacts/sotc/actor-restart-live.log` and `actor-restart.png` (local/ignored).

Next: reconstruct dynamic actor/scenery ordering from the original draw-list
builder, then continue toward player movement/input. The preview remains a
bounded scene diagnostic with provisional pacing, not a playable scene or a
physical PS4 validation.

### Dynamic actor draw ordering

The preview rebuilds actor ordering every frame. Original helper 0x71b5 inserts
an unsigned byte Y-depth after existing equal depths; 0x881d visits actors
0..10 in ID order, requiring both visibility (+26) and draw eligibility (+50).
The portable bounded helper is `draworder.h`. Actor reset restores eligibility.
Scenery entries retain their stored order/depths from the current room snapshot:
they are deliberately NOT globally sorted (INIT contains 199,177,113). This
preserves the original insertion semantics and foreground-layer traversal.
Loading another room's scenery depth metadata remains future work.

Validation: 105 edge/random insertion cases match original 16-bit 0x71b5 in
Unicorn, seeded with those non-monotonic scenery depths. Rebuilt initial order
exactly matches INIT.SAV. VM and pose audits pass. The actual PS4 package in
shadPS4 passed both old tick-13/tick-59 stops and reached tick 592 with nine
waiting tasks and zero stopped tasks; screenshot `artifacts/sotc/draworder-live.png`
and trace `draworder-live.log` remain local ignored evidence. Window left running.

```sh
.tools/sotc-audit-env/bin/python scripts/sotc/audit-draworder.py \
  build/sotc-DOS/disc-files/SHADOW.EXE
```

Next is player input/movement and the region-triggered script progression.
Continuous ambient scene animation is working in this bounded preview; this
still does not establish gameplay, original pacing or physical PS4 behavior.

### Directional walking diagnostic

D-pad (arrow keys in shadPS4) now controls actor 0 in the start scene. Held
input selects original walking sequences 4..7; release selects standing 0..3.
Movement consumes the current sequence frame's DX/DY, using the original
0x8242 signed speed/drift adjustments and 16-bit arithmetic (`walk.h`). The
original 0x82c4 supplies this movement model. Rendering stays in actor mode 0,
so walking deltas are not added again as cumulative scripted-animation offsets.
Queued raw SDL hat events are handled alongside keymapped arrow events: SDL's
hat handler pushes those directly, which bypassed the keyboard-only route.
Releasing an older direction does not cancel a newer held direction.

This is explicitly **Walk test - no collisions**. It uses screen bounds only;
terrain/actor collision, destination routing, original mode-1 transition delay,
input restrictions and original pacing are still pending. Directional input
is a diagnostic adapter, not a claim of original keyboard/gameplay equivalence.
Script-owned mode-2 animations reject manual walking. Options stops walking
before switching resource views. Triangle pauses/runs; Cross still steps.

Validation: 540 speed/drift/wrap cases match original 0x8242 executed in Unicorn;
VM regression audit passes. PS4 package tested in shadPS4: left moves from
(220,151) to (183,151), release chooses standing sequence 3; down moves to
(183,157), release chooses sequence 2. Further left movement reaches (130,157),
whose footprint enters task 1's rectangle. Task 1 advances from PC 14 to PC 27,
then pauses on unsupported opcode 18 (tick 497 in this interactive run), with
zero unsupported animation formats. Screenshot/trace are local ignored
`artifacts/sotc/walk-trigger.png` and `walk-trigger.log`.

```sh
.tools/sotc-audit-env/bin/python scripts/sotc/audit-walk.py \
  build/sotc-DOS/disc-files/SHADOW.EXE
```

Next: opcode 18 and the newly reached scene sequence; separately complete
collision and original movement transitions before calling this gameplay.

### Footprint collision detection

Manual walking now checks the candidate footprint against the restored terrain
height profile (DS339e, 320 columns), packed room obstacles (DS3544, count3542),
and live actor footprints. The portable `collision.h` follows original 0xcf9d,
0xd116 and 0xd053: unsigned terrain heights, direction-dependent edge checks,
inclusive rectangle intersection, doubled obstacle X coordinates, disabled
zero-width obstacles, ascending actor order, self/hidden actors excluded, and
actor collision-bypass byte +23 == 8 respected. Reset clears that bypass byte.
Terrain, obstacle and actor hits retain original result codes 100/200, 300|id,
and 600|id. Out-of-array terrain lookups are rejected defensively.

Collision response for the manual diagnostic stops walking and keeps the last
valid position. Original exit selection (0xd24a), collision response/slide/detour
(0xd3c5), destination routing and terrain changes on room transitions remain
pending. Thus this is a collision-aware walk test, not complete original
navigation. The scene's existing data is read from the user's INIT.SAV; no
terrain, obstacle or graphic assets are embedded in the engine.

Validation: 3600 predicate comparisons against the original executable via
Unicorn pass (terrain, obstacles, actors), including all three terrain results,
all eight synthetic obstacle results and all eleven actor IDs plus clear.
The 540 walk arithmetic cases and VM regression audit also pass.
Actual PS4 package in shadPS4: upward walking rejects (220,117), retaining
(220,121), with terrain result 100. Rightward walking at Y177 rejects (236,177),
retaining (234,177), with obstacle result 301. Trace:
`artifacts/sotc/collision-live.log` (ignored). No physical PS4 test performed.

```sh
.tools/sotc-audit-env/bin/python scripts/sotc/audit-collision.py \
  build/sotc-DOS/disc-files/SHADOW.EXE
```

### Destination walking with collision-aware detours

Circle/right-click now chooses a destination under the virtual pointer. The
left stick moves a software crosshair; no desktop F7 capture is required for
controller destination selection. D-pad input cancels the route and retains
manual walking. Switching resource views also cancels it.

`route.h` provides a bounded four-neighbour breadth-first search over the
320x200 scene. This is our diagnostic planner, **not a reconstruction of DOS
routing**. Every candidate uses the verified terrain/obstacle/actor predicates.
The actor follows cardinal segments with the original animation delta budget,
checks every one-pixel intermediate position, stops exactly at the target,
and cancels safely if a live actor blocks its next step. There is no automatic
replanning around moving actors yet. Unreachable/out-of-screen goals clear the
route without moving the player; room exits remain unavailable.

Validation: synthetic wall/detour, unreachable, invalid and same-position tests
pass. An INIT.SAV fixture routes (220,177) to (310,150) with 117 clear steps;
each step is adjacent and collision checked. The 3600 original collision oracle
cases still pass. Actual PS4 build in shadPS4: target (197,171) takes 43 steps
and arrives; next target (304,161) takes 117 steps, changes direction four times
(up/right/up/right) around the room obstacles, and arrives exactly. Failed
(0,0)/(83,16) targets returned without hanging. Local ignored evidence:
`artifacts/sotc/route-live.log`.

```sh
python3 scripts/sotc/audit-route.py build/sotc-DOS/disc-files/SHADOW/INIT.SAV
```

Remaining fidelity work includes original destination flags, mode-1 transition
delay, collision response/detour choices, moving-actor replanning and room exits.
This route implementation deliberately makes no claim of original path choice.

### Script-controlled player movement and input locks

Opcodes 18/19 now bind player control state through the scene host. Original
0xaa6e reads a byte: zero sets all four directional mask bits and stops player
motion; 1..4 OR the original mask values 1,8,2,4. Unsupported table indices
are rejected. Opcode 19 (0xaac9) clears the mask and exits scripted mode 2.
The diagnostic applies standing animation immediately; original mode-1 delay
is still pending. Manual input cannot cancel a script-owned route.

Opcode 36 (0xa06f) consumes doubled byte X and byte Y, starts a destination and
sets task wait bit 1. The scheduler checks movement completion (0xaf9b), while
continuing to update other wait conditions, then clears bit 1 and selects
sequence zero before proceeding. Player routes are supported through the
existing collision-aware diagnostic planner; other actors explicitly report
host unavailable. Unreachable scripted goals stop at the command instead of
being silently skipped. Original path choice/failure behavior remains pending.

VM tests cover missing/rejecting hosts, malformed input, control lock/unlock,
movement start/yield, repeated waits and completion. Route regression passes.
In the actual PS4 package under shadPS4, entering the trigger now sets mask 15,
walks actor 0 to (104,171) through 27 clear steps, arrives, resumes task 1,
and starts sequence 3. The next unsupported instruction is opcode 1 at PC 34,
with zero unsupported animation formats. Local trace:
`artifacts/sotc/script-movement-live.log`. This is emulator evidence, not a
physical PS4 or complete scene-playthrough claim.

Next: opcode 1 at 0x9bbc and the subsequent commands in task 1's scene sequence.

### Opcode 1: actor facing

Opcode 1 (0x9bbc) now dispatches the byte direction to the bound actor.
It clears scripted mode before applying original 0x797b semantics: zero or
unchanged direction preserves the facing; a changed direction enters mode 1.
The scene now tracks actor direction and animation base from INIT.SAV, resets
them with actor initialization, and updates them during diagnostic walking.
For ordinary actors 0..9, the mode-1 update follows 0x7afc: after rendering,
select base+direction-1, reset frame/count/gate, preserve interpolation substep,
and return without another frame advance. Actor 10's special update controller
remains unimplemented as previously documented.

Validation: VM tests cover directions 0..4, missing/rejecting host, truncated
operands and invalid direction/actor. Executing the original direction setter
and deferred update in Unicorn confirms 40 cases across standing/walking bases;
the existing 384 frame/subframe and three reset tests also pass. Pose regression
passes. Actual PS4 build in shadPS4 reaches (104,171), executes actor 0 direction
4, binds actor 1, then stops at opcode 5, task 1 PC 38. Zero unsupported
animation formats. Evidence: `artifacts/sotc/facing-live.log` (local ignored).
Next is opcode 5's one-coordinate actor destination command (0xa184).

### Axis-specific destinations and non-player movement

Opcodes 4/5 use the original shared handler 0xa0d7 (wrappers 0xa17a/0xa184):
4 sets doubled byte X and preserves Y; 5 sets byte Y and preserves X. Both
start movement for the bound actor and use task wait bit 1, as opcode 36 does.
Actors 1..9 now have independent pending destinations. Straight axis-aligned
moves select direction-specific original walking sequences, consume adjusted
frame deltas, check each intermediate footprint against terrain/objects/other
actors, clamp to the destination, and resume their waiting script on arrival.
Collision checks now accept the moving actor ID instead of always excluding
player 0. Invisible actors stop moving; reset clears pending actor movement.

This is bounded diagnostic movement: non-player diagonal/detour requests and
actor 10 remain unavailable; a newly blocked non-player move pauses explicitly.
Exact original transition timing, axis-constraint flags and collision recovery
are not reproduced. The player still uses the earlier diagnostic route planner.

VM tests verify both operand formats preserve the other coordinate, dispatch
the selected actor, yield/wait/resume, and reject truncation. Route regression,
3600 collision oracle cases and 540 movement arithmetic oracle cases pass.
Actual PS4 package under shadPS4: after actor 0 arrives at (104,171), West
(actor 1) walks (62,171)->(62,164) with sequence 4, then ->(98,164) with sequence
5. Both waits complete. Task 1 reaches opcode 82 at PC 48, with zero unsupported
animation formats. Local ignored trace: `artifacts/sotc/actor-axis-live.log`.
Next: opcode 82 and subsequent scene dialogue/event commands.

## 2026-09-23: opening conversation and carriage departure

The supplied R00 room-0 arrival now runs through all six dialogue records,
portraits, the three characters boarding, and four carriage movement cycles.
The PS4 eboot was rebuilt and exercised in shadPS4 with ordinary controller
input, without modifying the original scripts or game files. Task 1 completes
at PC 106; tasks 2, 3 and 4 complete. Task 11 reaches PC 43 / opcode 94 after
24 executed commands. This is the next real boundary: transition/fade setup
and loading room 21 are still unimplemented. No full-game or physical-PS4
playability claim is made.

Implemented in this slice:

- 82/89 dialogue requests and scheduler wait bit 0x40. Text comes from the
  user's E.CC4, nested bank `dialogueBank + 3`. The rolling subtraction is
  original 0x7386/0x72c1: byte minus 84 times its payload-relative position.
  Embedded zero bytes remain line breaks. Nothing is transcribed into source.
- Portrait actor 10 loads the original A00 resources, plays its sequence and
  disappears when dialogue completes. Normal speech restores sequence, frame
  and advance gate. The text-only expression cycling follows 0x7be0.
- Type-10 portrait graphics: bounded vertical run decoder, opaque versus
  transparent runs, per-row X cursors and packet-selected delta coding.
  Original dispatch 0x11636 and blitter 0x11736. Only decoding code is shipped.
- Opcodes 15/16 (target relative to player X/Y), 92 (draw-list eligibility),
  31/32/33 (word assignment/add/subtract), and 6 (task-local counted loop).
  Position aliases 2/3, 5/6, etc. address live actors; global/local variable
  arrays are mutable. Unknown aliases continue to stop rather than fabricate
  values. Arithmetic wraps at 16 bits and restart resets the loop counter.

**Correction to the earlier audio milestone:** opcode 77 is music and opcode
99 is sampled audio, not vice versa. 0x961d calls 0xd5ad through a filename
pointer at DS:423e, which points to MUS.PAK (DS:42d4). 0x9506 explicitly loads
SMP.PAK at DS:4582. The previous binding happened to accept low indices but
stopped on sample 36, K13.VOC, as MUS.PAK has only 15 entries. Both now use the
proper existing player; 255 and busy behavior apply independently. The trace
confirms K13.VOC, K13B.VOC and C7M.VOC as samples and SHADOW7.ADD as music.

Validation:

- `audit-scriptmachine.py`: routing of 77/99 to different hosts, dialogue
  waits/rejected hosts/truncation, 16-bit arithmetic, live coordinate aliases,
  persistent variables, four loop passes across timer yields, relative moves,
  draw eligibility, plus previous scheduler tests.
- `audit-dialogue.py EXE DATA`: 7,683 valid supplied records across E/D/S/I
  match the actual original decryption routine run in Unicorn. Unused entries
  and stale translated tail offsets are excluded; runtime validates each
  requested offset pair so bad unused tail entries do not reject good text.
- `audit-portraits.py EXE UNPACKED_A00`: all 163 referenced type-10 frames
  match the original blitter pixel for pixel, including transparency.
- Previous pose tests and 384 original animation render/update cases, three
  resets and 40 facing transitions pass. PS4 build and package generation pass.
- Ignored local evidence: `artifacts/sotc/dialogue-smoke.log` (tick 351 reaches
  opcode 94), `dialogue-1.png` through `dialogue-6.png`, `vm-dialogue.json`.

UI remains diagnostic: GUI font, a top text panel, eight wrapped lines per
page, Cross/left-click to advance. Original font/layout, speech audio and
original dialogue timeout are not claimed. Text source is currently English.
Portrait resources currently use the loaded archive bank (verified A00 only).
Scene cadence remains 10 Hz and pathfinding remains our bounded route finder.

Continuation: task 11 has `94 40; 99 255; 44 21; 2`. Opcode 94 (0x990a)
sets DS:6096 to its speed and DS:6084 to transition mode 2. Opcode 44
(0x9f59) sets pending destination DS:2dfa (255 maps to -1). Implement room
loading together with these deferred transitions; do not merely suppress 94
or fade to a permanently black screen. Room 21 script 0 begins with opcode
58/21, conditional palette selection (96), terrain setup (22), obstacle setup
(23), actor setup and task activation. Its initialization must execute before
normal room tasks; reusing INIT.SAV for every room would corrupt progress.

### Pointer walking instead of diagnostic single-stepping

Left-click/Cross now starts the same destination route as right-click/Circle;
it no longer pauses and single-steps the scene. Accepted walking input resumes
a user-paused scene, while actual script/render/movement failures remain
blocked. Dialogue clicks still advance pages. The footer describes the new
binding. PS4 build passed; the shadPS4 trace shows single-click routes reaching
(217,164), (200,170), (197,170), and (141,171) automatically. Walking into the
opening conversation region still starts its scripted approach and dialogue.

### Original dialogue speech

Dialogue now reads D00-D07.NAR using the current speech-bank variable and
subtitle index. Sparse offset tables and concatenated VOC segments are
validated before unsigned mono PCM is sent to ScummVM's speech mixer channel.
Advancing past a dialogue stops its voice; missing recordings retain subtitles.
Original data remains external. Music continues independently.

Validation: `audit-speech.py` checked 1,698 recordings, 1,764 VOC segments and
414 empty indices across the seven supplied banks, plus malformed inputs.
The script-machine audit and PS4 build passed. shadPS4 logged active speech
playback for all six opening dialogue lines; the user confirmed audible speech.
Dialogue advancement remains manual, and the subsequent opcode-94 room
transition remains pending. Local trace: `artifacts/sotc/speech-live.log`.
