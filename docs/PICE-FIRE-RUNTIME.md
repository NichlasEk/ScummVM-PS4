# PICE fire puzzle and dynamic navigation (2026-09-20)

The native PS4 engine completes the opening's extinguisher action: both fire
actors disappear, their hotspots are disabled, the extinguisher is consumed,
and script entry 21 finishes with input unlocked. A fresh emulator process
restores the completed puzzle and can walk Ryan to the chest.

This is one puzzle milestone, not a claim that the opening or entire game is
fully playable. The next verified stop is the chest's object response:
**bank 1140, entry 23, byte 8316, opcode 111**.

## Implementation and original-code evidence

Original offsets refer to the user's local ICE640 code/data, which stays ignored.

- **102**, handler 0xd20: argument 1 removes generated navigation connections
  (0x6ef4); argument 0 recreates them (0x6d73). Explicit room links remain.
  Native routing retains explicit edges and gates generated visibility links.
  Visibility is calculated from current triangle state on demand, so there is
  no stale cached graph to rebuild or restore.
- **85**, handler 0x7e6: kind 0 updates an edge (0x5183), kind 1 a triangle
  (0x5211). Only valid registered IDs and Boolean states are accepted. The
  engine synchronizes the runner's edited navigation before subsequent commands.
- Original 0x5957 refuses to generate a replacement for an existing explicit
  link, even a disabled one. Native routing follows the same precedence.
  Enabled explicit links may connect an object approach node outside the floor
  triangles. Movement preserves such a starting node instead of snapping it
  back to the nearest triangle before finding a route.
- **105**, 0x25e6 -> 0x15180: stops playback without releasing its sound slot,
  allowing the later replay/loop/release commands in the action.
- **90**, 0xaff -> 0x84e5: removes a hotspot and its usable actions. The native
  inactive hotspot prevents its registered actions from being clicked again.
- **82**, 0x3540 -> 0x8783: removes an owned item and clears its selection.
- Snapshot payload **version 4** adds the generated-navigation flag. Versions
  1 through 3 remain readable with generated navigation enabled by default.
  Edge/triangle states already participate in the saved navigation records.

The native route finder still uses triangle-union visibility rather than a
byte-for-byte recreation of all original generated nodes/intersections and
path ordering. This milestone verifies the opening's explicit link edits, not
all room geometries or later-chapter pathfinding.

## Verification

Guest startup tests pass, including:

- A node outside the floor is inaccessible through its disabled link, becomes
  reachable when the link is enabled, and becomes inaccessible when disabled.
- Disabling a triangle removes its interior from walkable space.
- Explicit-link routes work while generated links are suspended.
- Script 102/85 state transitions and every truncated argument prefix of 85
  reject incomplete edits without mutating the link or advancing the PC.
- Existing room-hole/disconnected-region, state, script, sprite and audio tests.

The actual PS4 guest restored the prior inventory checkpoint and clicked the
fire using item 15. It passed the former stops at 102 and 105, removed actors
9/10, disabled hotspots 10/11, consumed item 15 and finished entry 21.
The completed-state checkpoint had payload size **17765**, checksum `dd677fc8`
(timing-dependent, not a golden save hash).

A separate fresh process logged:

```text
restored navigation generated=1 edge51=1 edge52=1
walk actor=0 from=265,303 node=24 to=204,318 waypoints=1
walk complete actor=0 node=24 at=204,318
room-task=23 result=2 pc=8316 opcode=111
```

The room was visually checked with no fire. Local evidence:
`artifacts/pice/fire-save-verified.log`, `fire-restore-verified.log`,
`fire-removed.png`. These contain/use local game data and are not distributed.
No physical-PS4 or full-game validation is claimed.

The opt-in `PICE_TESTBENCH` key `pice_test_save_after_fire` writes the manual
checkpoint after the verified fire state and action completion. Combined with
the existing restore/click hooks, this avoids replaying the intro for each test.
All such hooks are removed from the normal local profile after testing.

## Next

Implement opcode 111's context-dependent object responses using the original
language lookup and fallback rules. Then exercise the remaining opening
objects/dialogue, room exits, and the global death/restart path. Do not treat
unsupported object responses as successful completed interactions.
