# Jönssonligan: Jakten på Mjölner — performance candidate 0.31

The tester accepted 0.30 on physical PS4 but reported slow character movement
in the attic. The earlier channel-order and Lingo lookup optimizations already
apply to both Jönsson games; reapplying them would not address this report.

## Change

For Mjölner scene scripts, keep polling input and updating widgets but leave
presentation to the main loop after the frame has finished. Hidden helper
windows created and forgotten inside the scripts invalidate the window manager;
presenting during each script can repeatedly upload the old Stage image.
Mid-script video presentation and Går på djupet's script cursor remain enabled.
Explicit script-driven Stage updates are not removed. Animation tempo and game
movement values are unchanged.

Restart the input-service interval after a blocking presentation completes,
so the next short behavior does not immediately request another presentation.

## Evidence and limits

In separate, single-process shadPS4 attic runs, the median `DirectorEngine::draw`
call rate fell from 30.65/s to 19.70/s (22 baseline and 10 candidate measurement
windows). These are presentation requests, not rendered FPS. The measured
`Window::render` invocation rate stayed near 9.95–9.97/s. Thus the measurement
shows about 36% fewer redundant presentation requests, not a demonstrated
increase in animation frame rate. The tester reported that movement felt better
in the candidate. Physical PS4 comparison remains necessary.

Earlier runs with overlapping emulator processes were excluded. Startup and
the first two attic samples were excluded; only intervals below 5.5 seconds
were used. These wall-clock measurements include waits and scheduling, not just
CPU time. Local data: `artifacts/jonsson-perf-20261025/comparison.json`,
`clean-baseline-profile` and `stage2-profile`.

The source-extracted input-service regression covers many short scripts,
blocking presentation, millisecond rollover, disabled games, Mjölner scene
input, video updates and the second game's presentation path. The original
blocking-present behavior failed the regression; the candidate passes. The
actual game-ID gate is included in the test. Cross-build and patch replay
passed. The shipping build contains neither timers nor automated test inputs.

MT-32 pacing, the 0.30 shared sound ownership, video sizing and video clearing
are preserved. Long-term stability and other scenes remain unverified.

## Package

- File: `dist/ps4-jonsson-031/ScummVM-2026.4.0-Vulkan-Test.pkg`
- Title: `SCVM00265`; APP_VER: `0.31`
- Content ID: `IV0000-SCVM00265_00-SCUMMVM2026VK001`
- PKG SHA-256: `09e56870480e611ac30eb9b50f2baa7ec5a0145f2a947e87032aa4fbbe35c339`
- Eboot SHA-256: `26ff1ccb90a1056902c3e4746410402dd7ccacc19ddce24c8bd424e29ca335ae`

Rollback: unchanged 0.30 in `dist/ps4-jonsson-030/`, SHA-256
`ba24bbc4b6fa7ea910f7a931386892aeed57f12e0fe3f33acadc8e866b7e8bca`.

Package signatures/digests passed validation. All 240 non-SFO payloads matched
staging after extraction; the packaged SFO identifies SCVM00265 / 0.31.
The complete package was uploaded and read back with matching SHA-256 at
`192.0.2.4:/data/pkg/ScummVM-2026.4.0-0.31.pkg`. It was not installed by
the agent. Delivery journal: `artifacts/ftp-jonsson-031/delivery.json`.
