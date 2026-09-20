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
