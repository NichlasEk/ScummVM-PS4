# Saturn Japanese edition: resource investigation

Status: inspected, **not a playable engine target**. No Saturn detector entry
has been added: renaming KVGA to KSVGA would not provide correct support.
Windows and demo support are independent, already committed milestones.

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
  8-bit PCM at 22254 Hz. This is header validation, not full movie decoding.
  ScummVM has a Cinepak codec; PICE currently has no Sega FILM container path.

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

Native support requires a resource/layout adapter, low-resolution coordinate
and rendering behavior, Shift-JIS font support, Sega FILM playback, verified
voice/music routing, and the separate Saturn UI. Validate those against the
original runtime before advertising Saturn compatibility. Do not include
commercial font/menu resources in the engine binary as a shortcut.
