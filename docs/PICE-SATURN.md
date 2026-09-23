# Saturn Japanese edition: resource investigation

Status: **experimental native Saturn gameplay in the PS4 engine**. The
opening narration, submarine film, radio sequence and bridge now execute
Saturn resources and scripts. Walking, examining a character, Japanese
captions, the engine menu and save/restore have been exercised in shadPS4.
This is not a completed Saturn playthrough or a physical-PS4 verification.
The native console menu and full sound-routing equivalence remain unfinished.

Detection recognizes KVGA.KRO and J_KLANG.KRO, then reads the resource directory
from the user's A.BIN. Existing editions retain separate paths and save names.
Never rename KVGA to KSVGA to bypass edition detection.

The supplied disc is `Prisoner of Ice - Jashin Kourin (Japan)`, product
T-26112G, V1.000. Its CUE describes three tracks: MODE1/2352 data, MODE2/2352
XA data, and one AUDIO track. Do not infer that the AUDIO track contains all
background music. The original ZIP, BIN and CUE files are preserved under
ignored `build/pice-saturn`.

## Verified resources

* KVGA.KRO: 1335 resources, 15468702 bytes. Low-resolution graphics; an
  inspected background is 320x158. Resource numbering differs from the
  SVGA engine: M02 begins at 1140 and objects at 1308. The runtime now resolves
  chapter resources through the embedded native directory.
* J_KLANG.KRO: 724 resources, 32921880 bytes. Japanese dialogue/inventory
  strings use Shift-JIS, rather than DOS-850 or MacRoman. Native Japanese
  captions now render through an external Unicode bitmap font. The original
  console font source remains to be established.
* KSOUND.KRO: 353 resources, 93841604 bytes. The disc also has platform sound
  banks in BGM/*.OUT, SMP*/*.SE, JINGLE and VOICE00/VOICE01 XA files. Merely
  reading the KRO does not establish the Saturn runtime's sound routing.
* All 2412 KRO entries decompress and pass the existing archive audit.
* MOVIE contains 25 FILM/CPK containers. All sample directories have valid
  bounds. ffprobe identifies VINTRO.CPK as Cinepak 320x176 with mono signed
  8-bit PCM at 22254 Hz. The initial inspection validated headers; the runtime verification below now
  covers full Cinepak decoding through the new Sega FILM container adapter.

## The different menus

OUT.BIN has 65 records: a palette-like block, a larger binary block, a DSP
block and 62 width/height-prefixed 4bpp textures. Extracted glyphs include
Japanese Main Menu, Options, Continue, Return to Game and Mouse Speed, plus
LOAD, SAVE, VOICE, SE, BGM, ON/OFF and Saturn backup/cartridge RAM messages.
This confirms a separate console menu layer, rather than merely a translated
PC menu. Its placement, colors, interaction and platform memory actions are
not yet implemented. The local atlas is grayscale for glyph inspection; it
is not a screenshot of a running Saturn menu.

Reproduce the read-only inspection on intact extracted files:

```sh
python3 scripts/pice/inspect-saturn.py build/pice-saturn/track1-files \
  --output artifacts/pice/saturn-inspection.json \
  --menu-atlas artifacts/pice/saturn-menu-labels.png
```

Only the optional atlas requires Pillow. No images or game bytes are stored
in source; the JSON records metadata and hashes.

## Extraction limits and next implementation work

The first track was stripped from 2352-byte MODE1 sectors to 2048-byte ISO
sectors for inspection. Selected first-track files were extracted; all 159
extracted file sizes match the ISO directory and all KRO streams validate.
The ISO also references XA files beyond the first track. A full extraction
must preserve their mode/subchannel information; the first-track ISO alone
is **not a complete playable disc conversion**. The original raw tracks are
untouched. No claim is made that a plain 2048-byte extraction preserves XA.

The following milestones distinguish tested functionality from the remaining
compatibility work. No commercial font/menu resources are embedded in the engine.

## First running milestone (2026-09-22)

`film.cpp` reads big-endian FILM 1.09/FDSC/STAB directories, validates bounded
sample extents and video timestamps, and distinguishes keyframe flags from
time values. The initial audio variant is signed 8-bit mono PCM; other codecs
and channel layouts fail explicitly. `saturn.cpp` feeds native packets to
ScummVM's existing Cinepak decoder and original movie audio to its mixer.
RGB565 output and integer scaling preserve the low-resolution frame geometry.
Skipping stops the movie audio, the final frame/audio duration is retained,
and opening the ScummVM menu pauses both media timing and audio.

