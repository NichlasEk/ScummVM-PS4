# PS4 PNG and zlib port

The 1.16 build adds static libpng and zlib, enabling CRAB, Glk, Sludge and
Wintermute: 101 main engines. Package 1.17 also includes the subsequent Theora
port. Engine availability is not a full-game or physical PS4 compatibility claim.

## Reproducible build

`scripts/build-ps4-png.sh` cross-compiles the official releases with clang,
the OpenOrbis sysroot and the repo's PS4 ABI header. Static PIC libraries and
headers are installed under `build/ps4-png/install`. The PNG build explicitly
uses target zlib, disables hardware optimizations, and builds no shared library
or host tools. The SDK is unchanged. Both license notices are packaged.

Pinned sources:

- [zlib](https://zlib.net/) 1.3.2, `https://zlib.net/fossils/zlib-1.3.2.tar.gz`:
  `bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16`
- [libpng](https://www.libpng.org/pub/png/libpng.html) 1.6.58, official
  `https://github.com/pnggroup/libpng/archive/refs/tags/v1.6.58.tar.gz`:
  `a9d4df463d36a6e5f9c29bd6f4967312d17e996c1854f3511f833924eb1993cf`

Main, SOTC and PICE builds invoke the dependency builder, fingerprint its build
stamp and relink when either archive changes. Configure checks target libraries
and now fails if PNG or zlib is unavailable rather than silently omitting them.
ScummVM's existing `USE_PNG` and `USE_ZLIB` code paths are used without game or
decoder patches. Enabling zlib also selects upstream's external zlib stream
implementation instead of its bundled fallback.

## Tests without game data

Run `scripts/test-ps4-png-build.sh`. `tests/ps4/make-png-fixtures.py` generates
30 small PNG images, covering every legal PNG color-type/bit-depth combination
both non-interlaced and Adam7-interlaced. Odd dimensions exercise packed rows
and short passes. The fixtures include all five PNG filters, split/empty IDAT
chunks, palettes, grayscale, RGB, gray+alpha, RGBA, tRNS and 16-bit samples.
Expected normalized RGBA hashes are computed directly from the synthetic pixel
values, independently of libpng. No copyrighted game data is used.

The C harness exercises the classic libpng API with custom read/error callbacks,
including longjmp error recovery. Host decoding matches the pixel oracle, and
the PS4 SELF produces byte-identical output. Bad CRC, truncated stream and
invalid signature fixtures are rejected. This is the harness's error policy;
it does not change ScummVM's existing error handler for damaged game assets.

Verified in shadPS4: guest exit 0 and all 33 cases pass.
Evidence: `artifacts/emulator/png-smoke.log`, `png-smoke-result.txt` and
`build/ps4-png-smoke/expected.txt`. Input hashes: `input.sha256` in the same
build directory. Run the staged `stage/eboot.bin` in an isolated emulator
profile and diff its `data/png-smoke.txt` against `expected.txt`.

The full 1.16 executable also reached the orange launcher in an isolated profile.
Its test-only theme ZIP contained an added RGBA Adam7 PNG bitmap declaration,
exercising ScummVM's theme loader, `Image::PNGDecoder` and ZIP decompression.
No fallback-theme or PNG decoding error appeared. The shipped theme is unchanged.
Evidence: `artifacts/emulator/png-launcher.log` and `png-launcher.png`.
Physical PS4 tests and gameplay in the four newly enabled families remain open.

Intermediate 1.16 PKG SHA-256:
`d0aec90d1aa761427f62091692d0c5a670e508934e0ebbf87e6b826d45251824`.
Its eboot is retained under `build/ps4-png-launcher/eboot.bin` with SHA-256
`d1bf4707cfb629edbafae50e6ca9bf12ea12b84208233384ae1cc114c15105ba`.
The normal distribution path is subsequently replaced by package 1.17.
