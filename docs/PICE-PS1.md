# PlayStation Japanese edition

Status: **experimental gameplay**, verified in the actual PS4 eboot under
shadPS4. This is not a full playthrough or a physical-PS4 compatibility claim.

The supplied single-track MODE2/2352 disc identifies as SLPS-01129. Detection
uses its `SLPS_011.29` executable (518144 bytes, first-5000-byte MD5
`ef534fb037ffd910c911107589454d6b`). Point ScummVM at the extracted disc root,
with `SLPS_011.29`, `DATA/` and `MOVIE/` present. The PS4 test bootstrap also
recognizes `DATA/KVGA.KRO` in a mounted root or its `scummvm/games/pice` directory.

## Shared resources, separate platform support

The supplied PS1 KVGA.KRO, J_KLANG.KRO and KSOUND.KRO match the supplied Saturn
archives byte for byte. The executable contains the same 29-group chained-XOR
resource directory, found at 0x62d64 in this revision. The engine scans and
validates this directory at runtime; it does not embed the extracted table or
borrow Saturn's A.BIN.

The two console editions therefore share low-resolution scene composition,
script mapping, actor navigation, inventory, native cursor frames and Shift-JIS
caption decoding. The former Saturn-only geometry flag is now `_console`, with
`_ps1` selecting PS1-specific media, executable and saves. PS1 saves use the
`-ps1-J` namespace and cannot overwrite Saturn or PC saves.

Room audio currently comes from the shared KSOUND.KRO path. Matching that to
the original PS1 runtime's MUS, SND and VOICE/XA routing is unfinished. The main
menu uses the shared PANEL resource and engine controls. The MENU shortcut now reads its original bitmap and palette from PS1's
DATA/FONTS.TIM. The full original console menu layout is still unfinished;
no Saturn OUT.BIN artwork is borrowed.

Japanese captions currently need the external PICE-JA.FNT described in
[PICE-SATURN.md](PICE-SATURN.md). The locally generated replacement font is
separate from the engine/package; the Japanese glyph atlases J1.TIM through J4.TIM remain unimplemented.

## Disc extraction

Preserve the original BIN/CUE. Ordinary ISO extraction drops the raw sector
headers needed by ScummVM's STR/XA decoder. The provided helper accepts the
single-track MODE2/2352 BIN and an empty output directory:

```sh
python3 scripts/pice/extract-ps1.py /path/to/original.bin \
  --output build/pice-ps1/disc-files
```

It reads the ISO9660 directory, validates bounded file extents and paths,
extracts ordinary files as 2048-byte sector payloads, and preserves complete
2352-byte sectors for STR and XA files. Consequently the extracted media files
are larger than their logical ISO directory sizes. `extraction.json` records
both sizes and original LBAs. It does not modify the source disc. Other disc
layouts, compressed images and multi-extent files are not supported.

All 180 files extracted with this helper matched an independent local
extraction by SHA-256. The original archives, raw disc, extracted media,
replacement font and generated inspection artifacts remain ignored by Git.
No game artwork, audio, movies or fonts are included in the PS4 package.

## Native STR movies

`ps1.cpp` uses ScummVM's existing PSXStreamDecoder for MDEC video and XA audio,
with a 2x CD sector clock. It does not transcode movies or use FFmpeg at runtime.
A preflight pass checks sector sync, Mode 2 subheaders, track layout, chunk
ordering, frame bounds, dimensions and supported MDEC/audio variants before
entering the decoder. Synthetic checks reject truncation, broken chunk order,
unsupported video versions and alternate tracks.

Movies render as RGB565 with integer scaling. Room audio pauses during movie
playback; the room palette and clocks are restored afterward. ScummVM pauses
are forwarded to the decoder. Frontend movies can be skipped, while scripted
movies follow the existing unskippable scene behavior. Exact original PS1
startup/menu ordering and pause/audio synchronization still need comparison
against the original console runtime.

## Verified milestone

Actual PS4 build, separate shadPS4 profile and PS1 save directory:

* PS1 detection selects the correct edition; resource/parser/font self-tests pass.
* INFOGRAM: 85/85 frames; XING: 100/100; XINGCOPY: 105/105;
  VINTRO: 718/718, each played to completion without skipping.
* New game executes the native chapter bootstrap, narration, VIN1 (194/194
  frames), radio sequence and bridge at bank 1148.
* A separate fast audit decoded all frames of all 25 MOVIE/*.STR files.
  This is a decoder audit, not real-time viewing or audio verification of all movies.
* The bridge checkpoint restores with six actors and 18 items; its 18153-byte
  serialization roundtrip passes without a state mismatch.
* The rebuilt eboot also restores copied DOS (bank 1147, 19169 bytes) and
  Saturn (bank 1143, 17987 bytes) saves with successful serialization roundtrips.
* A normal click makes Ryan walk to a character and examine them, then walk to
  another navigation node.

Local evidence: `artifacts/pice/ps1-first-boot.log` and
`ps1-audit-restore.log`. The test-only `pice_test_ps1_assets=true` enables the
full movie decode audit. Disable test keys for interactive play.

Remaining work includes the rest of the native PS1 menu/TIM interface, original Japanese
font sourcing, sound-bank/XA routing comparison, later chapter transitions,
scene composition comparison and a complete gameplay run.

## Local weekend test

With the local disc files and build tools prepared:

```sh
scripts/pice/run-emulator.sh build/pice-ps1/disc-files
```

This selects the separate `xdg-pice-ps1` profile by default. The prepared local
profile restores its PS1 bridge checkpoint; remove `pice_restore_slot0=true`
from that profile's ScummVM configuration to see the full startup again.
Newly created per-game emulator bindings include WASD movement, N/B clicks,
and mouse-to-left-stick input (F7 captures/releases, left/right mouse buttons
click). Existing bindings are preserved. No automatic gameplay tests remain
in the prepared profile.

## Original MENU bitmap

DATA/FONTS.TIM is a 256x256 4bpp atlas with a 256-entry RGB555 CLUT. The MENU
button occupies `(120,72)` through `(159,87)`, a 40x16 rectangle. Palette bank
6 supplies the blue frame, background and lettering matching the supplied PS1
reference. The decoder reads low nibbles first and validates TIM flags, CLUT
size, image bounds and exact file length before extracting the rectangle.

The bitmap uses the same upper-right placement, hand hover and click handler
as the Saturn shortcut. Colors map to the nearest current scene palette without
recoloring room pixels. Only geometry and decoding logic are in source; no
bitmap or palette bytes are embedded. Missing/malformed assets retain keyboard
access to the menu.

Local inspection: `artifacts/pice/ps1-fonts-atlas.png` and
`ps1-menu-original.png`. Actual eboot button and return-to-game verification:
`ps1-menu-test.log`. The existing test-only `pice_test_saturn_menu_button` key
also exercises this shared console-button path on PS1.
