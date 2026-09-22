# English SVGA demo support

The supplied `pice-dem.zip` contains the DOS/SVGA interactive demo under
`ICE.CD`. Point the engine at that extracted directory, or put its contents
under `scummvm/games/pice` on USB (including its `VIDEO` subdirectory).
No retail files are required. The ZIP and extracted commercial files stay
under gitignored `build/pice-demo`; none are embedded in the engine/package.

The detector matches KSVGA (10916737 bytes) and E_KLANG (12707023 bytes), marks
the game as a demo, and selects English. Its archives contain 320 graphics,
261 language and 83 sound resources. Movies use the `VIDEO/S*.MUX` layout.
Unlike the Windows edition, the demo includes six rendered PCM music tracks.

## Compact resource layout

The demo's 320 graphics entries are byte-identical to four ranges of the
English DOS retail archive. A virtual index maps logical IDs to the demo's
own offsets without inserting/requiring any full-game content:

| Physical demo range | Logical engine range |
| --- | --- |
| 0–178 | 0–178 (A02) |
| 179–245 | 815–881 (B02) |
| 246–287 | 1138–1179 (SM02) |
| 288–319 | 1306–1337 (objects/UI/fonts) |

Sound ranges 0–5, 6–73, 74–76 and 77–82 map to logical 0–5, 45–112,
305–307 and 308–313. Missing resources remain unreadable, rather than
borrowing resources from a retail installation. Parser tests verify the
range boundaries, holes and rejection of an incompatible index size.

Only chapter 2 is present. A request to leave it for another valid chapter
opens a `Demo complete` recovery/main-menu panel, instead of attempting to
load absent chapter resources. This is the preview engine's explicit demo
boundary; matching the original executable's promotional ending is not yet
verified. Demo saves use `-demo-pice-...`, separate from retail saves.

## Validation (2026-09-22)

* Actual PS4 eboot in visible shadPS4 detects the demo and passes parser tests.
* New game reaches opening captions, the complete 181-frame submarine movie,
  radio dialogue and playable bridge (bank 1146), with PCM music.
* All 664 archive entries decompress and validate. All 12 supplied MUX movies
  decode completely in the host harness using the engine movie decoder.
* Bridge save/load and snapshot roundtrip pass, including resumed PCM music.
* A separate restored-save fixture requests chapter 3 to exercise the demo
  boundary; this is a direct transition test, not a completed playthrough.
* Full demo playthrough and physical PS4 behavior are not yet verified.

Local evidence: `artifacts/pice/demo-visible.log`, `demo-resources.json`,
`demo-movies.log`, `demo-restore.log` and `demo-boundary.log`. These generated artifacts are ignored.
