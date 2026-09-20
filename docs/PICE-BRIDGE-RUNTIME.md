# PICE bridge transition and item actions (2026-09-20)

Ryan can leave the post-fire opening through hotspot 3. The original exit
script requests chapter 2, room 7 (bank 1147). The next room loads background
825, palette 826 and its navigation. This milestone implements the first
missing initialization command, opcode 81. The next room is not yet playable:
initialization now stops at byte 4102, opcode 79.

## Item-specific action registration

ICE640 object 2 handler 0x2ee5 reads hotspot, inventory item, approach node,
and script entry. It calls 0x83cb with verb 9 and the item ID stored in action
record offset 8. A hotspot of -1 returns -1 without registering anything.
Opcode 58 shares that absent-hotspot behavior.

Native actions now retain their item ID. Original selector 0x7f21 checks both
verb and selected item for verb 9; an item mismatch skips that record. It only
collects fallback records when the verb differs. Native click dispatch follows
those rules, so using a different item cannot run the first registered item
puzzle accidentally. Matching/registration and truncated-operand tests run at
guest startup.

Snapshot payload version 5 appends one item ID per action. Older versions
1–4 remain readable with item ID -1. New values are checked against loaded
inventory definitions and verb 9 before applying a snapshot. The new reader
has been exercised using the existing version 4 post-fire checkpoint.

The actor click testbench also checks active state and sequence/step bounds;
clicking a removed actor no longer indexes an absent animation. This was found
when probing the captain after he had left the opening scene.

## Emulator evidence and next continuation

The final guest startup passes self-tests and restores the post-fire checkpoint.
Hotspot 3 runs entry 13, walks/animates Ryan to the exit and requests room 7.
Initialization passes the two opcode 81 registrations at byte 2300 and reaches
Driscoll's actor/hotspot before stopping at opcode 79. Local trace:
`artifacts/pice/bridge-item-actions-verified.log` (ignored).

Original opcode 79 handler 0x3469 reads three expressions, an animation
resource string, and three further expressions. Helper 0x1c65e compares the
source chapter/room, positions the player at an entry node, changes animation,
and starts a follow-up task waiting on that actor. The first room-7 call is
source chapter 2, room 0, node 28, animation A02/local 0/sequence 3, task 21.
Implement source-room bookkeeping and these entry actions next; do not replace
this command with a no-op.

Room task 32 also stops at byte 15054 on an expression containing opcode 38
inside a wait. Its random-expression behavior needs implementation. No claim
of completed room-7 initialization, physical PS4 testing or full-game playability.
Original assets and checkpoints remain local and are not distributed.

## Entry animation and random expressions implemented

Opcode 79 now handles source-room matching, player placement/animation and
starting the follow-up script task with a wait on the **player actor ID**.
The return from animation opcode 96 is a sequence ID, so it must not be used
as the actor to wait for. Room initialization continues independently.

Original 0x1c350 copies current room/chapter into previous room/chapter before
installing the new room. Native tracking follows this for the supported chapter
2 transitions. Snapshot payload version 6 retains the previous room, while
versions 1–5 default it to unknown. Loading an old post-fire checkpoint remains
supported; leaving that room establishes the source for the next entry.

Opcode 38 (0x2965) evaluates its bound and returns random modulo bound. Native
support accepts positive bounds and uses the engine RNG, whose seed already
participates in snapshots. A failed expression restores the RNG seed and PC.
Original RNG sequence parity and nonpositive divisors are not claimed.

Guest tests cover nested random expressions, zero-bound rejection and failed
expression rollback, plus entry-task waiting while initialization continues.
The final PS4 guest run restored the post-fire checkpoint, traversed room 7's
arrival from room 0 (node 28, player 0, sequence 3, follow-up task 21), entered
room 9 and returned to room 7 (node 0, sequence 2, follow-up task 38). It wrote
a version-6 autosave (17207 payload bytes in this run) and played Ryan's next
scripted dialogue, key 1210 / voice resource 132, through completion.

Local trace: `artifacts/pice/room-entry-verified.log`. This is emulator evidence.
Room 7 still stops initialization at opcode 80, byte 4460. Task 38 now reaches
ordinary dialogue opcode 52 at byte 16310. These remain real blockers; the room
is not yet fully playable. The earlier random-expression stop in task 32 no
longer occurs in this run.

The autosave is now the newer Driscoll checkpoint. The manual slot remains the
post-fire checkpoint. Test/restore config overrides are removed after testing.

A fresh emulator process restored the version-6 autosave in bank 1147, played
key 1210 to completion again and reached the same opcode-52 stop. Local trace:
`artifacts/pice/room-entry-restore-verified.log`.

## Driscoll replies, subtitles and repeatable checking

Ordinary actor speech (opcode 52, original handler 0x2cff) is implemented.
It shares the page/audio player with object responses (111); both original
handlers call 0x4579. The script waits for the actor's reply before continuing.
Guest tests cover ordinary speech dispatch-once and waiting behavior.

Restoring the Driscoll checkpoint now plays the exchange through Driscoll's
keys 1220 and 1235, with both completions observed. Task 38 next stops at
byte 16416, opcode 68. Room initialization's opcode 80 remains unfinished;
this dialogue milestone does not make room 7 fully playable.

