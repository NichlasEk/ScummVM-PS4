# Sleeping quarters: medal pickup and background tasks

The medal was already created before the reported opcode-130 stop. The local
emulator test clicked its opaque sprite pixel at guest (571,297), walked Ryan
to node 17, and completed the pickup of inventory item 10. The user confirmed
that the corrected interaction works.

Three missing commands are implemented from the original executable:

- 130, handler 0x18a1: load a random idle animation with per-actor sequence and
  pause ranges. It is independent of speech. Wayne's task uses resource 110,
  sequences 0–1, pauses 50–150 before continuing its timed routine.
- 89, handler 0x2fc4: create an unnamed rectangular hotspot. This clears the
  initializer's former stop at byte 3086. No-effect actions with neither a
  primary script nor fallback may exist and do not produce a hand cursor.
- 112, handler 0x106c and dispatcher state 9: latch actor, region and desired
  inside/outside state, then wait. Mode zero waits for entry; other modes wait
  for exit. The original helper at 0x10 uses strict rectangle boundaries.

Save payload 11 retains per-actor idle state. The script state reader accepts
both the earlier 36-byte task records and new 48-byte records with region-wait
fields. Parser self-tests include a waiting region task that is saved, restored
and released by entry. Old sleeping-quarters previews stopped at the known
initializer offsets, opcode 130 in task 19 or opcode 112 in task 17 can resume
those exact instructions; room initialization is not replayed from the start.

Evidence: `artifacts/pice/medal-verified.log` records startup tests passing,
room 1142 initialization completing at byte 3224, Wayne's idle animations and
the medal pickup. No room-task error occurs in the final bounded replay.
The debug-room jump used the existing autosave's variables, so it does not
prove every preceding story transition. Later puzzles and physical PS4 remain
unverified. Original game assets, saves and logs are not distributed in Git.
