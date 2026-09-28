# Jonssonligan: Jakten pa Mjolner — Director 7 experiment

**Full-build integration (2026-09-27):** this engine/extension is included in
our combined Vulkan package. See [FULL-BUILD.md](FULL-BUILD.md) for activation,
build commands and the distinction from the historical isolated testbenches.
The progression notes below do not imply a complete verified playthrough.

The supplied Windows CD tree is retained unchanged at
`artifacts/jonssonligan_jakten_pa_mjolner`. The experimental emulator profile
links its `games/jonssonligan1` to that root (not only `toInstall`).
The stock detector matches the Swedish Director 7.02 release:
`toInstall/Jönssonligan.exe`, 2,432,602 bytes, final-5000-byte MD5
`f9299bd39817cbfadad4affa1187777d`.

Director is already compiled into the 106-engine experimental build.
The local profile has this target:

```ini
[jonssonligan1]
engineid=director
gameid=jonssonligan1
language=sv
platform=windows
path=/data/scummvm-vulkan/games/jonssonligan1
ps4_director7_experimental=true
start_movie=toInstall/Game.exe
```

The runtime overlay in `scripts/scummvm-vulkan-runtime.py` carries a narrow
opt-in backport based on upstream commit
`5f91b1bbd2123c7dd3fa11c8153fa617e4c1fea0` (2025-08-25): D5-D10
share CASt headers. Our gate only extends through D7 for this game and only
with the explicit flag. It does not enable other later Director games.
Build with `scripts/build-scummvm-vulkan.sh`.

## Backported parser support (2026-09-26)

`patches/scummvm/director7-experimental.patch` is applied by the runtime
overlay. It adds the indexed D6/D7 score container and D7 48-byte sprite
records, D7 bitmap metadata/BITD decoding and script-cast headers, 32-bit
cast-library keys, and Lingo code/name storage handling. Score offsets,
channel counts and name lengths are checked before use. RGB sprite extras,
rotation/skew and several newer configuration fields are not rendered yet.
The score and CASt version extension is opt-in for this target; shared
resource-ID and bytecode fixes also apply to existing Director readers.

The 32-bit fix is material: this game's cast key is 66560 (0x10400), which
was previously truncated to 1024. The default cast now adopts the movie's
key; external casts resolve their own key from the archive. Lingo function
tables can precede bytecode in D7, and name chunks can contain trailing data.

Frame/score parsing was adapted from local upstream reference `1fe4e8e4090`
(2025-08-29). Bitmap and Lingo format handling was checked against the
upstream `engines/director/castmember/bitmap.cpp`, `cast.cpp`, `movie.cpp`
and `lingo/lingo-bytecode.cpp` files retrieved from ScummVM master on
2026-09-26. These are source-level backports, not a wholesale engine upgrade.

## Runtime work in progress (2026-09-26)

The opt-in target now starts the actual Game.exe projector, loads the six
cast libraries, renders the original menu in 32-bit color, and runs the
original New Game button handler. The menu's sound fade finishes and its
script switches into Stockholm.DIR. This has been exercised in a visible
shadPS4 window, not on physical PS4 hardware.

Additional compatibility work in the patch:

- A fixed virtual DisplayRes implementation and TMSTools CD discovery backed
  by real SearchMan file checks; original Windows Xtras are not executed.
- SavefileManager-backed GetPref/SetPref, platform/applicationPath, sprite
  references, symbol/void builtins and D5+ object-call opcodes.
- Indexed cast-library mapping and single unparameterized sprite behaviors.
- D7 PCM sound headers/sample data and sound looping flags, plus exact fade
  completion. The supplied ljudfx library's 244 sound headers are PCM.
- Full D7 main-channel reconstruction before decoding partial deltas. An
  offline structural check covered 25 scene files, 4,519 frames and 64,839
  deltas without out-of-range channel spans.
- Correct sprite.member assignment: update its cast reference instead of
  duplicating the source bitmap into the old cast slot.
- mouseLoc/frontWindow properties and input argument validation for inside.

QuickTimeVersion reports the level needed by this game's SVQ1/PCM movies;
this is an opt-in game shim, not a claim of complete QuickTime 4 support.
D7 film loops, full multiple-behavior instances/initializers, some text and
configuration fields are still incomplete. Sound subloop ranges and cue
points are not implemented by the small PCM reader.

