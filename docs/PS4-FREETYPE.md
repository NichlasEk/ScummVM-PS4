# PS4 FreeType port

Package 1.14 enables FreeType2 and four more main engines: BAGEL, The
Journeyman Project 2: Buried in Time, Red Comrades and Z-Vision. This is
compiled support, not a claim of working gameplay for those games.

## Build

`scripts/build-ps4-freetype.sh` downloads FreeType 2.14.3 from the
[official release mirror](https://download.savannah.gnu.org/releases/freetype/).
The archive SHA-256 is pinned:

`36bc4f1cc413335368ee656c42afca65c5a3987e8768cc28cf11ba775e785a5f`

The static PIC build uses Clang with the OpenOrbis sysroot and the project's
PS4 ABI header. CMake's Generic platform selects FreeType's portable stdio
backend; the Unix file-mapping backend failed the target font-open test.
Optional PNG, external zlib, bzip2, HarfBuzz and Brotli dependencies are disabled.
No system SDK or host libraries are changed. Target headers, library and
pkg-config metadata live in `build/ps4-freetype/install`.

The shared configure script restricts pkg-config to target directories and
clears the ambient PKG_CONFIG_PATH. Main, SOTC and PICE build scripts include
the dependency fingerprint and relink on a changed static archive. Packaging
includes the FreeType license texts (GPLv2, FTL and license-selection notice).
FreeType is used under its GPL option with this GPL ScummVM distribution.

## Target rendering test

Run `scripts/test-ps4-freetype-build.sh FONT.ttf`. It builds the same library
version and test for Linux and PS4, then renders A, g, 0, Å, ä and ö at
9, 16 and 32 pixels. Output records bitmap dimensions, bearings, advance and
an FNV-1a hash of every glyph's pixels.

Boot `build/ps4-freetype-smoke/stage/eboot.bin` with an isolated shadPS4 profile,
then compare `data/freetype-smoke.txt` with `build/ps4-freetype-smoke/expected.txt`.

The local FantasqueSansMNerdFont-Regular.ttf fixture passed all 18 comparisons
byte for byte in shadPS4, with guest exit status 0. Its hash is recorded in
`build/ps4-freetype-smoke/input.sha256`; it is not packaged with ScummVM.
Evidence: `artifacts/emulator/freetype-smoke.log` and
`artifacts/emulator/freetype-smoke-result.txt`.

## Actual Buried in Time test

All three user-provided Windows CD ISOs were extracted locally. Their BITDATA
folders were merged with byte comparisons for duplicate paths (958 files,
no conflicts). The self-contained game directory is
`artifacts/BuriedInTime/extracted`; it includes BIT2416.EX_ and BIT24LIB.DL_
from disc 1. The ISO label says 1.03; the exact detection hashes match
ScummVM's English compressed v1.00 24BPP entry. The original ISOs are unchanged.
The general package includes no game data.

The actual PS4 executable booted in visible shadPS4, opened the original title
animation and menu, accepted New Game, played the intro and reached the game
interface. FreeType renders the status and inventory labels. The packaged
fonts.dat provides the Liberation Sans fallback. Evidence:
`artifacts/emulator/buried-launch.log`, `buried-mainmenu.png`, `buried-intro.png`
and `buried-gameplay.png`.

Package SHA-256:
`4a337faa4094e1497300ef3054fb351bb4befbba1fbc9db038db225884519ada`.
PS4 import allowlist passed; 95 top-level static engines linked.

Manual save slot 1 and autosave were written in the isolated test profile;
slot 1 was then selected in the load dialog and restored to the game view.
Evidence: `artifacts/emulator/buried-restored.png` and the save-file opens
in `buried-launch.log`. The user also confirmed that the game appears to work.

## Remaining validation

Full-game progression, audio verification and physical PS4 validation remain open.
PNG and Vorbis are the next shared dependency ports; The Longest Journey
still needs Vorbis. Shader-based games also need a graphics backend port.