The adapter was checked against the container fields documented by the
[FFmpeg Sega FILM demuxer](https://github.com/FFmpeg/FFmpeg/blob/master/libavformat/segafilm.c)
and ScummVM's local `image/codecs/cinepak.cpp`. No FFmpeg executable or converted
movie is needed at runtime. The movie container parser is independent of the
room-resource layout; a future PS1 adapter can reuse shared presentation code
if its actual resources permit it, without pretending PS1 movies are FILM.

Verification on the **actual PS4 eboot in visible shadPS4**:

* All 25 original CPK files decoded to their complete frame counts. This is a
  decoder audit, not a real-time playthrough of every movie.
* VINTRO played 777/777 frames, 1,261,800 PCM bytes, 56.7 seconds, without skip.
* INFOGRAM played 76/76 frames. A 1.5-second engine pause extended its 5-second
  presentation to 6.504 seconds (1.501 seconds measured paused).
* Synthetic parser tests reject truncated/out-of-bounds samples, timestamp
  errors, unsupported audio and malformed Cinepak geometry before decoding.
* A copied DOS checkpoint still restores at bank 1147, and its existing parser
  self-tests pass in the rebuilt eboot (`saturn-dos-regression.log`).
* Cross, Circle and D-pad browsing were exercised through shadPS4 input; Square
  replay was checked separately (`saturn-controls.log`).
* The preview discovers 125 raw background layers with adjacent palettes.
  Black gaps belong to separately drawn scene layers; actors, foreground
  composition, navigation and gameplay are not being simulated by the viewer.

Local evidence: `artifacts/pice/saturn-runtime-final.log`,
`saturn-visible.log`, `saturn-window.png` and `saturn-intro-screen.png`.
These generated files and all commercial assets stay ignored.

With `pice_saturn_media_preview=true`, the optional viewer plays INFOGRAM, XING, XINGCOPY and VINTRO, then opens the
background viewer. This is a development preview sequence, not a claim to
reproduce the complete original Saturn boot/menu flow. Cross/left click and
Circle/right click browse the background layers; D-pad left/right also browse;
Square replays the intro and Triangle exits. On the local default shadPS4
keyboard map those face buttons are N, B, V and C respectively.

Test-only config keys:

* `pice_test_saturn_assets=true`: decode all MOVIE/*.CPK before entering preview.
* `pice_test_saturn_movie=INFOGRAM.CPK`: select a single startup movie.
* `pice_test_saturn_pause_ms=1500`: pause at frame 10 to verify media timing.

## Native gameplay milestone (2026-09-23)

The engine scans A.BIN for its validated chained-XOR resource directory rather
than embedding an extracted table. Its 29 groups identify native chapter
scripts, actor banks, backgrounds and UI resources. M02's bootstrap is 1141,
its first room 1142, its bridge 1148, and its opening sequence 1181. Native
`empty\r\n` and `dummy` script slots are recognized as placeholders; unsupported
instructions are not silently skipped.

Room composition uses native 320x200 coordinates, then doubles the finished
image to the existing 640x400 presentation. Hit testing and navigation map back
to the same native coordinates. Cursor, inventory slots and icons are scaled
consistently. Scripted movies map to native CPK files, pause room audio during
playback and restore the indexed-color room afterward.

A fresh launch plays INFOGRAM, XING, XINGCOPY and VINTRO before the engine's
main menu. This is the current development startup sequence; exact Saturn boot
ordering is not established. New game runs the native chapter bootstrap and
opening sequence. The functional main menu uses the disc's PANEL artwork with
English engine controls, not the OUT.BIN console interface.

### Japanese captions

Place an external `PICE-JA.FNT` next to KVGA.KRO. Generate it from a Japanese
TrueType font you are entitled to use (Pillow required):

```sh
python3 scripts/pice/make-japanese-font.py /path/to/Japanese.ttf \
  --latin-font /path/to/Latin.ttf \
  --output build/pice-saturn/track1-files/PICE-JA.FNT
```

The optional Latin font supplies ASCII when the Japanese fallback font lacks
it. The output is a replacement bitmap font, **not the original Saturn font**.
Neither source fonts nor generated glyphs are part of the engine or package.
Keep their license terms when distributing a generated font separately.

PJF1 contains a four-byte signature, a little-endian glyph count, then sorted
38-byte records: Unicode code point (LE32), advance (1..16), reserved zero byte,
and a 16x16 one-bit bitmap, two bytes per row, most significant bit first.
The loader validates lengths, ordering, code points and advances before use.
Synthetic tests cover drawing, outline, clipping and malformed data.

Shift-JIS decoding also needs ScummVM's `encoding.dat`. The PS4 package already
ships it under `/app0/data`; custom test configurations need
`extrapath=/app0/data`, as the normal PS4 configuration provides. Missing
PICE-JA.FNT currently produces a log warning and leaves Japanese glyphs
unavailable; it does not make the underlying room scripts unplayable.

### Verification and remaining scope

Actual PS4 eboot, visible shadPS4, isolated profile and save namespace:

* Parser/font self-tests pass; all native script entry tables parse apart from
  the recognized placeholders.
* Opening bank 1181 runs its four narration pages and voices 636..639.
* VIN1 plays 271/271 frames and 400,572 PCM bytes, then the radio sequence
  completes and bank 1148 renders six animated actors.
* Clicking the bridge character executes the normal walk/examine action;
  Ryan then walks to another navigation point.
* A captured rendered frame contains readable Japanese dialogue.
* Native checkpoint restore and serialization roundtrip pass at bank 1148
  (18,177 bytes); saving through the resumable main menu also succeeds.
* Saturn saves use a `-saturn-J` namespace, separate from DOS/Mac/Windows/demo.
* The final startup build plays all four opening CPK files to completion, then
  opens the main menu.
* The same eboot restores the copied DOS bank 1147 checkpoint and passes its
  19,169-byte snapshot roundtrip (`saturn-game-dos-regression.log`).

Evidence lives in ignored `artifacts/pice/saturn-game-third.log`,
`saturn-restore-interaction.log`, `saturn-menu-caption.log`,
`saturn-caption.png`, `saturn-startup-menu.log` and
`saturn-new-game-final.log`. These are development
artifacts, not redistributable game content.

Remaining work: native OUT.BIN menu appearance and interactions, original
Japanese font sourcing, verified platform audio routing including XA,
comparison of scene composition against original Saturn output, and testing
later chapters and a complete playthrough. A separate experimental PS1 adapter now reuses the verified shared archives;
see [PICE-PS1.md](PICE-PS1.md) for its STR movies, saves and test coverage.

## Saturn MENU shortcut

The runtime now reads OUT.BIN record 62 (40x16, packed 4bpp) and its blue
palette bank 6, selected against the supplied console screenshot. It displays
the original bitmap at 2x scale in the upper-right safe margin, clear of the
inventory. Colors are mapped to the nearest current scene-palette colors;
this does not overwrite palette entries used by the room. Exact console color
reproduction remains part of the native menu work.

Left click opens the resumable engine menu before dialogue, inventory or room
click handling. The button is hidden on the game-over screen and is not drawn
into CPK movies. Missing/malformed OUT.BIN leaves the existing keyboard menu
available. The loader checks directory ordering, bounds, palette count and
texture geometry before decoding. No extracted bitmap or palette is embedded.

Actual eboot verification: `saturn-menu-button-test.log` shows the original
texture loaded, the real button hit path opening the resumable menu, and return
to the restored bridge. `saturn-menu-button.png` captures the displayed button.
The automatic click uses test-only `pice_test_saturn_menu_button=true`; remove
it for interactive play.

For the local shadPS4 profile, ordinary desktop mouse control needs explicit
mouse-to-left-stick mapping in `input_config/default.ini` (unified inputs):

```ini
mouse_to_joystick = left
mouse_movement_params = 0.15, 1.0, 0.05
cross = leftbutton
circle = rightbutton
```

F8 reloads input configuration; F7 toggles mouse capture/release. WASD and N/B
remain alternatives. This is emulator input emulation, not direct native mouse
support on the physical PS4. No automatic test keys are left in the interactive
Saturn profile.

### Native pointer frames

Saturn CURSOR has 43 frames. Its normal arrow and pointing hand are frames
32 and 33 (images 25 and 33), rather than PC frames 25 and 26. The latter
select unrelated small horizontal symbols in the Saturn archive. Interactive
room/inventory targets and the MENU shortcut now select the native hand; the
resumable menu also uses it. Cursor state is explicitly refreshed after leaving
the menu, even if the previous and next hover states are both noninteractive.
Selected inventory objects still provide their own cursor images.

The rebuilt PS4 eboot passed its self-tests and restored the bridge; opening the
resumable menu was exercised in `saturn-cursor-live.log`. The ignored inspection
atlas `saturn-cursors.png` identifies the native source images. All pointer
artwork continues to come from the user's KVGA.KRO.
