# Drawer closeup, inventory and legacy music recovery

Room 1166 stopped at byte 84 because its opcode-80 exit has script=-1 and
approach=-1. The original action dispatcher explicitly permits this direct
return (0x7cb6 and 0x7cfd). The native registration, save validator and click
handler now accept it. Clicking the bottom black band (guest Y 358–399) returns
to the bridge; its original hotspot label is EXIT.

Completing initialization creates the key, code book and tape recorder from
original item sprites. These are not new drawings. Closeup scenes without a
player actor now accept item/exit clicks without attempting to walk Ryan.
Opcode 60 skips already-owned items, matching the status check at 0x3032, so
collected items do not reappear on another visit. Old drawer saves stopped on
opcode 80 can resume at that instruction rather than replay initialization.

The inventory is in the top 40 guest pixels and appears only while the pointer
is over that row and normal interaction is available. Move away to hide it.
Hover names for both room objects and inventory items appear below the scene,
at Y 360, separately from the inventory. The preview heading and routine
control text no longer occupy those areas during gameplay. Script errors are
still shown below the scene when no hover name takes priority.

Pickup stores the original hotspot name with the inventory item. Save format
9 preserves these names; formats 1–8 remain readable. Items already owned in
old saves without a recorded name use the fallback "Inventory item".

The local version-4 post-fire manual checkpoint had inactive task 20 at byte
6840 (opcode 101), before the original MUS02/2 load/start. The derived version-6
bridge autosave consequently had no active music, music handle A85=-2 and
bridge stage B105=0. Sonar/environment sounds continued on separate SFX tracks.
Loading the old hold checkpoint now resumes that exact stopped instruction.
For old bridge/drawer snapshots with the same missing-music signature, loading
restores the original post-fire MUS02/2 track (sound resource 310), loops it,
and records its handle. Existing active music and version-9 saves are untouched.
This is a targeted repair of incomplete preview snapshots, not a generic rule
that every room should play the same track.

Validation: the experimental PS4 package builds and guest self-tests pass,
including absent-script exit registration, sparse inventory hit testing and
pickup-name retention. Emulator trace `artifacts/pice/drawer-verified.log`
shows room 1166 initialization completing, all three original object hotspots,
and successful pickups of item IDs 9, 16 and 14. It also records resource 310
starting through legacy music recovery. The user confirmed the result works.
The manual session has no test clicks or timeout. Original soundtrack parity
across later story stages, all exit paths and physical-PS4 behavior remain
unverified.
