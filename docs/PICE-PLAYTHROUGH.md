# PICE playthrough and regression ledger

Work toward a complete game, using the PS4 binary in shadPS4 as the first
runtime testbench. A successful script step is not proof that a puzzle,
animation, inventory change or chapter transition behaves correctly.

## Method

1. Preserve the last usable checkpoint in an ignored, isolated test profile.
2. Reproduce the next failure by walking/clicking/using the required item.
3. Trace the original instruction and its callees before implementing it.
4. Check bounds, waits, cancellation, resource lifetime and saved-state behavior.
5. Replay from the preceding checkpoint, checking observable story progression.
6. Keep manual play free of automatic test clicks and time limits.

No missing opcode should be skipped just to advance a scene. Metadata and
reproduction code belong in Git; commercial data and local snapshots do not.

## Coverage

| Segment | Evidence | Remaining coverage |
| --- | --- | --- |
| Opening, hold and fire | Existing opening/fire runtime notes and user play | Full continuous replay and alternate actions |
| Bridge dialogue and drawer | Driscoll replay, drawer notes, user play | All dialogue branches and revisits |
| Sleeping quarters and Hamsun | Full-inventory replay: questions, medal, recorder, return | Continuous pickup route and alternate puzzle order |
| Bridge return and recorded incantation | `check-bridge.py --recorder` | Follow its latest report; a failed check is still a blocker |
| Remaining submarine sequence | Not yet verified end to end | Continue from bridge replay |
| Later chapters and ending | Script tables indexed, gameplay unverified | Chapter loading, resources, puzzle paths, recovery and final sequence |

The original runtime restricted room transitions/snapshots to chapter 2 and
several resource operations to A02/B02. The chapter-3 follow-up below extends
that boundary. Further chapters still need real bootstrap,
resource-group and persistent-state support. Merely enabling more opcodes will
not make the whole game playable. The existing 155-bank/1,499-entry index audit
and isolated prefix probes are discovery tools, not a playthrough.

## Replays

`python3 scripts/pice/check-hamsun.py --full-inventory` creates a copied fixture,
collects five objects through pickup commands, runs the Hamsun interaction and
saves a checkpoint in the sleeping quarters. It skips walking to those pickups.

`python3 scripts/pice/check-bridge.py --recorder` uses that isolated checkpoint,
clicks the door, waits for the Prisoner to appear, and uses the recorded tape
on Ryan (the original self-use action). It rebuilds the PS4
target and emits `artifacts/pice/bridge-check.json` plus a detailed log. The
source checkpoint must remain byte-for-byte unchanged. This focused replay is
shorter than restarting the entire Hamsun conversation for each new blocker.

Both runs currently show a visible emulator window and have a time limit.
Manual clicks can change the replay; don't mistake that window for the manual
session. Passing these bounded checks does not certify the full game or PS4
hardware.

## Original-code findings, 2026-09-20

- Bridge initializer byte 2548: opcode 58 calls 0x83cb, which records an action
  against a fixed hotspot slot even when inactive. 0x851d clears hotspots on
  room teardown; it does not clear the scalar B table. Keep slot IDs bounded to
  0..49, without inventing a clickable object. Saves must accept dormant slots.
- Bridge task 17 byte 13292: opcode 115 calls 0xb940, freeing an animation/sound
  binding in place. Actor/sound removal also invalidates slots in place
  (0xb97a/0xb955); opcode 109/0xb8ea reuses the first free slot. Compacting the
  vector corrupts later references. Native empty slots use canonical defaults.
- Bridge task 11 byte 12084: opcode 122/0x12a8 plays positioned text/voice on
  the independent dialogue channel, waiting in state 5. It must not replace or
  restore an actor animation. The recorded incantation is key 1320, voice 143.
- Room 1167 task 1 byte 106: opcode 128/0x1625 also waits on state 5. The two
  arguments following its animation resource are text anchors, not fixed
  dimensions. The Ryan scene uses 0,0 and has no camera actor; Hamsun uses
  320,480. Native captions retain their existing shared readable placement.

