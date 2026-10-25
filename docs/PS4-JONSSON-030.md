# Jönssonligan: Jakten på Mjölner — candidate 0.30

Checked on 2026-10-25 using the isolated ScummVM 2026.4.0 Vulkan port.

## Reproduced causes and changes

The existing port routes every Director window to the Stage sound manager,
but still allocated a private sound manager and permanent PC speaker for each
helper window. Repeated `block.dir` helpers on the attic frame consumed mixer
slots. Forgetting any helper also called `stopSound()` on the shared Stage
manager, silencing the music. Only the Stage now owns the manager; helper
movie changes and window disposal do not reset or stop the Stage audio.

The QuickTime Xtra video casts in this game acquire their dimensions when
loading the linked media. Sprite dimensions were cached before that load,
leaving the robot and polar clips at 0 × 0 while audio/time advanced. Resolve
video dimensions before sizing Jönsson sprites. This is game-specific; other
games can intentionally use zero-sized videos as hidden audio/timing clocks.

Direct-to-stage video was also treated as persistent sprite trails. Preserve
explicit trails, but do not infer them from direct-to-stage mode for Jönsson.
This restores the background when the next movie has different dimensions.
Video layer ordering is unchanged.

The durable changes are in `experiments/scummvm-2026.4.0/director7.patch`.
Temporary mixer/window diagnostics were removed from the final source and
binary. SDL audio pacing, MT-32 and the graphics bridge were not modified.

## Validation and limits

- PS4 cross-build and patch dry-run against clean upstream passed.
- Existing channel ordering test passed 800 randomized scenes with three
  mutations each under host address/undefined-behavior sanitizers.
- The tester confirmed attic music continued in the fixed shadPS4 run.
  Instrumentation recorded one sound manager, at most three occupied slots
  before a new stream allocation, and no mixer-slot exhaustion.
- Visible shadPS4 captures show IQ Media, the robot and the polar intro.
  The final polar capture has a clean black surround instead of the robot
  image remaining underneath.
- Package integrity validation passed. All 240 non-SFO payloads match the staging
  files after extraction; SFO identity/version match (the packager adds its
  PUBTOOL metadata). The final extracted executable was also tested
  in shadPS4: all opening clips displayed, the polar clip had a clean black
  surround, and the tester accepted the result.

Local evidence is under `artifacts/jonsson-audio-20261025/`: `lifecycle-profile`,
`fixed-profile`, `video-profile`, `video-fixed-profile`, and `video-clear-profile`
keep the distinct runs. Game assets, logs and captures are not committed.

The earlier PS4 CE-34878-0 report has no recovered native stack. Fixing the
reproduced mixer exhaustion does not establish the exact cause of that crash.
Prolonged play and these changes on physical PS4 still need testing. No console
installation or FTP delivery was performed for 0.30.

## Package

- File: `dist/ps4-jonsson-030/ScummVM-2026.4.0-Vulkan-Test.pkg`
- Title: `SCVM00265`; APP_VER: `0.30`
- Content ID: `IV0000-SCVM00265_00-SCUMMVM2026VK001`
- PKG SHA-256: `ba24bbc4b6fa7ea910f7a931386892aeed57f12e0fe3f33acadc8e866b7e8bca`
- Eboot SHA-256: `8b26ef760202aed67a7b1e41b00a9e9f43feaa84edb9aae2c282fdf785b1dd9f`

Rollback remains the unchanged 0.29 package in `dist/ps4-compat-029/`, SHA-256
`39f640f68bbb474a6204cb57c9ac889acf469e0429547f05a1b924728d5de52f`.
