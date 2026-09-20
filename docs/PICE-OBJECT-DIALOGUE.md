# PICE object responses (2026-09-20)

The native PS4 guest can now examine and try to use the chest after the fire
puzzle. Both actions select the original response and finish script entry 23,
returning control instead of stopping at opcode 111, byte 8316 in bank 1140.
This is an interaction milestone, not full-game playability.

## Implementation

Original ICE640 object 2 handler 0xdef reads actor, resource name, local index,
and a base key. It appends ACT for context -6, LOOK for context -1, or tries
O%02d for a selected inventory item followed by OBJ. The helper at 0x1c7ee
is string concatenation. Speech helper 0x4579 uses the existing actor rather
than replacing its animation resource as opcode 129 does.

The runner validates all operands before calling the scene and waits for the
actor's dialogue to finish. The engine resolves object-specific keys through
the existing language directory and page decoder. It supports the per-object
item-specific to OBJ fallback; missing responses still stop as unsupported.

The chest's D09ACT and D09LOOK entries in E_T02 local 3 have a zero voice
flag. They are text-only responses, not missing audio. Text-only pages now
remain visible for 60 ms per character, bounded to 2–10 seconds. This is native
readable pacing; the original configurable text-speed formula is not yet
recreated. Voiced entries retain mixer-driven page completion.

## Validation

The built PS4 guest passed parser self-tests, including key selection,
text-only/multipage decoding, script callback-once behavior, dialogue waiting,
and truncated opcode operands leaving the callback and PC untouched.

Two separate emulator processes restored the post-fire manual checkpoint and
used real hotspot hit testing and approach walking. Left click selected
D09ACT, right click D09LOOK; both logged dialogue completion and
`action finished entry=23`. No unsupported stop occurred in either action.
Local traces are ignored: `artifacts/pice/object-act-verified.log` and
`artifacts/pice/object-look-verified.log`. Original text, audio, saves and
executable data remain outside Git.

The opt-in testbench key `pice_test_right_click=true` changes the existing
synthetic hotspot click to the right mouse button. Restore/click keys are
removed from the normal local profile after testing.

## Remaining work

- Generic chapter fallback: MAXACT/MAXLOOK/MAXOBJ and randomized
  DACT/DLOOK/DOBJ responses when the object's own key is absent. The original
  handler has this path, but its resource selection still needs verification.
- Further opening interactions, dialogue choices, room transitions, death and
  restart, and later chapters remain incomplete.
- This milestone was tested in shadPS4, not on physical PS4 hardware.

The experimental package is `dist/pice-experimental/ScummVM-PS4-PICE-preview.pkg`.
Game files stay separate. The broad stable ScummVM package is unchanged.
