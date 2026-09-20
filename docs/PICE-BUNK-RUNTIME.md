# Bunk closeup and selected-item cursor

Room 1143 stopped at opcode 153, byte 174, before the actors for Bjorn Hamsun
and Wayne were created. Original handler 0x2108 writes the player movement flag
at 0x2806. The movement code at 0x1b698 bypasses player path requests when the
flag is zero. Native opcode 153 now preserves that state, suppresses floor
walking and skips approach walking in the closeup. Other actors remain usable.
Room changes reset movement permission, and save payload version 10 retains it.
Older snapshots remain readable; an old bunk initializer stopped at opcode 153
can resume from the exact saved instruction.

Opcode-58 actions with script=-1 and an explicit fallback are valid. They are
now registered and serialized, allowing the bunks' object response scripts and
the preceding sleeping-quarters initialization to continue. Matching a missing
primary script still does not invent an action; an applicable fallback must be
selected by the original verb matching rules.

Selected inventory items now replace the pointer with their own sprite, up to
40x34 guest pixels. The icon uses a centered hotspot and the current scene
palette. Two masked compositions distinguish transparent pixels from opaque
palette-zero pixels; an unused palette entry becomes the cursor key color.
Room/save changes invalidate the icon cache so the palette and selection are
refreshed. Deselecting restores normal arrow/hover feedback. Right click in the
scene deselects, as before; clicking the selected inventory icon also toggles it.

Validation: the experimental PS4 package builds and startup self-tests pass.
The emulator completes room-1143 initialization at byte 762, registers Ryan,
Bjorn Hamsun, Wayne and the exit, and runs Wayne's dialogue on a user click.
`artifacts/pice/bunk-figures.png` was visually inspected: both missing figures
are present with the original art. Full conversation branches, later rooms,
selected-icon behavior on physical PS4 and complete game playability remain
unverified. The final cache invalidation and old-bunk restore guards compile;
the active manual process was not restarted for those follow-up guards.

The opt-in `pice_test_room` hook opens a numbered chapter-2 room after a
checkpoint restore. This is a diagnostic shortcut, not evidence that all
intermediate story transitions work. Remove it for normal play.
