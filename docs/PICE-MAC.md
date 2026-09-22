# Native Macintosh PICE support

The engine reads extracted original Mac resources directly. No game data is
embedded in the engine or package. Keep `MAC_KRO` and `MAC_MUX` together under
the game directory; USB discovery also accepts this layout below
`scummvm/games/pice`. ISO, SIT and HFS extraction remains an external step.
Local originals and extracted files under `build/pice-mac` are gitignored.

## Edition and language selection

Detection distinguishes the tested multilingual Mac graphics archive
(45719760 bytes) and English/Swedish archive (45719789 bytes) from DOS.
The multilingual copy provides English, Portuguese, German, French, Italian,
Dutch and Spanish. The other copy provides English and Swedish (`W`, not `S`).
Swedish is the default when present; otherwise English is used. Options lists
only installed language banks. A language change takes effect after restarting
the engine. Text, dialogue/voice indices and language-specific final movies
follow the selected bank. Saves are namespaced by Mac/language; existing DOS
save filenames are unchanged.

The Mac path handles big-endian KRO directories, script words, sprite tables,
font offsets, dialogue indices and lip-sync tables. KRO flags remain little
endian. Text uses MacRoman. Mac voice wrappers precede standard RIFF samples.
MUX uses big-endian headers and lengths but little-endian chunk tags; image
flags occupy two bytes. Header offset 26 is frame rate on Mac, PCM bit depth
on DOS. This distinction fixes the `Vin3` stop (room task 1, opcode 138 at 16).

## Validation and remaining limits (2026-09-22)

* Actual PS4 build runs visibly in shadPS4 with each extracted edition in a
  separate profile. Multilingual startup, radio and bridge were reached;
  Swedish edition detection, parser self-tests and startup film decode pass.
* Synthetic movie tests cover DOS and Mac containers, endian differences,
  Mac rates 5 through 12, retained pixels and invalid chunk lengths.
* Host execution of the actual movie decoder checked every movie in both
  Mac sets: all 74 finish after the narrowly matched SWFINAL3 workaround.
  All 45 DOS movies also finish. Both copies of SIN3 decode successfully.
* Swedish `SWFINAL3.MUX` originally failed at frame 167. Its raw delta header contains
  control length `0xffff07aa` but only 1962 (`0x07aa`) payload bytes and zero
  literals. Frame 169 uses `0x000007aa` with the same payload size. This is
  now handled by a narrowly matched decoder workaround: Mac 640x480, mode 2,
  zero offset/literals, exact length fields and packet size, and full-packet
  FNV-1a `9cf59ba1`. Only that packet gets control length 1962. Original files
  remain untouched; arbitrary high bits are never masked.
  Frame 169 differs from frame 167 only at header bytes 6 and 7. Tests
  construct the skip packet, verify the prior image remains unchanged, and
  reject a one-byte mutation, DOS interpretation and an incompatible canvas.
  The source movie SHA-256 is
  `139bb48a83a43578cb22422e2e0806d6cd254d9c8ef52037edb4ff297a8aafa4`.
* Full Mac playthrough, language switching during later chapters and physical
  PS4 behavior are not yet verified. The earlier DOS playthrough does not
  establish Mac completeness.

Local test evidence: `artifacts/pice/mac-all-movies.log`,
`mac-iso-movie-fixed.log`, `mac-swedish-visible.log` and
`dos-movie-regression.log`. These generated logs and commercial data stay local.

Follow-up evidence: `artifacts/pice/mac-final3-regression.log` checks all 119
movies; `mac-final3-playback.log` records a separate PS4 emulator playback
fixture for VFINAL3 (resolved to SWFINAL3), without using player saves.

Count correction: there are 47 ISO and 27 English/Swedish MUX files (74
total). The earlier count of 76 included two non-MUX directory entries.
