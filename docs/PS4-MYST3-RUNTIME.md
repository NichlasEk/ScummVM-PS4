# Myst III runtime investigation: package 1.13

## Failure and fix

A visible GDB run of the PS4 executable played the Atrus intro, then exited
with status 42 at the first panorama. SDL asserted `mouse->WarpMouse` in
`ShouldUseRelativeModeWarp`. The stack passed through SDL_SetRelativeMouseMode,
SdlWindow::lockMouse, Myst3::Cursor::lockPosition and loadNodeCubeFaces.
The PS4 SDL driver has no desktop relative-mode/warp implementation.

The PS4 branch of SdlWindow::lockMouse now records the lock state without
calling SDL_SetRelativeMouseMode. VirtualMouse already produces relMouse
deltas independently of the clipped absolute cursor. Myst III consumes those
deltas for its locked camera. Other platform paths are unchanged.

## Verification

- Full package rebuild and PS4 import allowlist passed.
- Visible shadPS4 run passed the intro-to-panorama transition. The user
  confirmed working play; a subsequent screenshot shows the outdoor area
  with Catherine. This is not a full playthrough or physical PS4 test.
- Debug logs: artifacts/emulator/myst3-gdb-fixed.log and myst3-fixed-run.log.
- Screenshot: artifacts/emulator/myst3-fixed-play.png.
- Package version: 1.13.
- Eboot SHA-256: 748d9ddf636376a422791b71b4d20c739cd5af6bc244ff022a1f8442e2b06f54
- PKG SHA-256: 58815e0ddb4d9e5b368834931ec44f44e03f121499ed4a196abdfcc374890bf7

The earlier 1.12 headless run detected heap corruption at musl __bin_chunk
(offset 0x91c4). That failure did not recur during this visible test; the mouse
assertion is a separate confirmed issue. Do not claim that the heap corruption
has been diagnosed or repaired. The original binary is preserved under
artifacts/myst3/debug-baseline and its evidence is listed in PS4-MP3.md.

## Next dependency port

FreeType2 is the next small shared dependency to target: the current source
requires it for Z-Vision and Buried in Time. Zork Grand Inquisitor is a useful
Z-Vision integration test for fonts, panorama input, video, audio and saves.
The Longest Journey also needs Vorbis support, so it is a later milestone.
