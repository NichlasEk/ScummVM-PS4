# Actual TWP startup through the native bridge (2026-09-26)

The isolated SCVM00010 build now passes Thimbleweed Park GOG/1.0.958's
engine shader capability check, compiles its lighting shaders and enters game
rendering in shadPS4. It is not playable yet: the first substantial frame
exhausts the bounded 16 MiB command recording (4,194,303 dwords used), after six
presented application frames. No physical PS4 validation is claimed.

## Changes

- Capabilities come from the live explicit desktop context (78 checked entry
  points, GLSL 120 subset), avoiding a second hidden window. GL version stays
  0.0 and general extensions remain disabled. The 3D shader manager requests
  the fixed 1280x720 color-only window.
- Fresh shader programs may link during recording; existing executable relinks
  are still rejected. A native fixture exercises linking after its first clear.
- Scalar boolean uniforms translate to signed storage and normalize uploads;
  unsigned SPIR-V uniforms and boolean arrays remain rejected.
- Uniform blocks allow 8 KiB per stage and arrays up to 64 elements. Original
  lighting and debug-lighting shaders use eight arrays of 50 lights. This
  increases each draw's current fixed uniform snapshot to 16 KiB; the aggregate
  64 MiB snapshot budget remains enforced.
- Depth write masks are tracked for save/restore; depth testing remains
  unsupported. The optional overlay background capture checks for glReadPixels,
  which is still absent. Game readback/screenshot paths need further work.
- Context replacement presents its acquired frame before quiescing, avoiding
  an abandoned acquired image and poisoned frame owner.

## Reproduction and evidence

Use `scripts/build-scummvm-vulkan.sh`. With the local `twp` target and game data
already installed in the isolated profile, create
`.tools/xdg-scummvm-vulkan/shadPS4/data/scummvm-vulkan/start-twp-once`, then run
`EMULATOR_TIMEOUT=0 scripts/run-scummvm-vulkan.sh`. The marker is consumed before
launch; subsequent runs open the launcher. Game data are not shipped in Git.

Local evidence: `artifacts/scummvm-vulkan/twp-lighting-visible.log`,
`twp-lighting-scummvm.log`, `twp-lighting-driver.log`, and
`twp-lighting-bootstrap.log`. Earlier runs isolated boolean reflection,
context handoff and missing depth-state entry points before reaching this gate.

Host ASan/UBSan bridge, loader and compiler checks passed, including original
lighting source reflection, exact 8 KiB boundary/overflow and array limits.
The final SDL native fixture passed **2,553 pixel checks and lifecycle cleanup**,
including scalar boolean execution and fresh linking during recording.

Next: measure and reduce initialization/copy command volume and redundant
texture snapshots. Increasing command capacity alone is not the intended fix.
Later gates include texture/FBO deletion ordering, readback, line primitives,
program-slot lifetime and longer gameplay. The normal 1.17 build is unchanged.

## Later rendering and input checkpoint

[PS4-TWP-RENDERING.md](PS4-TWP-RENDERING.md) supersedes the buffer-exhaustion
status: the game menu is visible, analog pointer input works and the user
started the game. Rendering is faster but not yet smooth.
