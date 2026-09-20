# Native PICE movie checkpoint

**Continuation update:** autosave and native checkpoint restoration are now
implemented. See [PICE-SAVE-RUNTIME.md](PICE-SAVE-RUNTIME.md) for the latest
verified state and exact continuation. The earlier stop below is historical.

The experimental engine implements the DOS MUX variant used by `VIN12.MUX`.
This is part of the opening sequence, not a complete playable-game release.
The original movie stays in the user's external game directory, `E_VIDEO/`.
No converted movie or game image is included in the source tree or package.

## Decoder and scheduling

`engines/pice/movie.cpp` reads the 0xaa07 container, then its six-byte chunk
headers (16-bit type, 32-bit payload length). The supported variant has
320x200 indexed images and unsigned mono 22050 Hz PCM. Chunk types 6/7 carry
sound, 112 carries the palette, 111 carries compressed image data, 5 presents
the current image, and 4 terminates playback. Unknown variants fail explicitly.

Image flags 4/6 use two stages: a bounded short-distance back-reference stream
expands separate control and literal streams, then bit-coded pixel runs update
the previous image. Flag 6 also permits unchanged-pixel spans. Decode failure
leaves the previous image intact. Other image flags remain unsupported.

The runtime validates every image before starting audio. Script opcode 138
waits synchronously for playback, while the engine keeps pumping quit events.
The DOS PIT divisor sets the clock: 12 interrupts per displayed frame,
1193182 Hz input. Images are doubled to the preview's 640x400 surface. This
scaling is a preview presentation choice, not a claim of exact DOS layout.

Same-chapter movie-only rooms may initialize without a background or palette;
the preview supplies a blank canvas until their script starts the movie. The
room runner then resumes normally. No unsupported command is silently skipped.

## Evidence

Original ICE640 object-2 offsets, reconstructed locally from the user's files:

- 138 -> 0x1c1f: movie filename, clear-before expression, clear-after expression.
- 0x162ef: opens the movie and invokes its playback callbacks.
- 0x161d6: image/palette/presentation chunk callback; image -> 0x230e5.
- 0x235fb: container header validation, including magic 0xaa07.
- 0x23976: indexed seek masks offsets with 0x3fffffff. The current implementation
  reads chunks sequentially and does not expose seeking.
- 0x22ff5/0x2300c: first compression stage, sentinel control bits and paired
  length nibbles. Control words and literal runs share the compressed stream.
- 0x230e5: second stage, full 32-bit control words, paired run-length nibbles,
  and unchanged-pixel spans for delta images.
- 0x2e7c8: PIT clock divides by six; 0x2e751 processes every second clock tick.

A separate local Python reconstruction decoded all 72 image chunks in VIN12.
The film contains 73 presentation markers, including a repeated image. Its
last 64000-pixel image has FNV-1a **58f69777**. Frame 30 was visually inspected
and shows the crate and corridor. The prototype, movie bytes and rendered
frames remain ignored under `/tmp` and `artifacts/pice`.

Synthetic guest tests cover literal decoding, delta preservation, run lengths,
output overflow, invalid headers, truncation of every prefix, transactional
failure and complete-container iteration. Script tests check opcode-138 argument
parsing, every truncated argument prefix, callback rejection and exact PC
advancement. Additional lip-sync, navigation and scheduler tests run at startup.

## Current limitations

Only the verified movie variant and codec flags are accepted. General movie
seeking, arbitrary movie resolutions, skipping with game input and precise
original screen placement remain pending. Movie decoder success does not
establish that later scripts, inventory, save/load or an entire playthrough work.

## Emulator checkpoint and exact continuation

The shadPS4 run reached the movie through the unmodified original scripts:
room 0 -> room 24 -> room 0 -> room 36 -> room 0. Guest diagnostics reported:

```text
movie validated name=Vin12.mux frames=73 audio=139650 final-fnv=58f69777
movie completed name=Vin12.mux frames=73 failed=0
animation-trigger actor=1 sequence=0 step=5 task=30
remove-actor id=1
room-task=20 result=2 pc=6718 opcode=152
```

The image hash agrees with the independent host reconstruction. This validates
native decode and progression; it is not a physical-PS4 test or a human check
of audiovisual synchronization. `artifacts/pice/emulator.log` and `build.log`
contain the local evidence. The desktop was locked, so no live window capture
or manual interaction test was performed.

Opcode **147(B[60],0,5,30)** and the following actor removal (83) now execute.
The original handler 0x1e0d starts a non-looping sequence, waits for the actor in
its calling task, and starts entry 30 in dispatcher state 13. State 13 at 0x50b
waits until the actor becomes idle or reaches the requested animation step.
The native once-only sequence check also accepts a later step so catch-up does
not lose the trigger. Negative sequence selectors remain unsupported.

Opcode 83 removes actor 1 using stable slots, disables its hotspots, removes
its animation-sound bindings and detaches world-item references. Other IDs do
not shift. New actors can reuse vacant slots. Synthetic tests cover the
20-actor capacity, middle-slot removal, association cleanup and slot reuse;
scheduler tests cover delayed trigger release, crossing the target step,
independent caller waiting and rejection of a task that would replace itself.

**Historical stop, now implemented: bank 1140, entry 20, byte 6718, opcode 152.** This requests
the original autosave (`G_IceTxt`,2,`SAV00`). Do not silently skip it or create an
empty save file. Implement native state serialization and restoration first:
A/B variables, room/script PCs and waits, actor slots and animation/path state,
inventory and menus, hotspots/actions, navigation, persistent sound ownership,
and relevant dialogue/input state. Store clocks as remaining durations rather
than host timestamps. Restore and continue the same script instruction after
the save request; do not rerun room initialization over restored state.

Original opcode-152 handler 0x20cf stores the localized label request and save
key, then raises global flag 0x27fb for deferred processing. Identify its consumer
before implementing the request's scheduling semantics. Later instructions
perform further sound/music and hotspot setup. Input dispatch, context
expressions, inventory interaction, save/load and gameplay scripts all still
require implementation and validation. No complete playthrough is claimed.

The native room snapshot after the movie and actor removal was visually
inspected: Ryan stands at the left door, Jones remains on the floor, and the
room's fire/foreground are rendered. This is an engine buffer snapshot, not a
live capture or physical-PS4 verification.
