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

The current runtime still restricts room transitions/snapshots to chapter 2 and
several resource operations to A02/B02. Later chapters need real bootstrap,
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
