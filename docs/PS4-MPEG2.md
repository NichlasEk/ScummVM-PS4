# MPEG-2 decoder port

Implemented 2026-09-26. The shared PS4 configuration now requires static
libmpeg2 and enables QD Engine when building all engines. The all-engine
configure check selects 104 main engines, up from 103. The existing installed
game packages have not been replaced by this test.

## Source and build

`scripts/build-ps4-mpeg2.sh` builds libmpeg2 0.5.1 from the
[VideoLAN source archive](https://download.videolan.org/pub/contrib/libmpeg2/libmpeg2-0.5.1.tar.gz).
The upstream [project page](https://libmpeg2.sourceforge.io/) describes the
decoder; its original download URL currently returns 404.

Archive SHA-256:
`dee22e893cb5fc2b2b6ebd60b88478ab8556cb3b93f9a0d7ce8f3b61851871d4`.

The build compiles the portable decoder sources directly with OpenOrbis headers,
static PIC and no architecture acceleration macros. It omits assembly, CPU
probing, players and the separate RGB conversion library; ScummVM performs its
own color conversion. Output is `build/ps4-mpeg2/install/lib/libmpeg2.a`.
The GPL-2.0-or-later source notices are retained, and `libmpeg2-COPYING` is
included in the package manifest. No system SDK files are modified.

The shared configure script checks `USE_MPEG2` rather than silently disabling
it. Main, SOTC and PICE build fingerprints and static-library relink checks
include the dependency. Vulkan builds use that shared configure script too.

## Repeatable test without game data

Run:

```sh
scripts/run-ps4-mpeg2-smoke.sh
```

This builds three synthetic two-second, 25 fps, 162x98 MPEG-2 elementary streams:
progressive YUV 4:2:0, progressive YUV 4:2:2, and top-field-first interlaced
4:2:0. FFprobe verifies the encoded format/interlacing. FFmpeg independently
decodes reference raw YUV. Fixtures explicitly include a sequence-end marker.

An AddressSanitizer host harness and the actual PS4 ELF each decode all three
streams twice, using 4096-byte and 17-byte input chunks. Checks cover cropped
picture dimensions, frame period, 50 displayed frames, I/P/B frame types,
display order, final-frame draining, and all visible YUV samples. Host and PS4
reports must match exactly. Against FFmpeg the fixture permits at most three
levels of difference per sample and mean absolute error 0.2, allowing integer
IDCT rounding differences between decoders.

Observed on host and in shadPS4:

| Fixture | Frames (I/P/B) | YUV FNV-1a | Maximum difference from FFmpeg | Mean absolute difference |
| --- | --- | --- | --- | --- |
| 4:2:0 | 50 (5/13/32) | `9a90b5c4` | 2 | 0.01065 |
| 4:2:2 | 50 (5/13/32) | `00245089` | 2 | 0.01067 |
| Interlaced | 50 (5/13/32) | `3e1a3de6` | 2 | 0.01120 |

Both chunk sizes produce the same results. Truncated and invalid fixtures fail
the harness's completeness checks. This is a test policy, not a claim that
libmpeg2 universally rejects truncated streams; it supports stream recovery.
The host sanitizer reports no memory errors. The guest finishes with exit 0.
The runner terminates only its own emulator process group after completion.

Evidence:

- `artifacts/emulator/mpeg2-smoke.log` and `mpeg2-smoke-result.txt`.
- `build/ps4-mpeg2-smoke/expected.txt`, `input.sha256`, and `*-probe.json`.
- Private profile `.tools/xdg-mpeg2-smoke`; user saves/settings are untouched.

## ScummVM integration and limits

An isolated QD-only ScummVM executable compiled and linked successfully under
`build/scummvm-mpeg2-check`, with `USE_MPEG2` and `ENABLE_QDENGINE` enabled.
Its imports remain the six existing PS4 system libraries, with no host-library
dependency. An all-engine configure check under `build/scummvm-mpeg2-all-config`
selects 104 main engines. This is not a completed all-engine package rebuild.
The isolated ScummVM build still reports pre-existing warnings in shared UI,
SDL and FM-synth sources; the codec and smoke harness compile with `-Werror`.

The test verifies video decoding, not program-stream demuxing, audio/video
synchronization, in-game cutscenes, arbitrary damaged-file robustness or
physical PS4 execution. Those remain separate integration checks.
