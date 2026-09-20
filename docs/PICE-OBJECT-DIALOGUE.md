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
item-specific to OBJ fallback, followed by the common responses described below.

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

- Generic fallback is now implemented; see the continuation below.
- Further opening interactions, dialogue choices, room transitions, death and
  restart, and later chapters remain incomplete.
- This milestone was tested in shadPS4, not on physical PS4 hardware.

The experimental package is `dist/pice-experimental/ScummVM-PS4-PICE-preview.pkg`.
Game files stay separate. The broad stable ScummVM package is unchanged.

## Common response fallback (2026-09-20 continuation)

The original global at data 0x156a points to F_ICETXT.PAK. Handler 0xee8
copies it, 0x3ffd substitutes the language letter, and 0x4008 changes character
2 to T and loads local index 1. Thus these are **common**, not chapter-specific,
responses: E_TCETXT local 1 for the supported English edition.

When the object-specific key is absent, the engine reads MAXACT, MAXLOOK or
MAXOBJ from that resource and selects a zero-based DACT%02d, DLOOK%02d or
DOBJ%02d response using random modulo count, as the original does. The supplied
English resource has 3, 1 and 10 choices respectively. Missing MAX metadata
defaults to one choice; invalid/zero/oversized counts fail safely. Random
selection uses the engine RNG; original RNG sequence parity is not claimed.

Directory validation now distinguishes absent keys from damaged directories.
A present response with invalid page data or audio fails rather than falling
back silently. Existing object and inventory-specific replies retain priority.

Synthetic startup tests cover all three families, modulo selection, missing
MAX metadata, invalid counts, absent selected replies, corrupt directory
bounds, item-specific priority and OBJ fallback. The actual guest restored the
opening autosave, picked up item 15 through hotspot 9, then used it on hotspot
0 through normal click dispatch. Entry 6 selected DOBJ04, displayed the text,
and finished with control returned. No original game strings are in the tests.
Local evidence: `artifacts/pice/generic-response-verified.log`.

The opt-in testbench key `pice_test_use_hotspot` uses the first owned inventory
item on a hotspot after the initial pickup action finishes. It does not create
inventory items or overwrite checkpoints. Normal profiles omit this key.

A fresh post-fire restore also selected the chest's own D09ACT and finished
entry 23 without generic fallback (`artifacts/pice/generic-chest-regression.log`).