Check the current replay report for the next unresolved stop; record a new
checkpoint only after the transition and its inventory/story state are sound.

- Room 1167 byte 178: opcode 95 mode 7 fades palette entries 0..239 toward
  black asynchronously. Original 0x755e/0x7873 sets step and period from the
  requested duration; the script separately waits before entering movie room
  1177. Native saving is unavailable during this transient fade.
- Bridge task 37 byte 15812: opcode 95 modes 5 and 6 are verified no-ops in
  this executable's jump table at 0x7536 (as are modes 1 and 2).
- Movie audio: original 0x1644b/0x16450 suspends music/SFX, and
  0x16548/0x1654d resumes the previously active channels. Native playback now
  pauses individual existing handles, leaving the movie's own PCM audible,
  then balances those pauses on completion or quit.
- Early recorder input exposed a partially initialized room: the initializer
  yielded before starting the monster sequence, yet input became available.
  Original initializer loop 0x651 completes before ordinary scheduling. Room
  initialization now owns the scheduler across waits, including restored
  partial initialization. This needs both the immediate-use probe and normal
  monster-triggered replay; waiting in a fixture alone is not an engine fix.

## Latest verified boundary

The normal bridge replay reached and completed `Vin1.mux` (181 frames, no
decoder error), paused/resumed four existing audio channels, entered bank 1157
and returned to bank 1147. After adding verified palette no-ops, no room-script
stop occurred during the 120-second normal replay. Its overall report remained
failed because the requested checkpoint in transient room 1157 was never
available. The checkpoint request now targets the returned bridge, but that
new save has not yet been validated. Do not describe it as an all-green run.

The separate `--early-recorder` probe still fails at bank 1147 task 11 byte
12210 (opcode 96, actor B66 absent), despite initializer scheduling ownership.
There is still a window before the monster task's first input lock. Investigate
the entry-continuation/task-start ordering and original input lock ownership;
do not claim that initialization priority alone fixed early use. For the normal
route, wait for the Prisoner hotspot before using the recorder on Ryan.

Manual handoff uses `.tools/xdg-pice-bridge-manual/shadPS4`, restored from the
safe quarters checkpoint after recording Hamsun, with all test hooks removed.
The live session has no timeout. Q saves, U loads; physical F7 is emulator
mouse capture. The original player profile and source checkpoint are preserved.

Next: observe the player's next segment, verify a returned-bridge checkpoint,
close the early-input race, then extend the replay beyond this submarine event.
Whole-game completion remains unverified and later-chapter support remains work.


## Intercom and hold follow-up (2026-09-20)

- Bridge bank 1147, entry 4 byte 10198: opcode 23 increments B111 before
  examining the intercom. Opcodes 23/24 now implement signed 16-bit variable
  increment/decrement, including returned values and wraparound (original
  handlers 0xa38/0xaa2). Startup regression fixtures cover both variable tables,
  both operations, expression assignment and invalid indices.
- Hold bank 1141 byte 764: the initializer failed inside actor creation because
  node 0 requests scale 60. This prevented subsequent actors, including Stanley,
  from being created. Node-based actors and world objects now render at the
  requested percentage; picking uses the same pixel mapping. Moving actors
  interpolate node scales over floor triangles/links without changing saves.
  This interpolation is an implementation approximation; exact original
  perspective interpolation and scaled animation stride still need comparison.
- Sprite checks cover shrink/enlarge, negative placement, clipping, transparent
  pixels and opaque palette zero. Compound frames are composed before scaling.
- shadPS4 hold probe: parser self-tests PASS, initializer progresses beyond
  byte 764 to its scripted wait at 3368; Stanley is created as actor 1/resource 7,
  hotspot label Stanley, and speaks key 504/voice 13. No room-script failure
  occurred in the 40-second run. User reports this room now works better.
- Corrected intercom replay finds the hotspot by label (`An intercom`), since
  post-movie numeric IDs differ. Right-click queued hotspot 7/verb 1/entry 4,
  emitted D02LOOK and completed. An earlier numeric-ID probe entered the hold
  instead; it was not evidence of a successful intercom examination.

