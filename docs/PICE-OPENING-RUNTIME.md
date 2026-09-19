# PICE opening-room runtime checkpoint (2026-09-19)

The native PS4 engine now runs the original first-room initialization and a
substantial part of its opening sequence. **It is not yet a playable game.**
This document supersedes the background-only continuation point at the end of
PICE-ENGINE.md. Continue with the current checkout; do not rebuild this work
from the older preview.

## Verified behavior in shadPS4

- Bootstrap bank 1139 creates 18 items, a dialogue menu, persistent ambient
  sound handles, the inventory-panel resource and the initial room variables.
  It stops explicitly at scene-change opcode 34, byte 1144. The testbench then
  opens bank 1140 explicitly; bootstrap scene selection is not implemented.
- Bank 1140 entry 0 completes normally at byte 3810. It loads the background,
  palette, compound foreground, actors, looping fire animations, 27 navigation
  nodes, 56 edges, 33 triangles, 12 labelled hotspots, action registrations,
  sound handles and animation-sound bindings. The extinguisher is a world item.
- Entry 1 starts Ryan's original entrance animation at (602,322), waits for it
  to finish at (561,323), closes the door and restores his ordinary resource.
- Opcode 129 resolves text key 1000 in language resource 636 to voice 0. The
  captain's first line comes from the original script, with its temporary
  animation resource 76. The actor's previous resource is restored afterward.
- Entries 1 and 11 run concurrently. The captain walks to node 7, (432,318),
  and Ryan to node 8, (466,336), using their directional walking sequences.
  Sequence positions are interpolated along the segment; this is not a claim
  of pixel-identical original path interpolation.
- Both entries pass their mutual opcode-44 rendezvous. Exclusive sections
  (41/42) and same-chapter room changes (34) now run. Room bank selection is
  **SM02 base 1138 + room + 2**, not base + room.
- Room 24, bank 1164, displays its 640x317 closeup background. All three text
  pages play voices 714..716, with original EDITLS tail records selecting the
  timed face-animation sequences. It then returns to room 0, entry 2.
- Further room dialogue and walking execute. Ryan's route from (466,336) to
  (262,314) takes three waypoints across the floor instead of crossing its gap.
- The sequence requests room 36, bank 1176, which contains no background:
  its task plays VIN12.MUX and then returns to room 0. Movie verification and
  the latest exact stop are recorded below. Unsupported commands still stop
  explicitly rather than being skipped.

The default startup no longer plays the unrelated chant over the room. The
older image/animation/voice browser remains available through its test controls.
Clicking the room no longer advances to an unrelated background image.

## Evidence and testing

The maintained sources are `engines/pice/`; `scripts/pice/build.sh` copies them
into the isolated ScummVM build. Use:

```sh
EMULATOR_TIMEOUT=110 scripts/pice/run-emulator.sh
```

The experimental package is
`dist/pice-experimental/ScummVM-PS4-PICE-preview.pkg` (SCVM00003).
Original game files stay outside the package, mounted separately at /mnt/usb0.
The stable DOTT-capable PS4 package and USB contents are not changed.

Latest runtime log: `artifacts/pice/emulator.log`. Check for parser self-tests
PASS, completed walks, scripted-dialogue start/completion and the explicit
latest explicit unsupported-command stop. Build diagnostics: `artifacts/pice/build.log`.

Synthetic guest tests additionally cover:

- Transparent versus opaque palette-zero pixels, clipped bottom-anchored
  sprites, recursive compound frames and transactional rejection of cycles.
- Legal unused empty animation sequence slots, while rejecting their selection.
  Resource 76 contains such an unused slot; rejecting its entire container was
  the first obstacle to the captain's scripted dialogue.
- Wrapped side-effectful assignments, invalid destinations and truncated
  arguments without resource allocation or PC advancement.
- Actor waits that invoke an animation once, then resume after completion.
- Mutual script barriers with staggered arrival and independent task stops.
- Exclusive scheduling, room-change teardown, multipage lip-track parsing,
  track dependencies, and transactional movie instruction argument decoding.
- Raw-word navigation instructions, triangle geometry, nearest floor points
  and exact segment/triangle interval coverage, including disconnected floors and an L-shaped route around a corner.

A locked desktop prevented compositor screenshots of the game. The optional
`PICE_TESTBENCH`-only configuration key `pice_debug_snapshot_path` writes a
640x400 PPM of the composed room and current palette on an unsupported command.
The local profile uses `/data/scummvm/saves/pice-room.ppm`, mapped to
`.tools/xdg-pice/shadPS4/data/scummvm/saves/pice-room.ppm`. This is an opt-in
local diagnostic, not an asset to publish. The rendered room was inspected
through this output; actors, fire and foreground objects are visible.

