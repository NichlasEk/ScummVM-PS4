# Saturn Japanese edition: resource investigation

Status: **native movie and background preview in the PS4 engine**; gameplay
is not implemented. A separate unstable Saturn detector recognizes KVGA.KRO
and J_KLANG.KRO. It enters a clearly labelled preview instead of executing PC
room scripts. Existing DOS/Mac/Windows/demo paths and save namespaces remain
separate. Never rename KVGA to KSVGA to bypass edition detection.

The supplied disc is `Prisoner of Ice - Jashin Kourin (Japan)`, product
T-26112G, V1.000. Its CUE describes three tracks: MODE1/2352 data, MODE2/2352
XA data, and one AUDIO track. Do not infer that the AUDIO track contains all
background music. The original ZIP, BIN and CUE files are preserved under
ignored `build/pice-saturn`.

## Verified resources

* KVGA.KRO: 1335 resources, 15468702 bytes. Low-resolution graphics; an
  inspected background is 320x158. Resource numbering differs from the
  SVGA engine: SM02 begins at 1140 and objects at 1308. The current engine's
  fixed graphics, scripts, UI and coordinate assumptions need adaptation.
* J_KLANG.KRO: 724 resources, 32921880 bytes. Japanese dialogue/inventory
  strings use Shift-JIS, rather than DOS-850 or MacRoman. Native Japanese
  dialogue font sourcing/rendering remains to be established.
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

Full gameplay support still requires a resource/layout adapter, low-resolution
room coordinates and composition, Shift-JIS font support, verified room
voice/music routing, and the separate Saturn UI. Sega FILM playback is now
implemented by the first runtime milestone below. Validate those against the
original runtime before advertising Saturn compatibility. Do not include
commercial font/menu resources in the engine binary as a shortcut.

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

The normal preview plays INFOGRAM, XING, XINGCOPY and VINTRO, then opens the
background viewer. This is a development preview sequence, not a claim to
reproduce the complete original Saturn boot/menu flow. Cross/left click and
Circle/right click browse the background layers; D-pad left/right also browse;
Square replays the intro and Triangle exits. On the local default shadPS4
keyboard map those face buttons are N, B, V and C respectively.

Test-only config keys:

* `pice_test_saturn_assets=true`: decode all MOVIE/*.CPK before entering preview.
* `pice_test_saturn_movie=INFOGRAM.CPK`: select a single startup movie.
* `pice_test_saturn_pause_ms=1500`: pause at frame 10 to verify media timing.

Remaining work: actual Saturn script/resource mapping, actor and scenery
composition, Japanese text/font decoding, native OUT.BIN menu colors/layout,
room speech/music routing (including XA), save/load, and a real game playthrough.
This milestone does not establish physical-PS4 or PS1 compatibility.
