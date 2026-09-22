# Native dialogue presentation

PICE reads the decorative S_FONT1 font (KSVGA resource 1318) and the
S_CURSOR sprites (1315) from the player's original game archive at runtime.
No extracted font, cursor bitmap, palette or game screenshot is shipped in
source or the PKG. Pointing-hand frame 26 and arrow frame 25 use the cursor
resource's own palette.

The font reader validates the 8-byte header, 10-byte glyph records and raw
indexed glyph bitmaps. DOS850 game text is mapped to Unicode for ScummVM.
Transparent pixels, outline pixels (96) and fill pixels are handled separately.

## Speaker colors and position

Original opcode 135 (0x1aac -> 0x1aab5) sets an actor's outline and fill
palette indices. The engine had previously stored those two numbers in an
unused field named labelPosition. They are now interpreted as colors, and
resolved against the current scene palette rather than hardcoded by actor ID.
For example, bridge scripts assign Captain Lloyd 128/139, Ryan 131/124 and
Driscoll 56/47. Opcode 136 supplies the six independent UI text styles;
the defaults follow original routine 0x40db. Narration uses style 2.

Caption layout preserves the authored line breaks. Actor speech is centered
above the current scaled sprite, taking camera scrolling into account;
narration/close-ups use the script's text anchor. The viewport clamping follows
original routine 0x43b7. Sprite frame bounds provide the actor's top edge;
this has not been proven pixel-identical to every original animation's
internal text offset. Very long lines wrap, and exceptionally tall text falls
back to the smaller GUI font to remain readable.

Dialogue choices use the native font, scene-derived gold/highlight colors,
a framed black panel and the original pointing hand. Picking and rendering
share menu bounds and row height. Border RGB values are a visual approximation;
this is not a claim of fully pixel-identical original menus.

## PS4 viewport and saves

The existing 640x400 logical composition is centered vertically in a 640x480
viewport. Inventory and scene input are transformed consistently by 40 pixels,
including menus and test event injection. This moves the inventory inward from
the television edge without changing world coordinates or save positions.

Save format 15 retains the existing actor color field layout. Older saves
remain readable; their unset color/style defaults are upgraded. Dialogue
anchors are transient because active dialogue is not serialized as a resumable
speech channel. New game resets the default UI styles.

## Validation

The actual PS4 PICE target built and booted in shadPS4. Startup self-tests
passed, including synthetic font decoding/truncation, outline/fill remapping,
invalid actor color rejection and menu picking. New game progressed through
the intro to the bridge and executed all three actors' native color assignments.
An existing save loaded and a format-15 snapshot round trip passed.
The generated PKG manifest contains engine/support files and games/README.txt,
not original PICE resources. Physical PS4 visual/controller verification remains.

Optional test-only pice_debug_snapshot_ui includes the composed 640x480 UI
in pice_debug_snapshot_path instead of the default world-only snapshot.
Local evidence: artifacts/pice/native-interface-build.log,
native-colors-check.log, native-colors-restore.log and native-normal-start.log.
A subsequent combined test was misconfigured: it restored chapter 12 and
requested room 6, entering a non-saveable cinematic (bank 1300). Its immediate
round-trip assertion failed with zero snapshot bytes. This is not evidence of
a successful round trip there; the successful round trip above was bank 1140.
The interactive profile was cleaned of restore/test flags and restarted through
the ordinary title menu/new-game flow.

## Radio close-up wait correction

Opcode 128 uses the independent narration channel (original scheduler state
5). Its returned -27 marker was incorrectly converted with the actor-speech
encoding, becoming -1 (no wait). Room 22 could therefore enter the bridge
before voice 107 finished. The runner now waits on narrationBusy(), as opcode
122 does. A synthetic regression keeps the PC fixed over ten scheduler visits
while the voice is active, then verifies release after completion.

The rebuilt PS4 target passed its tests under shadPS4 on an isolated Xvfb
display, leaving the interactive game alone. The full new-game path reached
room 22, waited during closeup 1020, logged dialogue completion, and only then
entered bridge bank 1146. Evidence: artifacts/pice/radio-hidden-check.log and
radio-wait-build.log. No physical-console audio check has been performed yet.

## Native inventory slots

The inventory now paints the panel already loaded by opcode 137 from OBJECT
8 (KSVGA resource 1314): sequence 0 is the blue slot and sequence 1 the grey
selected slot. Other script-selected pairs use the same base/base+1 rule.
The original 40x40 artwork is decoded at runtime. Icons retain their aspect
ratio within a centered 32x32 area, with transparent coverage preserved so
they cannot erase the frame. The old selection underline is removed.
Rendering, pointer hit testing and test input now share 40-pixel slots, up to
16 across the 640-pixel viewport. Slot-edge picking checks cover 39/40/79/80.

The PS4 package builds and guest self-tests pass with the new slot-edge checks.
The copied autosave used for the hidden visual check entered a locked bridge
cutscene, so that run did not reach the inventory-selection assertion. The
interactive emulator was opened with the new build for manual visual checking;
do not treat the hidden run as verified selected-slot gameplay.
