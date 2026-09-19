# Native PICE movie checkpoint

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
room-task=20 result=2 pc=6612 opcode=147
```

The image hash agrees with the independent host reconstruction. This validates
native decode and progression; it is not a physical-PS4 test or a human check
of audiovisual synchronization. `artifacts/pice/emulator.log` and `build.log`
contain the local evidence. The desktop was locked, so no live window capture
or manual interaction test was performed.

Next command: bank 1140, entry 20, byte 6612, **147(B[60],0,5,30)**.
Original handler 0x1e0d starts the actor's sequence, waits for that actor in the
calling task, and starts entry 30 in dispatcher state 13. State 13 at 0x50b
waits until the actor becomes idle or its animation step equals the requested
step (5). This is a frame-triggered concurrent script, not just a sequence
change. Implement it without losing a trigger when native catch-up crosses
multiple animation steps. The following opcode 83 removes the actor through
0x1b849; preserve stable actor IDs and remove its related bindings/hotspots.

Later in entry 20, opcode 152 requests the original autosave (`SAV00`), followed
by sound/music and hotspot setup. Do not skip autosave or start reporting a
playable game just because the introductory movie completed. Input dispatch,
context expressions, inventory interaction, save/load and gameplay scripts all
still require implementation and validation.