Ignored evidence: `artifacts/pice/intercom-label-check.log`,
`artifacts/pice/hold-scale-check.log`. The test profiles are isolated; the manual
profile is `.tools/xdg-pice-bridge-manual`, now containing the player's own
post-movie bridge save. Preserve it. Remaining early-recorder race and later
chapter limitations above still apply.


## Winch archive spelling (2026-09-20)

Hold task 21 stops at byte 6868, opcode 117, with actor A129, archive
`A02.Pak`, local 28, sequence 0, position 0,0. The existing actor animation
handler rejected the suffixed spelling. Animation group matching now accepts
both `A02` and case-insensitive `A02.PAK`, retaining index/sequence validation
and rejecting paths, other groups and extra suffixes. Name regression tests
passed in shadPS4, and the PS4 package was rebuilt.

The timed replay did not verify the winch: manual interactions during the test
left the room before the automatic use. Do not count absence of a script stop
there as progression evidence. Manual testing resumed without timeout or test
hooks from the player's room-1141 save (`.tools/xdg-pice-bridge-manual`).
Evidence: `artifacts/pice/winch-check.log`, `artifacts/pice/winch-manual.log`.


## Winch action replacement (2026-09-20)

User passed the archive-name stop and reached task 21 byte 7378, opcode 84.
Original handler 0x7cb evaluates an action ID and calls 0x84d7, clearing its
active word. The following script creates a replacement action via opcode 58
and sets its fallback with 110. Implemented stable inactive action slots and
first-free allocation (original 0x83cb), with save validation allowing the
canonical inactive record. Click dispatch skips inactive slots. Regression
checks cover removal, invalid IDs, preserved adjacent IDs and slot reuse.

Rebuilt PS4 package; handoff is the normal untimed manual profile. Full winch
completion remains pending player replay. Log: `artifacts/pice/winch-action-manual.log`.


## Icy hold sound binding update (2026-09-20)

User reached bank 1152 task 11 byte 4372, opcode 116, after saving. Original
0x1114 evaluates three arguments; 0xb99e updates sound ID at binding offset +2
and volume at +8. Either -1 preserves that field. Actor, sequence, frame and
fired latch are untouched. Added the invocation and atomic validation, with
regression checks for both replacements, preserve sentinels and invalid values.
Rebuilt PS4 package and reopened the user's normal manual profile with no test
hooks/timeout. Full continuation beyond the icy hold remains a playtest boundary.
Evidence: `artifacts/pice/ice-sound-manual.log`.


## Chapter 3 transition after torpedo movie (2026-09-20)

Bank 1157 task 7 byte 1310 already decodes opcode 34 correctly: it requests
chapter 3, room 0. The engine's chapter-2-only guard rejected it. Added a
separate chapter-bootstrap path (1181), then room 0 (1182). References now
resolve A03 (179..379), B03 (882..1000), SMP03 (113..164), and rendered WUS03
music (314..330). Existing A02/B02 bounds are preserved. These numeric bounds
come from the English CD executable's group directory; game assets stay external.

The introductory scene uses a 1280-pixel background. Rendering now respects
source row stride and draws a 640-pixel viewport; actor/scenery positions and
picking share the camera offset. Camera tracking is currently a simple clamped
actor-centered viewport, not a verified reproduction of original camera easing.

Additional instructions required by startup:
- 30: sample volume, verified handler 0x2615 -> 0x14f6a.
- 64: register paired inventory objects and global script entry (0x314b ->
  0x8680). The registrations are preserved in saves; dispatching combinations
  through the separate global script bank still needs implementation.
- 36: hold the script PC until fresh keyboard/mouse input (0x236c); no-op
  skipping would incorrectly remove the pause. Regression tests cover wait/resume.
- 149: inventory availability flag (0x1fc3, referenced by original inventory
  drawing/input paths); restored as part of save state.
- Fade-in counterpart to the existing black palette fade is supported.

Snapshot version 12 adds inventory visibility and pair registrations. Old saves
remain readable. New chapter bank IDs and music references are validated;
chapter is derived from the bank on restore. Later chapters remain rejected.

