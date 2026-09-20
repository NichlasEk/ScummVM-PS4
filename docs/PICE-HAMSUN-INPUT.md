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
