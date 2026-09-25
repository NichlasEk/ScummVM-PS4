# PS4 Theora decoder port

Package 1.17 adds static libtheoradec 1.2.0. Together with PNG and the existing
Ogg/Vorbis ports this enables Broken Sword 2.5 and Tetraedge (103 main engines).
Tetraedge requires supported game editions; this does not add support for
original Windows Syberia data. Physical PS4 and full-game tests remain open.

## Source and build

`scripts/build-ps4-theora.sh` downloads the official
[Xiph release](https://xiph.org/downloads/):
`https://downloads.xiph.org/releases/theora/libtheora-1.2.0.tar.gz`.
SHA-256: `279327339903b544c28a92aeada7d0dcfd0397b59c2f368cc698ac56f515906e`.

The autotools cross-build uses the OpenOrbis target, static PIC, target Ogg
headers/library, and disables assembly, encoder, examples and documentation.
Only `libtheoradec.a` and its public headers are installed, under
`build/ps4-theora/install`; no encoder or legacy combined library is packaged.
The BSD notice is included in the PKG. Configure checks and main/SOTC/PICE
build fingerprints/relink checks include this dependency. No SDK changes or
host-library substitutions are used.

## Decoder verification

Run `scripts/test-ps4-theora-build.sh`. FFmpeg creates three synthetic 2-second
clips (12 fps, 162x98) in YUV 4:2:0, 4:2:2 and 4:4:4. The odd macroblock sizing
exercises picture cropping. The harness demuxes Ogg, parses the three Theora
headers, decodes keyframes and interframes, hashes cropped planes in display
order, and checks frame count and completion. Output hashes and final timestamp
are independently compared to FFmpeg's decoded raw pixels. Truncated and
invalid streams must be rejected.

Host and PS4 outputs match exactly; shadPS4 reports guest exit 0:

```text
clip420.ogv 162x98 fmt=0 frames=24 yuv-fnv1a=4c87185b end=2.000
clip422.ogv 162x98 fmt=2 frames=24 yuv-fnv1a=08806080 end=2.000
clip444.ogv 162x98 fmt=3 frames=24 yuv-fnv1a=bde4210b end=2.000
truncated.ogv rejected
invalid.ogv rejected
PASS
```

Evidence: `artifacts/emulator/theora-smoke.log`, `theora-smoke-result.txt`.
Generated fixtures, their hashes, expected pixels and native/target harnesses
are under `build/ps4-theora-smoke`. Compare the isolated emulator profile's
`data/theora-smoke.txt` against `expected.txt`.

## Broken Sword 2.5 data

The user-supplied `artifacts/brokensword25/bs25-setup.exe` and its `.bin` were
extracted with innoextract, without executing the installer or modifying the
originals. Game data is under `artifacts/brokensword25/extracted/app`.
The game archive contains PNG assets and Ogg/Vorbis/Theora menu and intro video.
It is only staged locally for verification and is never included in the PKG.

## Full executable and game check

The complete PS4 build links 103 static main engines; the import allowlist
passes with only the existing six PS4 libraries. The PKG manifest contains
61 files, all expected executable/runtime/support/license assets; the only
`games/` entry is README.txt. Both new codec notices and the zlib notice are
included. The user's Broken Sword data is a local staging symlink, not a package
payload.

In an isolated shadPS4 0.18.0 profile, the launcher detected the supplied archive
as Broken Sword 2.5 (English), then opened the original animated main menu.
Controller-generated pointer movement and Cross selected New Game. The intro
advanced through multiple shots with rendered captions. An isolated recording
of only the emulator's audio stream contained stereo 44.1 kHz output (278528
frames, peak absolute sample 302), proving nonzero mixer output, not subjective
quality or audio/video synchronization. The bounded test session ended before
an interactive gameplay/save-load test; those and physical PS4 remain pending.

Evidence: `artifacts/emulator/bs25-auto.log`, `bs25-auto-start.png`,
`bs25-newgame-hover.png`, `bs25-intro.png`, `bs25-intro-later.png`, and
`bs25-audio.json` / `bs25-audio.wav`. The isolated profile is
`.tools/xdg-bs25-test/shadPS4`; existing user configurations/saves are untouched.

Package: `dist/ScummVM-PS4-2.9.1.pkg`, application version **1.17**.
SHA-256: `c519b288b663d4f7b5db0e86b23a2a7031f1dacc97984edaf9b807f3738191e1`.
Eboot SHA-256:
`0aaf4005a904dce442020fe46a1c109bcdb1d6dbe12585525930293833502cde`.

The next graphics milestone is documented in [PS4-OPENGL.md](PS4-OPENGL.md).
