# Native PICE save/load checkpoint (2026-09-20)

The experimental PS4 runtime implements the opening's autosave, a manual
checkpoint, and restoration into a fresh emulator process. This is a native
preview format, not an import/export implementation of the original DOS saves.
The engine is still not a complete playable game.

## Controls and files

- **F5** writes one manual checkpoint.
- **F7** loads the manual checkpoint, falling back to a valid autosave.
- The opening's `SAV00` request writes the separate autosave automatically.
- Saving during speech/closeup dialogue or a pending room transition is rejected.
  This avoids pretending those unsupported restore states are implemented.
- The standard ScummVM save manager owns the files. For the current `pice`
  testbench target they are `pice-pice-slot0.sav` and `pice-pice-autosave.sav`.
  Host location: `.tools/xdg-pice/shadPS4/data/scummvm/saves/`.

There is no save-slot selection dialog or launcher load integration yet. F5/F7
are preview controls. The stable PS4 port, DOTT package and USB are unchanged.

## State and validation

The envelope contains `PICESV01`, payload length and FNV-1a corruption checksum.
Payload version 2 adds music state; version 1 checkpoints remain readable.
The bounded codec rejects truncated values, oversized collections, invalid
booleans/narrowed integers, invalid resource references and inconsistent state.

State includes A/B variables, script PCs and wait states, exclusive execution,
actor slots and animation/path progress, inventory objects and panel, world
items, hotspots/actions, menu contents, the navigation graph, palette/pulse,
text colors, input locks, camera/player selection and random seed. Graphics
and scripts are restored using original resource IDs and content hashes.
The save does not embed image or audio resource payloads.

The loader reconstructs and validates a candidate before replacing live state.
It does not run room initialization over restored variables. Sound handles are
recreated from resource IDs, original loop counts, volume and elapsed time.
Finite and infinite loops resume within their current iteration. Music uses a
separate mixer channel and participates in version-2 snapshots.

Original opcode 152 (0x20cf) queues the save; the main-loop consumer at 0x18f9c
calls 0xe504 after the script scheduler. The native engine likewise advances
the requesting PC and saves at the end of the dispatcher visit. Re-loading
the opening autosave resumes bank 1140, task 20 at **byte 6740**, rather than
executing opcode 152 again.

## Additional progress after the autosave

- Opcode 101 stops/releases a sound and removes its animation bindings.
- Opcode 134 plays a sound once, then waits for mixer completion. Sound waits
  survive save/load without re-triggering the sound.
- Opcodes 32/104/118/33 load, release, loop and adjust music. The local executable's
  PCM music path (0x1594e) changes `MUS02` to `WUS02`; the decoded local name table
  maps WUS02 to KSOUND 308..313. The engine uses these original WAV resources,
  including track 310, instead of implementing the alternative HMP MIDI path.
- Actors and world items can be created after room initialization, using the
  live runner's navigation graph rather than an initialization-only pointer.
- Action opcode 58's third argument is an **approach node**, not an inventory
  item. Handler 0x2e89 passes it to field 0x0a in 0x83cb; the item field at 0x08
  receives -1. The earlier internal field name was corrected; its serialized
  position/value are unchanged.

## Verification and continuation

Guest startup tests cover bounded state IO, every truncated scalar/string
prefix, script snapshot round trips, rejection without replacing the previous
scheduler, animation-trigger waits, sound waits, and deferred-save argument
parsing. Existing movie, sprite, lip-sync and navigation tests also run.

The first full opening run wrote a 17651-byte version-1 payload and continued
past the old opcode-152 stop. A separate emulator process restored it with
11 actor slots, 18 inventory objects and task 20 at byte 6740, then continued
through the same subsequent animations. Local evidence:
`artifacts/pice/restore-checkpoint-verified.log`.

A version-2 manual checkpoint was then written while music was playing:
18016-byte payload, checksum `bf8f4e94`. A second fresh emulator process loaded
it at task 20 byte **7188**, with music active and elapsed time **1518 ms**.
Local traces: `artifacts/pice/music-checkpoint-write-verified.log` and
`artifacts/pice/music-checkpoint-restore-verified.log`. This verifies the native
state/seek path; no human listening comparison of sample-accurate timing is
claimed.

For repeatable tests on a locked desktop, these **PICE_TESTBENCH-only**, opt-in
configuration keys are available in the local `[scummvm]` profile:

- `pice_restore_autosave=true`: load the autosave on startup.
- `pice_restore_slot0=true`: load the manual checkpoint instead.
- `pice_test_save_after_music=true`: request a manual checkpoint 1.5 seconds
  after starting music, once the current script instruction has completed.

These are test hooks, not gameplay defaults. The normal profile leaves them off.
Use `EMULATOR_TIMEOUT=130 scripts/pice/run-emulator.sh` for a full opening run,
or a shorter timeout with a restore hook to continue directly from a checkpoint.

With no player input, the timed sequence now advances through the monster's
appearance and stops at bank 1140, task 28, **byte 8462, opcode 131(1)**.
Original handler 0x19ba stops room bank slot 0 and starts entry 1 in global bank
slot 1. The chapter's global bank is resource 1138; its entry 1 plays
`sdead.mux`, then executes opcode 132. Implement that bank transition and death
flow, not a silent no-op. No complete death/restart flow is claimed yet.

The more important gameplay work is click dispatch: hover hit testing, action
selection, approach walks, inventory pickup/use, and script context expressions.
The timed opening must allow the player's actions to avert the scripted outcome.
Those systems, later chapters, dialogue choices and a full playthrough remain
unfinished. No physical-PS4 verification of this engine checkpoint is claimed.
