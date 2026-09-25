# PS4 JPEG port and Myst III preparation

Package 1.11 enables JPEG using [libjpeg-turbo 3.2.0](https://github.com/libjpeg-turbo/libjpeg-turbo/releases/tag/3.2.0).
The official source tarball is pinned to SHA-256
`6f30092cef9fb839779646608f4ee14ae3cbac989c47fa05e841b0841f09878e`.

## Build

`scripts/build-ps4-jpeg.sh` cross-compiles a static PIC library for
`x86_64-pc-freebsd12-elf` using OpenOrbis headers and the shared PS4 ABI header.
SIMD is disabled for this initial port. Shared libraries, TurboJPEG, tools and
upstream test executables are not built for PS4. The isolated installation is
`build/ps4-jpeg/install`; the SDK installation is not modified.

The generic and preview build scripts track the dependency fingerprint and
relink when the archive changes. ScummVM's JPEG configure probe uses this
prefix. The package includes both libjpeg-turbo's license and IJG notice.

Run `scripts/build-scummvm.sh` to produce
`dist/ScummVM-PS4-2.9.1.pkg` (SCVM00002, package version 1.11).
Myst III, Myst Masterpiece and Groovie 2 are enabled. Myst III uses TinyGL;
this does not add a hardware OpenGL backend.

## Decoder verification

Run `scripts/test-ps4-jpeg-build.sh`. It builds the same decoder without SIMD
for Linux and PS4, and decodes three upstream fixtures into RGB using integer
IDCT. The PS4 test executable is `build/ps4-jpeg-smoke/stage/eboot.bin`.
Use an isolated shadPS4 profile; it writes `/data/jpeg-smoke.txt` in that
profile. Compare with `build/ps4-jpeg-smoke/expected.txt` using `cmp`.
The fixture images are build/test inputs only, never packaged with ScummVM.

Verified in shadPS4 0.18.0: target output matches the host byte for byte:

```text
testorig.jpg 227x149 RGB fnv1a=f29cc190
testimgint.jpg 227x149 RGB fnv1a=b5106c06
testimgari.jpg 227x149 RGB fnv1a=b5106c06
PASS
```

Evidence: `artifacts/emulator/jpeg-smoke.log` and `jpeg-smoke-result.txt`.
This exercises baseline and arithmetic JPEG decoding and the target runtime.
It is not a Myst III gameplay test or a physical PS4 test.

## Game validation (English CD 1.2, disc 1)

The actual PS4 executable opened the original Myst III main menu in shadPS4.
Screenshot: `artifacts/emulator/myst3-start.png`; logs: `myst3-launch.log` and
`myst3-guest.log`. Selecting New Game failed with
`Unable to open archive TOHOnodes.m3a!`. This file is absent from the supplied
first disc, which contains the six `MA*nodes.m3a` archives. Additional disc data
is needed to test the first panorama. Menu MP3 music was skipped because MAD
is still disabled; uncompressed sound has not been assessed in this test.

Package SHA-256:
`c2d0b8c2eedb7eade2ea9730bfaf1211423c985260955d5bf3a892882408750c`.
The package import allowlist passed and `myst3.dat` was included automatically.
No game data was included in the package.

## Follow-up in package 1.12

All four discs are now extracted at `artifacts/myst3/extracted`. MP3 menu
music works, and New Game reaches the intro movie before a heap-integrity
crash. See [PS4-MP3.md](PS4-MP3.md) for the updated evidence and next step.

## Original game-validation plan

Place the user's Myst III disc images under `artifacts/myst3/iso/` and extract
the game data into a separate directory. Preserve the source images. Verify
the edition and required files before adding the game to ScummVM. Test startup,
panorama rendering, movies/audio, input and saving/loading in shadPS4, then on
physical PS4. No game images or extracted proprietary data belong in Git or
the general-purpose package.