**Basic scene interaction now works; complete gameplay is unverified.**
The list-key and mouseLoc errors have been addressed.
Windows D7 font IDs also needed correction: their high bit was incorrectly
selecting Japanese text decoding and corrupting Swedish resource names.
The frame-61 memory exhaustion was traced to `getPos`: A* searches for
fresh coordinate lists, but identity-only comparison reported coordinates
already present in the visited list as absent. An opt-in title-specific
value-comparison rule now lets this search complete. The Director version
boundary for this rule is not yet established; it is not enabled globally.
Two visible runs subsequently reached the attic and continued executing
frame scripts without the former `bad_alloc`.

Further work adds D7 shape records, locZ depth ordering, releases a stale
in-flight input marker when a movie is reset, and lets score behavior bindings
update even on puppeted pictures. D7 per-property overrides no longer freeze
untouched ink, removing the white boxes around the characters. Repeating a
frame preserves script edits as the baseline for the next render.

The `sprite.member` property was missing from the object-property table. A
read therefore returned VOID and the game's inventory script repeated its
12-step bag-closing animation on every frame. Registering the existing member
getter/setter reduced idle frame script time from roughly 1,000 ms to 5–9 ms.
A 100-frame sample in the attic had 69/75/84 ms min/median/max intervals
(about 13 frames per second; the movie requests 15). Floor clicks dispatched
the original behavior and visibly moved a character to two destinations.
This is actual PS4-binary execution in shadPS4, not a physical-console test.

Long Lingo handlers pump input/presentation on a wall-clock cadence instead
of presenting every 100 opcodes. Debug-only object formatting is gated before
formatting; the PS4 bootstrap no longer forces debuglevel 5, so the standard
ScummVM config controls verbosity. These shared changes still need regression
coverage with other Director titles.

Object use, character switching, saving/loading and later scene progression
remain unverified. Small rendering artifacts and unsupported Director features
remain. Do not treat the walking test as a full gameplay compatibility pass.

Useful evidence under `artifacts/scummvm-vulkan/`:

- `jonssonligan-menu32-result.log`: decoded menu and shared-library scripts.
- `jonssonligan-stockholm-compile.log`: decoded Stockholm scripts.
- `jonssonligan-main-deltas.log`: New Game passed the header-delta crash.
- `jonssonligan-member-reference.log`: reached scene frame 116.
- `jonssonligan-mouse-entities.log`: trace identifying property-list key access.
- `jonssonligan-font-encoding.log`: original frame-61 memory exhaustion.
- `jonssonligan-memory-trace.log`: A* trace showing existing coordinates missed.
- `jonssonligan-list-values.log`: attic remains running after the list fix.
- `jonssonligan-input-reset.log`: clicks reach the new movie after reset.
- `jonssonligan-span-preservation.log`: floor behavior and first walk.
- `jonssonligan-fast-walk.png`: clean character sprites after a floor click.
- `jonssonligan-builtin-costs.log`: repeated inventory updateStage calls.
- `jonssonligan-member-property.log`: timing after registering sprite.member.
- `jonssonligan-clean-validation.log`: rebuilt executable without timing probes.

Timing probes have been removed from the patch. Its consolidated forward and
reverse application were checked byte-for-byte (30 files). The package builds
successfully; runtime warnings for incomplete Director features still remain.

2026-09-27 candidate SHA-256:
- eboot.bin: `327aa21c5cd5efdff63802a3b7465383b2331d39fe433aec2b6a020c6442462c`
- PKG: `4a3d0cd31b6f58837b2dc29922e88bda0428b1cb9bec57de36a11985ced258fb`

The visible validation run retains event logging in memory; the on-disk
profile has had temporary debug settings and warning suppression removed for
the next launch.

The work is not committed yet. Watchmaker remains deferred while this
experimental Director support is tested.

## Next game available

`artifacts/thewatchmaker/THE_WATCHMAKER.iso` is readable and contains
`program files/Trecision/The Watchmaker` plus `Speech`. No original files
were modified. Watchmaker still requires the classic OpenGL rendering port;
its presence on disk does not enable the engine or establish compatibility.
