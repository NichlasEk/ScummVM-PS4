# PICE native input and inventory checkpoint (2026-09-20)

The opening room now accepts clicks through its original hotspot/action records.
This is partial gameplay, not a complete playable opening or game. The broad
ScummVM 1.09 release remains separate from the PICE experimental package.

## Controls

- Left click an object to act, or empty floor to walk.
- Right click selects the examine action. Many examine responses still reach
  unsupported opcode 111; do not treat this as complete examine support.
- Collected items appear along the bottom. Click an icon to select/deselect it,
  then left click a target. Right click in the room cancels a selected item.
- F5 saves a manual checkpoint; F7 loads it, falling back to the autosave.
- Saving during a pending/running player action is rejected. This prevents
  silently losing an approach walk, pending script or action context on restore.
- The old picture/animation browser shortcuts are disabled while in room mode.

The temporary inventory strip displays up to 12 owned items. It is not the
original inventory panel, paging or item-combination UI. Hotspots use original
rectangle bounds or the visible actor frame's opaque pixels, including opaque
palette index zero. The native view offsets original actor coordinates by -40 Y.

## Verified original behavior

- Input paths 0x1a128/0x1a188 select verbs 1/6; exits use verb 3, inventory use 9.
- Action lookup 0x7f21 prefers an exact verb match, otherwise retains the last
  matching fallback. Action execution 0x7fa5/0x7c59 walks to the action's approach
  node and waits for the player actor to become idle before starting the entry.
- Fallback context is minus the chosen verb, or the selected item for verb 9.
  Expression 46 at 0x2a28 reads a **raw word index**, not a nested expression.
  Context index 9 is implemented; other runtime contexts still fail explicitly.
- Pickup opcode 67 at 0x31ea/0x87ac marks an item owned once and removes its
  world actor (0x873a). Native removal also disables actor-bound hotspots and
  detaches bindings. Repeated pickup is idempotent; invalid item IDs fail.
- Snapshot payload version 3 adds inventory ownership and selection. Versions
  1/2 remain readable, with empty inventory ownership. Active player actions
  are not saved yet; they are explicitly rejected instead of partially restored.

## Emulator evidence

The test loaded the existing opening autosave in the PS4 guest, then routed a
synthetic mouse coordinate through the same hit-test/click path as real input:

1. Clicked hotspot 9 at (353,164). Ryan walked from (230,306) to node 18 at
   (346,290), then entry 19 started with context -6.
2. Original pickup animation/sound and opcode 67 completed. Item **15**, the
   extinguisher, became owned, its world actor disappeared, and entry 19 ended.
3. Saved a version-3 checkpoint with the item selected: 17769-byte payload,
   FNV checksum `858f3d09` (timing-dependent; not a universal expected checksum).
4. Used it on fire hotspot 10. Entry 21 started with context 15 after approach
   node 25. It stopped at **bank 1140, entry 21, byte 7888, opcode 102**.
5. A separate fresh emulator process loaded that manual checkpoint and logged
   `restored inventory owned=1 selected=15`. Repeating the fire click reached
   the same entry/context and unsupported command.

Local evidence: `artifacts/pice/pickup-and-use-verified.log` and
`artifacts/pice/inventory-restore-verified.log`. Startup tests cover nested
context comparisons, absent/unknown/truncated contexts without PC advance,
transparent/opaque-zero hit testing, and pickup/removal/idempotency/invalid IDs.
The final inventory icon was also visually checked in shadPS4 after correcting
its original frame offsets (`artifacts/pice/input-inventory.png`). Icon bounds
are normalized into the temporary strip; startup tests check those bounds.
No physical-PS4 validation of these new interactions is claimed.

PICE_TESTBENCH-only keys `pice_test_click_hotspot` and `pice_test_use_fire` drive
these automated clicks. Restore keys are described in `PICE-SAVE-RUNTIME.md`.
They are opt-in; the normal local profile has all test/restore hooks removed.

## Next continuation point

Opcode 102 (0xd20) clears/rebuilds the original generated navigation graph via
0x6ef4/0x6d73 around edits. Opcode 85 (0x7e6) changes graph records: kind 0 calls
0x5183 (edge records at 0x14972, stride 11), kind 1 calls 0x5211 (triangle records
at 0x11484, stride 14). Entry 21 next enables edges 51/52. The current native
visibility route finder uses enabled triangles but does not model all original
edge-state/generated-node semantics. Resolve that behavior before accepting
these commands; do not just skip opcode 102 to call the fire puzzle solved.

Also pending: context-sensitive speech (111), item-specific action registration,
inventory consumption/combination, dialogue choices, remaining context values,
and the global death/restart flow (131/132). Without intervention the timer
still reaches the earlier death-transition stop. Later rooms/chapters and a
full playthrough remain unverified.