Dialogue captions now wrap and normally render at twice GUI-font size on a
high-contrast panel at the bottom. Long pages use the smaller font to fit more
lines. Palette contrast is chosen from the current screen palette. Voiced
pages remain tied to mixer completion; text-only replies retain their reading
time. This is native subtitle presentation, not original font/layout parity.
The Driscoll subtitle was visually checked in shadPS4; local screenshot:
`artifacts/pice/subtitles-driscoll.png`. Movie-caption support is not part of
this change.

Run `python3 scripts/pice/check-driscoll.py` with the local room-7 autosave.
It builds and runs a bounded 40-second emulator session, checks startup tests,
checkpoint restoration and completion of both Driscoll replies, and writes
`artifacts/pice/driscoll-check.json` plus logs. The report lists script stops
separately from the observed dialogue checks. It restores the previous config
and autosave on exit and refuses to overlap an already-running PICE emulator.
No saves, game text, audio or screenshots are committed.

This automates reproducible execution and identifying the next blocker.
It does not infer missing opcode semantics or prove puzzle correctness,
complete game coverage, subtitle readability, or hardware behavior. Those
still need original-code analysis and relevant interaction/visual checks.

## Clickable conversation menu

Opcodes 68/69 enable and disable individual original menu options. Opcode 62
opens the enabled options and waits for a click. The selected original index
is exposed through context expression 46/6. Dispatcher state 6 at 0x5a7 skips
the following four-byte cancel jump after a successful selection; retaining
that jump incorrectly bypassed the answer branches in the initial test.

The native menu is displayed at the upper right with doubled GUI text. Click
routing takes priority over room/inventory clicks, including while scripted
input locks are held. Hidden entries do not shift the returned original index.
The Driscoll resource has Weapons=1, Hamsun=2 and Lloyd=6.

Startup tests check enable/disable bounds, clicks outside the menu, sparse
visible rows and the returned original index. Snapshot payload version 7 adds
option-enabled flags and last selection; versions 1–6 initialize options to
disabled. Saving while a menu is open is currently refused. Large menus do not
yet have scrolling, and original font/positioning parity is not claimed.

Local runs reached the menu without the former opcode-68 stop. An automated
option-2 click exposed the cancel-jump issue, which was then fixed from the
original dispatcher. The subsequent manual run has no automatic selection and
no timeout, so the user can choose. Reopening the dialogue through the room's
Driscoll hotspot still reaches an unsupported expression at bank 1147, entry 1,
byte 5966 (opcode 91/86 inside conditional 21); that is a separate remaining
blocker. Room initialization's opcode 80 remains unfinished as before.

The optional testbench key `pice_test_menu_option` selects one original option
through normal menu hit testing after a two-second display delay. Omit it for
manual play. The screenshot `artifacts/pice/driscoll-menu-manual.png` is local
only; no original text/assets are distributed in Git.

The subsequent manual session selected Weapons (1), Hamsun (2) and Lloyd (6).
Each played its own Ryan/Driscoll exchange through completion and reopened the
menu: keys 1520/1530, 1240/1250 and 1500/1510 respectively. Repeating Lloyd also
returned to the menu. The enabled options match the user's original screenshot.

Right click or Escape now cancels the menu instead of quitting the engine.
Cancellation retains the following jump, while a selection skips it, matching
original dispatcher state 6. The panel includes a close hint. Startup tests
verify cancellation clears the active menu and returns selection -1. The manual
emulator is left open without a timeout or automatic selection for user testing.

## Reopening Driscoll and completing bridge initialization

The conditional at byte 5966 is now decoded rather than skipped. Opcode 91
(handler 0xb1a) is Boolean NOT. Opcode 86 (0x8af), kind 2, tests inventory
record status == 1; native ownership mirrors that state. Other state-table
kinds remain explicitly unsupported. Tests cover present/absent items, nested
predicates, missing state providers and invalid IDs without advancing the PC.

Opcode 80 (0x34e8) registers an exit action with hotspot, approach node, script,
chapter and destination room. The former stop at byte 4460 prevented later
hotspots from being registered: the drawer, radio, intercom, ladder, periscope
and electrical box. These now exist on a fresh visit. This repairs interaction
registration; it does not add guessed artwork to the background.

Exit actions retain their destinations (snapshot version 8; versions 1–7
remain readable). After successful completion of the selected exit script,
the native action dispatcher requests the destination. A failed script does
not transition. The registered exits have not all been played through; their
remaining script instructions can still stop safely as unsupported.

When loading a version 1–7 bank-1147 snapshot whose inactive initializer is
still positioned on opcode 80, the loader resumes that exact instruction.
It does not restart the initializer or overwrite the player's variables.
Existing initialized scenes are not replayed. This recovers the local older
autosave without requiring the player to redo the opening.

Validation: new-room initialization registers the drawer and other missing
hotspots. The old room-7 autosave resumes initialization at byte 4460, plays
both Driscoll replies, opens the menu, accepts an outside click to close it,
and reopens the menu after a new click on Driscoll. No room-script stop occurs
in that bounded replay (`artifacts/pice/reopen-verified.json`). Startup tests
pass, including inventory ownership, exit metadata/bounds and guarded resume.

`python3 scripts/pice/check-driscoll.py --reopen` reproduces the close/reclick
flow with a 55-second limit. The test-only `pice_test_close_menu` injects one
outside click after two seconds. Omit it and all `pice_test_*` keys for manual
play. Full-game completion and physical-PS4 validation are still pending.
