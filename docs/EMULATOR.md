# Checkout-local ScummVM testing

Run `python3 scripts/run-scummvm-shadps4.py` for an interactive Vulkan session.
The default stage is `build/scummvm-2026.4.0-vulkan-pkg`; use `--stage PATH`
to select another staged app containing `eboot.bin`.
Set `SHADPS4_BIN` to a built emulator, or create the ignored local symlink
`artifacts/emulator/shadps4` pointing to it. No emulator is downloaded automatically.

Each mode has its own portable profile under `artifacts/emulator/<mode>/user`.
ScummVM settings and saves are under that profile's `data/scummvm-2026-vulkan/`.
The launcher uses a profile lock, timestamped console logs, and cleans up its
owned process group. It does not use the shared `.tools` emulator profiles.

For a bounded screenshot check:

```sh
xvfb-run -a -s '-screen 0 1280x720x24' python3 experiments/scummvm-2026.4.0/capture-test.py vulkan
```

Timeout returns 124; this is a stopped test, not a clean guest shutdown.
Screenshots/logs stay under `artifacts/emulator/<mode>/`. An emulator menu check
is not gameplay or physical PS4 validation. Build/package the current sources
before using this workflow to validate code changes; a copied release stage
only validates that release binary.

---

# shadPS4 diagnostic validation — 2026-09-19

Run `./scripts/run-emulator.sh --diagnostic` from the repository. It builds the PS4
diagnostic, then boots the packaged `eboot.bin` directly in shadPS4 0.18.0.
This is PS4 code running in the emulator, not a Linux rebuild of the app.

## Verified

- Main entered; SDL, video, audio, and joystick initialized.
- Three green bars visibly rendered in a 1280×720 emulator screenshot.
- Keyboard N, mapped to Cross, changed the background and restarted the tone.
- An isolated recording of the shadPS4 audio stream contained a 0.333-second
  tone, approximately 441 Hz, with peak sample magnitude 5000 at 48 kHz.
- Keyboard B, mapped to Circle, reached the app's `clean shutdown` log.
- The persistent guest log was written under the emulator's `/data` mount.
- PKG generation and checksum verification passed. No PS4 hardware test yet.

## Fixes behind the successful boot

The previous pacbrew toolchain build crashed before `main` while starting its
small `libSceFios2.prx`. Its allocator also imports `sceLibcMspace*` functions
that this emulator resolves to unimplemented stubs. Its SDL library imports
Piglet/OpenGL functions as well.

The build now uses the existing OpenOrbis SDK at
`/opt/openorbis/OpenOrbis/PS4Toolchain`, including its SDL software framebuffer,
musl C library, and matching SDL sample runtime modules. Host `clang++` and
`ld.lld` compile/link the target; SDK tools create SELF and PKG files. The
original generated pacbrew files remain under `diagnostic/build` and `dist`.
New files use `build-openorbis` and `dist-openorbis` to prevent stale mixing.

The app initially still rendered black despite successful initialization.
The SDL PS4 framebuffer copies using unsigned window coordinates. A 1920×1080
window implicitly centered on a 1280×720 screen is clipped off-screen. Using
the current display size, explicit position (0,0), and a logical renderer size
of 1920×1080 produced the visible bars. Cross now locks the audio device while
resetting its tone counter, and only Cross changes the background.

## Evidence and artifacts

Local artifacts (ignored by Git):

- `artifacts/emulator/diagnostic-green.png`: three status bars.
- `artifacts/emulator/diagnostic-cross.png`: changed background after Cross.
- `artifacts/emulator/verified-boot.log`: successful rendering/input run and
  guest shutdown.
- `artifacts/emulator/guest-verified.log`: copied persistent guest log.
- `artifacts/emulator/tone-isolated.wav` and `audio-check.json`: audio evidence.
- `artifacts/emulator/second-boot.log`: original runtime-module crash.
- `artifacts/emulator/first-boot.log`: first experiment, retained as a truncated
  excerpt because missing allocator calls produced a repetitive log loop.
- `artifacts/emulator/build-openorbis.log`: package build output.

Hardware candidate:

`diagnostic/dist-openorbis/IV0000-SCVM00001_00-SCUMMVMDIAG00001.pkg`

SHA-256:
`f0b58b49c4cfe357aa17dfdbc831245ab0c1343e5beb31a0919676a7bb1446f6`

Emulator settings/data are under `.tools/xdg/shadPS4`. F12 saves screenshots
there. The launcher retains timestamped console logs under `artifacts/emulator`.
Set `EMULATOR_TIMEOUT=180` for bounded sessions; timeout status 124 is not a successful
guest shutdown. Set `EMULATOR_TIMEOUT=0` for unrestricted manual testing.

## Remaining limitations / next step

shadPS4 reports some stubs, a buffer-unregister warning during shutdown, and
`Exiting with status code 0` as a critical diagnostic. The guest reaches its
shutdown log, but the emulator can remain open until closed or timed out.
The initial test also created a default profile under
`~/.local/share/shadPS4`; the launcher uses the project-local profile instead.

The tested input was emulated controller input from the keyboard; a physical
DualShock has not been tested. Booting `eboot.bin` does not validate installation
of the PKG, console firmware compatibility, or actual hardware behavior.

Next: install the hardware candidate on PS4 and verify the same three bars,
tone, Cross, Circle, and `/data/scummvm-ps4-diagnostic.log`. The user subsequently authorized ScummVM integration before the hardware test;
see [SCUMMVM.md](SCUMMVM.md).

References: [shadPS4 release](https://github.com/shadps4-emu/shadPS4/releases/tag/v.0.18.0),
[OpenOrbis SDL framebuffer](https://github.com/OpenOrbis/SDL-PS4/blob/master/src/video/ps4/SDL_ps4video.c).