shadPS4 startup tests PASS. Isolated chapter-start replay reached banks 1182,
1214, 1184 and closeup 1205 without a room-script failure after these changes.
This fixture enters chapter 3 from a copy of the player's save; it is not an
end-to-end replay of the torpedo movie, and does not establish full chapter
playability. Evidence: `artifacts/pice/chapter3-check3.log`,
`artifacts/pice/chapter3-save-check.log`. Test profile:
`.tools/xdg-pice-chapter3-check`; the player's profile remains separate.

The version-12 write/reload probe remains unverified: its target is still in a
mandatory conversation, where saving is intentionally unavailable. No successful
checkpoint was reported. Preserve the original player's saves; a local backup
exists at `.tools/pice-save-backup-before-chapter3`. Manual handoff returns to
the player's torpedo-room save, not the chapter-start test fixture.


## Letter description, chapter 3 (2026-09-20)

Bank 1206 task 1 byte 656 calls opcode 125 with anchor 320,240, F_M03/4,
base B01. Original 0x138e chooses ACT/LOOK/item keys and generic fallback,
then 0x45e7 starts independent narration (state 5). Added the opcode with
blocking/resume semantics, sharing object key resolution with actor speech.
Regression checks cover dialogue ownership and waiting without replaying text.

shadPS4 isolated letter replay: self-tests PASS; left click selected B01ACT,
right click selected B01LOOK, both displayed text-only narration and completed
entry 1. Evidence `artifacts/pice/letter-check.log`. Original player save remains
separate; manual handoff still restores the torpedo-room slot unless the player
saves later progress. No timed or automatic clicks in the manual profile.


## Chapter 3 hallway actor resource (2026-09-20)

Bank 1186 entry 0 byte 934 failed inside the discarded assignment expression
(opcode 6), when creating A03/88 sequence 1 at node 15. Resource 267 has 70
frames, but unused sequence 3 ends with frame 70. Previously this rejected the
whole resource, including its valid sequence 1. Structurally valid sequence
records are now preserved with per-sequence validity; selecting a sequence with
an invalid frame is rejected through hasSequence. No invented frame or skipped
script operation. Scenery loading also checks sequence validity before indexing.
Synthetic regression covers an invalid sibling followed by a valid sequence;
existing malformed/truncated container and rendering checks remain enabled.

shadPS4 hall replay: parser self-tests PASS; initialization advances past 934 to
2388 (wait opcode 93), without initialization failure. Evidence:
`artifacts/pice/hall-check2.log`. The invalid sequence itself is not implemented
or repaired; this does not prove all interactions or later rooms are playable.

The user had continued playing in the letter-check profile. Its newest Q slot
(version 12, bank 1199) was backed up alongside the older manual saves under
`.tools/pice-save-backup-hall-1789917105` and copied to the normal manual profile.
Loading that checkpoint succeeded in shadPS4. This supersedes the earlier note
that version-12 checkpoint restore was unverified. The isolated hall test uses
chapter/room overrides; those are absent from the manual profile, which resumes
the user's bank-1199 save, with no timed shutdown or automatic test inputs.


## Office exit after the safe, chapter 3 (2026-09-20)

Bank 1184 task 26 byte 9018 tests opcode 86 kind 3, task 27, against zero.
Original handler 0x916 reads the current script bank's task-active byte (28-byte
records). Added this predicate against the room runner; waiting tasks count as
active, stopped tasks do not. Invalid task IDs and missing runners are rejected.
Regression checks cover inactive/start/wait/completion and invalid indices.

Built PS4 package and ran shadPS4 from a copy of the player's Q save (bank 1184,
8 actors). Self-tests PASS. Automated click on exit hotspot 1 ran task 26 and
transitioned to hallway bank 1186 with no repeat of the 9018 failure. Evidence:
`artifacts/pice/office-exit-check2.log`. This verifies the saved exit route, not
all subsequent chapter interactions. Player saves backed up under
`.tools/pice-save-backup-office-exit-1789918444`. Manual restart uses the original
Q save, with no test click or timed shutdown.


