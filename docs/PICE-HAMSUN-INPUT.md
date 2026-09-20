# Hamsun: inventory during speech and Nordic text

Room 1143 task 2 intentionally repeats dialogue key 1050 while waiting for the
player. The preview incorrectly disabled all inventory and scene interaction
while that background speech played. Move the pointer to the top edge to open
the inventory, select the medal, then click Hamsun. Ambient speech can now be
interrupted by the same click that performs the action. Foreground conversations
and explicit script input locks remain protected. Competing speech instructions
wait without advancing their PC or terminating the task.

The subsequent medal action exposed missing opcode 124 at byte 2856. Original
ICE640 handler 0x1379 takes no arguments, clears the selected inventory ID at
0x15c4 to -1, and returns zero. Implementing it lets the original script enter
room 1150 and return to the bunk, now playing key 1070 instead of 1050.

Original resource strings are DOS code-page bytes (the reported line uses 0x84
for ä), not Unicode. Subtitles, menu choices and hover/item names now decode
CP850 into Unicode for the built-in GUI font. It already contains the Nordic
glyphs. Game strings in snapshots remain unchanged, preserving old saves.
Tests cover ÅÄÖåäöÆØæø and ASCII, as well as speech-channel contention.
Spelling already present in the original resources is preserved.

Verification on September 20, 2026:

- PS4 experimental package rebuilt; guest parser/engine self-tests pass.
- `artifacts/pice/ambient-check.log`: click the bottom exit during key 1050;
  room 1142 loads successfully.
- `artifacts/pice/medal-ambient-fixed.log`: pick up actual item 10 in room 1142,
  open the inventory during key 1050, select it through a mouse event, click
  Hamsun with context 10, enter room 1150, return and play key 1070. No script
  stop occurred in this replay.
- `python3 scripts/pice/check-hamsun.py` reproduces the medal replay using a
  separate profile and a copied bridge autosave. It adjusts story variables
  only in the test copy, so this is a focused regression, not a full walkthrough.
  Leave the bounded test window alone while it runs.

Normal profile saves are untouched. Original game data and test artifacts stay
ignored. Physical PS4 behavior and later puzzles have not been verified here.

## Wayne's conversation and the full inventory

The focused medal-only fixture skipped Wayne's conversation. Actual play reached
opcode 99 at room 1143 task 1, byte 2262, and stopped before its remaining input
lock was released. Original handler 0xd0f calls 0x94da: close the conversation
menu, clear context 6 to -1 and signal completion. The implementation latches a
cancellation for the next menu poll so the script follows its cancel branch,
finishes cleanup and unlocks inventory input instead of reopening the menu.
A regression checks that the cancellation is consumed once, then the menu can
be opened normally again.

Run `python3 scripts/pice/check-hamsun.py --full-inventory` for the broader replay.
It prepares five items through the original pickup commands in the drawer and
sleeping quarters, plays Wayne's Hamsun/Lloyd choices, selects the medal through
the inventory, then selects the recorder after Hamsun changes his response.
The fixture skips walking to the pickups and remains confined to the copied
profile. Its automatic clicks must not be enabled for manual play.

In the PC emulator use **Q to save and U to load** (mapped to **L1/R1**).
The engine receives these as F5/F7 via its gamepad keymap. Pressing physical F7
on the PC toggles shadPS4 mouse capture; physical F5 is also an emulator hotkey.
Earlier instructions to press physical F5/F7 in shadPS4 were incorrect.

Returning to the sleeping quarters exposed a second bug: initializer opcode
110 at byte 1610 rejected action ID -1 for the already-collected medal. Original
0xddc explicitly accepts -1 as a no-op. Without that, initialization stopped
before creating the door hotspots. This sentinel now succeeds, and snapshots
stopped at exactly that initializer instruction can resume it without resetting
inventory or story variables. Initialization failures are now visible/logged,
and the regression checks them as well as later task stops.

The full replay also clicks the left door after recording Hamsun and requires
an actual transition to bridge bank 1147. Its isolated manual checkpoint retains
the collected items and recording state. Host Q/U keypresses were verified by
separate checkpoint-saved and checkpoint-restored log entries in shadPS4.

Fixing initialization revealed the actual immediate return-to-bunk loop. Opcode
79's continuation was previously started as an ordinary background task. Original
0x1c728 selects that entry task, and scheduler 0x6ac visits it alone until it
finishes. Room 1142 task 18 must move Ryan from node 9 to node 11 before region
watcher task 17 resumes; otherwise task 17 detects the spawn position inside the
bunk hotspot and changes back to room 1143. Entry continuations now take scheduler
ownership after synchronous initialization, releasing it when they finish.
Existing snapshot exclusive-task storage preserves an in-progress continuation.

The restored room then reached its original autosave for the first time. Snapshot
validation incorrectly rejected fallback script -1, which the room deliberately
uses for a hotspot without a fallback action. Snapshots now accept that sentinel;
input does not treat it as a callable fallback. An autosave failure retains the
gameplay view and displays an error rather than switching clicks into the asset
browser. The full regression checks for invalid snapshots and autosave failures.

Latest full replay: all checks except `no_script_stop` passed. Medal use, recorder
use, return to sleeping quarters, autosave, and the left-door transition worked.
Bridge re-entry then stopped initializer 0 at byte 2548 (wrapper 6, assignment
to A47 calling opcode 58 with B69). B105=1 skips recreating Wayne's hotspot,
leaving a stale B69 handle from another room. Original handler 0x2e89 only
special-cases -1; do not silently accept arbitrary invalid hotspot IDs. Next
step is to reconstruct original room teardown/handle invalidation semantics.
Evidence: ignored `artifacts/pice/hamsun-full-result.log` and `hamsun-check.log`.
This is not a clean full-regression pass or a full-game compatibility claim.