## Reverse-engineering pointers

All addresses below are object-2 offsets in the user's local ICE640 executable.
Original binaries/disassembly remain ignored under `artifacts/pice/`.

- 47/48/49: 0x2a52/0x2ac5/0x2b30, navigation registration. Limits are 50 nodes,
  450 edges, 50 triangles. Triangle registration resolves its three edges.
- 55/77: node-positioned/explicit-position actors, 0x2d7f/0x3394 -> 0x1a966.
- 56/92: player selection and route-buffer allocation, 0x1aafe/0x1aac7.
- 57/59/61: actor/rectangle/world-item hotspots, 0x8229/0x817e/0x82f1.
  The approach argument is not always -1 (the bridge exit uses node 3).
- 58/110: action registration and field 0x10 updates, 0x83cb/0x8561.
  Full verb dispatch and the exact use of this field remain to be implemented.
- 63/60: inventory item creation and world-item actor, 0x8623/0x859e. A world
  item's actor uses the inventory sequence plus two and the requested node.
- 66/137: dialogue menu and inventory-panel resources. These are loaded;
  inventory UI, choice enablement and item pickup flags are not implemented.
- 29/133 load SFX (persistent flag differs); 100 plays once, 114 takes loops
  (zero means infinite), 30 takes volume 0..100.
- 109 -> 0xb8ea registers **actor, sound, volume, sequence, step**, in that
  argument order. Do not mistake volume for resource ID or step for loops.
  0xb84f triggers once on matching sequence/step and resets after leaving it.
- 95 mode 3/4 enables/disables the room's green/blue palette pulse. 0x77e8
  changes indices 1..239. Initial delta/offset at data 0x1584 are 15 and 10.
- 51/96/117 play an actor sequence, replace its resource at an optional node,
  or replace it at explicit coordinates, then wait for the actor.
- 97 loops an animation; 0x1b61b resets its origin on each completed loop.
- 129 -> 0x1704: actor, localized text name/index/key, animation name/index,
  inclusive sequence range and inclusive pause range. 0x1ab30/0x1b580 choose
  talking sequences and pauses. Dispatcher state 10 at 0x556 restores the
  previous resource/sequence after the dialogue finishes.
- 44 -> 0x223e; dispatcher states 3/8 synchronize two mutually waiting tasks.
- Original timer 0x1e9e4 returns hundredths of a second. The runtime advances
  waits and sequence timing from elapsed 10 ms units with bounded catch-up.

## Remaining work / next exact continuation

1. The opening now reaches the movie-only bank 1176. Opcode 138 uses a native
   MUX reader and returns to room 0. Entry 20 now stops at byte 6612, opcode
   **147(B[60],0,5,30)**. See `PICE-MOVIE-RUNTIME.md` for evidence and the next
   concurrent animation-trigger implementation.
2. Bootstrap 34 still contains unsupported context expressions **46(5),46(4)**.
   Opcode 46 dereferences object-3 pointer table 0x11ad. Context 4/5 are previous
   room/chapter; 7/8 are current room/chapter. Do not guess the other contexts.
3. Walking now finds visibility-graph paths through the enabled triangle
   union. Dynamic edge restrictions, non-100 scale and arbitrary user clicks
   remain pending. It is not a reproduction of the original path algorithm.
4. Voiced dialogue advances through all pages. Closeup voices use EDITLS
   animation tracks; ordinary dialogue uses the original random sequence/pause
   ranges. Silent dialogue, interruption and simultaneous owners are pending.
   Font rendering and subtitle layout still use the preview font.
5. Hotspot records and actions are registered but not dispatched from clicks.
   Hover labels, verb selection, inventory pickup/use, dialogue choices,
   transitions, save/load and a complete playthrough are still required.
6. Foreground is composited after actors; general depth sorting is pending.
   Frame-triggered sounds are checked on runtime visits; catch-up that crosses
   multiple steps can miss an event. Renderer image caching and performance
   verification on physical PS4 remain pending.
7. Resource groups are currently bounded to this first chapter/room's known
   A02/B02/OBJECT/SMP02 and English BIN mapping. Expand based on original name
   tables rather than assuming these ranges apply to every room.

No physical PS4 verification is claimed for this runtime checkpoint. Do not
present it as a playable release or distribute the local screenshots/game data.