## Ward after elevator: palette-free animation (2026-09-20)

Bank 1199 initialization byte 1076 assigns an actor from opcode 77, A03/179,
sequence 0 at 0,0. Archive resource 358 has a zero palette offset: its sequence
section ends at EOF, without the otherwise usual 768-byte palette trailer.
The reader now accepts this form and leaves its palette empty; scene rendering
continues to use the room palette. Standalone sprite preview rejects resources
without their own palette, preventing out-of-bounds palette access.
Synthetic checks cover parsing/rendering this variant and truncated sequences.

Built package; shadPS4 self-tests PASS. Isolated room-17 initialization from a
copy of the player's bank-1183 Q save passes 1076 and reaches 3366; no repeated
initialization failure (`artifacts/pice/ward-check.log`). This direct room entry
is not proof of the entire elevator sequence. A separate elevator-click probe
restored the save but did not activate the exit hotspot within its test window.
Player save backed up under `.tools/pice-save-backup-ward-1789919522`; manual
profile retains that save and has no automated click or room override.


## Doctor reception coordinate predicate (2026-09-20)

Bank 1196 task 20 byte 3320 compares opcode 73(A0) with 300. Added actor X/Y
expression reads (73/74), matching original handlers 0x32c8/0x32f4 and their
anchor fields +3/+5. These are room coordinates, independent of the camera.
Invalid/inactive actor references fail safely. Tests cover both axes, signed
coordinates, invalid actors and nested greater-than expressions.

Built package and replayed the player's bank-1196 Q save in shadPS4: self-tests
PASS. Automated hotspot 1 and dialog option 11 reproduce the reported route;
actor 1 walks to nodes 7/12, becomes Miss Trend, then returns to node 8 without
the task-20 stop. Evidence: `artifacts/pice/doctor-check2.log`. Added an opt-in,
one-shot `pice_test_menu_choice` hook that validates menu bounds/enabled state.
Manual profile does not enable it. Later doctor interactions remain unverified.
Save backup: `.tools/pice-save-backup-doctor-1789921352`.


## Manual theft input window (2026-09-20)

Surgery bank 1197 task 12 explicitly unlocks input at byte 4116, then waits
1000 ticks before starting the doctor's return. The frontend incorrectly kept
all hover/click/inventory blocked because the initiating action still owned
input throughout that timer. A successful step that releases the last input
lock now detaches its own foreground action without stopping its script. Other
tasks cannot detach it; nested locks, queued walks and deferred exit actions
retain ownership. Regression checks cover owner, lock and exit guards.

shadPS4 self-tests PASS. Isolated replay from a copied save enters room 15,
uses item 11 on the doctor, then clicks manual hotspot 4 after the unlock.
Log confirms action 12 releases input, action 10 starts and inventory pickup
item 8 succeeds before returning to reception. Evidence:
`artifacts/pice/manual-input-check.log`. Test-only initial item and follow-up
hotspot options support reproduction; neither is enabled in the manual profile.
The user's Q save is preserved (backup
`.tools/pice-save-backup-manual-input-1789923737`) and restarted on request.


## File-room closeup entry without Ryan (2026-09-20)

Bank 1195 entry 0 byte 102 uses opcode 79 for arrival from chapter 3 room 12,
with no continuation (-1), although this closeup has no player actor. Original
0x1c683 skips actor placement for player slot -1. The scene sink now permits
this no-actor/no-continuation entry while still rejecting invalid positive
actor slots. Actorless entry continuations remain unsupported, rather than
silently dropping their task. Regression covers the absent actor and invalid
positive reference cases.

shadPS4 self-tests PASS; copied Q save restores bank 1194, then room 13 entry
passes byte 102, registers 'To the armoury' and 'A drawer', and completes at
306 without initialization failure. Evidence `artifacts/pice/file-room-check.log`.
Drawer interactions beyond initialization remain for playtesting. Original Q
save backed up under `.tools/pice-save-backup-file-room-1789924509` and retained
for manual restart; no room override in the player's profile.
