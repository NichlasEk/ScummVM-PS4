# FLAC, VPX, A/52 and MT-32 support (2026-09-27)

The shared PS4 configuration now requires `USE_FLAC`, `USE_VPX`, `USE_A52`
and `USE_MT32EMU`. This applies to the ordinary software build and the
experimental Vulkan build. These are decoder/synthesizer additions, not
an assertion that every game or edition has been verified.

| Capability | Implementation | Enables |
| --- | --- | --- |
| FLAC | Static libFLAC 1.5.0 | FLAC-compressed game audio |
| VP8 / VP9 | Static libvpx 1.15.2, decoders only | VP8 movies, including SLUDGE's optional movie path |
| A/52 (AC-3) | Static liba52 0.7.4, float samples | AC-3 sound; together with existing MPEG-2, the Zork Grand Inquisitor DVD movie path |
| MT-32 | ScummVM's bundled Munt emulator | MT-32 MIDI rendering with user-supplied ROMs |

Sources, download URLs and SHA-256 pins live in `scripts/build-ps4-flac.sh`,
`scripts/build-ps4-vpx.sh` and `scripts/build-ps4-a52.sh`. All libraries use
OpenOrbis's target headers and ABI fixup, PIC and portable C, without host
shared libraries. libvpx enables both decoders but no encoders or pthreads.
FLAC uses its native stream container; Ogg-FLAC is disabled. Library notices
and the MT-32 LGPL license are included in the packages.

The shared PS4 source patch also supplies the missing `<cstdlib>` include
before `<cmath>` in Munt, needed by OpenOrbis libc++.

The shared configure script works around ScummVM 2.9.1's `A52_CFLAGS` versus
`A52_FLAGS` mismatch. Required-feature checks prevent silently packaging a
build in which any of these four capabilities was not detected. The ordinary
build's fingerprint and relink checks include all three static libraries.

## MT-32 setup

Select **MT-32 emulator** as the music device and set ScummVM's **Extra Path**
to a directory containing either:

- `MT32_CONTROL.ROM` and `MT32_PCM.ROM`, or
- `CM32L_CONTROL.ROM` and `CM32L_PCM.ROM`.

ROM files are not bundled. Existing music-device settings are preserved.
Synthesis with real ROMs and audible output has not been verified in this change.

## Validation

`scripts/check-ps4-codecs.sh` generates its own sine-wave and solid-color
fixtures with FFmpeg and builds an isolated PS4 executable. It does not use
game data. Launch `build/ps4-codecs-probe/stage/eboot.bin` in shadPS4.

Observed target stdout:

```text
PS4 CODECS: FLAC exact PCM=PASS VP8 frame=PASS A52 audio=PASS
```

- FLAC: stereo 48 kHz/16-bit decoding matches every reference PCM sample.
- VP8: decodes a 64×48 frame and checks dimensions and Y/U/V color values.
- A/52: decodes all six audio blocks of an AC-3 frame, checks sample rate,
  finite bounded samples, and nonzero signal energy.

The executable exits with status 0 on success. The target run was captured
in `artifacts/codecs/target-smoke.log`. VP9 decoding, whole-game movie playback,
long streams, seeking, performance, MT-32 ROM synthesis and physical PS4
hardware are outside this smoke test.
