# Isolated ScummVM 2026.4.0 PS4 experiment

Checkpoint 2026-10-15: both software and Vulkan builds with SCUMM, PICE and
SOTC link, produce validated PS4 packages and visibly display the 2026.4.0
launcher in fresh shadPS4 profiles. Vulkan reports two successful presents.
These are emulator launcher checks, not gameplay/input/save/load or physical
PS4 validation. The combined Vulkan build now links with **126 top-level engines**, including
Watchmaker and Macs2, and its launcher is also visually verified in a fresh
shadPS4 profile. The subsequent Watchmaker runtime fix reaches its menu and
opening 3D scene; see the APP_VER 0.03 checkpoint below.

The existing 2.9.1 port and packages are unchanged. The experiment uses
`.tools/upstream-2026.4.0/`, `build/scummvm-2026.4.0-*`,
`artifacts/upstream-2026.4.0/` and `dist/upstream-2026.4.0/`.
No commercial game data is included.

## Source and dependencies

Clean source: `.tools/upstream-2026.4.0/scummvm-2026.4.0/`.
Official archive:
https://downloads.scummvm.org/frs/scummvm/2026.4.0/scummvm-2026.4.0.tar.xz

SHA-256: `cd660b34104da7f85ed0a11b7c6c2320c629bb87ee297935711ebc1e09731f12`.
Verify before extracting into `.tools/upstream-2026.4.0/`.

The recipes reuse this checkout's existing OpenOrbis SDK and compiled codec,
SDL, OpenGNM, Vulkan, PSBC and glslang dependencies. They are not a fresh-SDK
bootstrap or a reproducible toolchain release. Vulkan bridge objects are copied
from `build/scummvm-vulkan/bridge/`; the main wrapper is rebuilt with isolated
settings/log paths. Changes stay in the generated source copies. `prepare.sh`
checks patches before applying and will fail on conflicting edits.

## Build and package

Run from the repository root:

```sh
bash experiments/scummvm-2026.4.0/build.sh software
bash experiments/scummvm-2026.4.0/package-software.sh
bash experiments/scummvm-2026.4.0/build.sh vulkan
bash experiments/scummvm-2026.4.0/package-vulkan.sh
```

The default engine list is `scumm,pice,sotc`. Pass `all` as a second argument
for the broad experiment, or a comma-separated engine list. Building does not
package automatically. Packaging replaces that mode's test package; use
`SCUMMVM_DIST_DIR` and `SCUMMVM_STAGE_DIR` for a separate Vulkan candidate.
Do not run builds/configure concurrently against the same build directory.

| Variant | Test title ID | Guest settings/saves | Package |
| --- | --- | --- | --- |
| Software | SCVM00264 | `/data/scummvm-2026-software` | `ScummVM-2026.4.0-Software-Test.pkg` |
| Vulkan | SCVM00265 | `/data/scummvm-2026-vulkan` | `ScummVM-2026.4.0-Vulkan-Test.pkg` |

Both initial packages have APP_VER 0.01. They are local experiments, not releases.
Vulkan uses the controller-only startup marker. The isolated package recipe
allows only the new Vulkan test title ID/name. Upstream engine data and shader
manifests determine which support files are packaged.

## Evidence at the initial checkpoint

Artifacts in `artifacts/upstream-2026.4.0/`:

- `software-launcher.png`, `vulkan-launcher.png`: visually inspected launchers.
- `package-{software,vulkan}-validation.log`: PKG integrity/signature checks pass.
- `focused-build.log`, `vulkan-build.log`: final successful links; logs also retain
  earlier failed attempts for diagnosis.
- `scummvm-vulkan-focused.elf`, `config-vulkan-focused.mk`: focused Vulkan checkpoint.
- Per-mode emulator logs and isolated profiles under the source parent directory.

Initial package hashes:

- Software: `d6e196783e7e4434bd613330fb51cc153a04f39c571fc14951a0140aa51725e6`
- Vulkan: `32b56581d2c9ee9ee2036204cb9dd6125af740b389ce00ec30549d37857366b8`

Capture the already staged build with a bounded run:

```sh
xvfb-run -a -s '-screen 0 1280x720x24' \
  python3 experiments/scummvm-2026.4.0/capture-test.py vulkan
```

This reuses only emulator graphics settings when creating the dedicated profile.
It does not start a game or verify input; it captures after 30 seconds and ends
only its emulator process group.

## Port changes and current scope

`software.patch` and `vulkan.patch` each apply to clean 2026.4.0. They contain
rebased PS4 storage/input/window/framebuffer changes. The Vulkan variant ports
the bridge to upstream's unified SDL/OpenGL backend. Upstream already includes
the texture bind guard and updated rotation logic, so obsolete hunks were omitted.
`local-engines.patch` applies after copying the maintained PICE/SOTC engines:
three cursor API calls and the renamed `SV_SWE` language identifier are adapted.
All three patches passed zero-fuzz dry runs against their actual baselines.

Old Director 7, Watchmaker, HPL1 and TWP game-specific overlays are not imported
wholesale. The initial broad configuration enables 124 top-level engines but
skips Watchmaker (classic GL) and Macs2 (ImGui), plus intentionally excluded
TestBed and Playground3D. Enabling engines does not establish gameplay support.


## Watchmaker and Macs2 follow-up

`extra-engines.patch` enables both missing engines in the isolated Vulkan tree:

- Macs2 uses MIDI without requiring the optional ImGui debugger. Its existing
  module/callback guards are retained; the three unguarded debug-overlay calls
  are now guarded as well. The ordinary game drawing path is unchanged.
- Watchmaker uses the existing PS4 bridge's bounded immediate-mode-to-shader
  implementation rather than enabling classic OpenGL for every engine. The
  maintained experimental engine patch is rebased onto 2026.4.0, retaining
  upstream's RGBA32 format change and adapting the changed vertex-color code.
  This includes the previous archive/structure, rendering and sound fixes;
  compilation alone is not new gameplay validation.

Both engine libraries and a separate executable containing these two engines
link successfully (`missing-engines-build.log`, `missing-engines-link.log`).
The full configuration now selects 126 top-level engines and skips only TestBed
and Playground3D. The original title-specific Director 7, HPL1 and TWP patches
remain outside this experiment; enabling their upstream engines does not restore
all behavior from our old experimental port.

The patch replay check applies all patches to a separate copy of pristine
2026.4.0 and the maintained local engines. All 77 modified files match the active
Vulkan source byte-for-byte. Re-running preparation is a no-op. The reverse
check uses `--forward` as well as `-R`, preventing GNU patch from treating a
clean, unpatched tree as an already patched one by auto-reversing its operation.

The initial focused packages were fully extracted with `pkg_makegp4` and compared
to their GP4 manifests: all 40 software and 46 Vulkan payloads match by SHA-256.
Use `check-payload.py STAGE EXTRACTED` to repeat this check. `pkg_extract` alone
omits package metadata and outer-PFS module files, so use `pkg_makegp4` here.

Full-build recipe, with separate package output:

```sh
BUILD_JOBS=12 bash experiments/scummvm-2026.4.0/build.sh vulkan all
SCUMMVM_STAGE_DIR="$PWD/build/scummvm-2026.4.0-vulkan-full-pkg" \
SCUMMVM_DIST_DIR="$PWD/dist/upstream-2026.4.0/full" \
SCUMMVM_APP_VERSION=0.02 \
  bash experiments/scummvm-2026.4.0/package-vulkan.sh
xvfb-run -a -s '-screen 0 1280x720x24' \
  python3 experiments/scummvm-2026.4.0/capture-test.py vulkan-full
```

The full candidate uses the experiment's SCVM00265 identity, separate from the
original SCVM00010 port. Its guest paths are isolated from the old port, and the
emulator full-candidate capture uses its own profile.


## Full candidate result (APP_VER 0.02)

- Package: `dist/upstream-2026.4.0/full/ScummVM-2026.4.0-Vulkan-Test.pkg`.
- Title ID: `SCVM00265`. 126 static engines; only TestBed and Playground3D excluded.
- Package SHA-256: `83913ffff6403fc6143c295dd5273b561c1b8bf83712970b8808d9f3d76743a7`.
- Eboot SHA-256: `941192570b0a54f1b61fd31f3d94d97bf8e4f29b2ff76b50bb04434deef68c0a`.
- Full link/import gate passed. Required codecs remain enabled. The exact engine
  configuration is recorded in [ENGINES.json](ENGINES.json).
- `PkgTool.Core pkg_validate` passed. All 239 GP4-listed payloads match the
  fully extracted package by SHA-256 (`vulkan-full-payload-check.json`).
- Fresh-profile shadPS4 startup reached two successful Vulkan presents and the
  screenshot `artifacts/upstream-2026.4.0/vulkan-full-launcher.png` was visually
  checked: the launcher displays ScummVM 2026.4.0 correctly.
- Logs: `vulkan-126-build.log`, `package-vulkan-full.log`,
  `package-vulkan-full-validation.log`, `vulkan-full-emulator.log` under
  `artifacts/upstream-2026.4.0/`.

Watchmaker/Macs2 gameplay, controller input, audio, USB browsing, save/load and
physical PS4 startup are not verified for this candidate. Earlier 2.9.1 results
are not evidence for these new binaries. No games or ROMs are bundled.

Both source preparation paths were replayed from pristine upstream: 77 patched
Vulkan/local-engine files and 30 software/local-engine files match the working
sources byte-for-byte. Shell/Python syntax checks also pass.


## Watchmaker runtime fix (APP_VER 0.03, 2026-10-15)

Watchmaker now passes startup, draws the full-screen menu and renders its opening
3D scene in shadPS4 v0.18.0. The user confirmed startup and accepted the updated
run. This is an opening-scene checkpoint, not a complete-game or physical PS4 test.

- Adapted the unified 3D renderer to the bounded PS4 bridge: explicit state and
  matrix save/restore replace unavailable desktop attribute stacks. Initialization
  avoids unsupported dithering, polygon bias, line-width and desktop-only state
  calls, including the null `glPolygonOffset` call caught in GDB.
- Guarded the optional per-instance Watchmaker archive: when absent, use the
  archive already registered in SearchMan instead of dereferencing a null pointer.
- Use 1280x720 on PS4 so the direct-render viewport and game pointer coordinates
  match the fixed swapchain. Added missing right-button events and key bounds.
- Emulator controls in the isolated test profile: F7 captures/releases the host
  mouse; WASD moves the pointer, N/Cross clicks, B/Circle right-clicks.
- Full 126-engine rebuild passed. A pristine patch replay matches all 81 modified
  files. Package validation and all 239 extracted payload hashes pass.
- Package: `dist/upstream-2026.4.0/watchmaker/ScummVM-2026.4.0-Vulkan-Test.pkg`.
  SHA-256: `ed13770af1ced9688d644012ba4bffc4bbb819559bb040db97bde2422b860dae`.
  Eboot SHA-256: `1374c3841cb7e9821ea8cffddf01c63218bd52a097896ea9ef8cf0cee60e207b`.
  APP_VER is 0.03. It uses the same linked ELF as the live test; SELF metadata
  differs because packaging records the absolute rather than relative ELF path.
  Live-tested eboot SHA-256:
  `a170ecddc9e4a13b0aecc545d82b91aae512a2a878b8f660e3a57a16a716f773`.
- Local evidence under `artifacts/upstream-2026.4.0/`: `watchmaker-fix3.log`,
  `watchmaker-fix3-before-input.png` (menu), `watchmaker-pointer-right.png`
  (opening 3D scene), `watchmaker-package-validation.log`,
  `watchmaker-payload-check.json` and `watchmaker-replay.log`.

Audio playback is logged, but listening quality, sustained gameplay, save/load,
Macs2 and physical PS4 operation still need separate verification. Game data and
runtime screenshots remain local and are not bundled or committed.


## Jönssonligan Director 7 migration (2026-10-15)

`director7.patch` ports the remaining local compatibility layer to the Vulkan
experiment. Enable it only for `jonssonligan1` with
`ps4_director7_experimental=true`; launch the original `toInstall/Game.exe`.
Game data and preferences remain outside the package.

Upstream already supplies D7 cast, bitmap, script and sound readers, cast-library
mapping, sprite-member references and per-property overrides. The old 2.9.1
patch must not be applied wholesale. The new delta preserves these readers and
adds the DisplayRes/TMSTools shims, save-backed preferences, Swedish font/name
handling, true color, mouseLoc/frontWindow/locZ, coordinate-list comparisons,
input reset, behavior updates for puppeted sprites and exact sound-fade completion.
It also reconstructs partial D7 main-channel deltas and accounts for the six main channels in the score header.

The following regressions were reproduced during migration:

- The intro decoder advanced while its sprite had a zero-sized rectangle.
  This target now uses the video cast's native dimensions for un-stretched
  sprites, restoring the visible 600x250 intro. Other titles keep upstream's
  zero-size handling. Film playback was visually checked and user-confirmed.
- New Game exhausted flexible memory while entering Stockholm. Ordinary
  scene playback now omits the full-frame debugger/export cache, which copied
  all 1000 sprite channels per frame. Runtime score delta loading remains in
  use; film-loop and embedded-movie caches are retained. The debugger's
  explicit cached-frame lookup handles an absent cache.
- The old `frontWindow` adaptation incorrectly treated a Mac GUI window as
  a Lingo object in the new engine. It now returns the Director window wrapper
  for this target, avoiding an invalid reference-count access in the first scene.
- Upstream treated every D6+ behavior as a mouse handler, allowing decorative
  sprites to intercept clicks before the floor and actors. For this target,
  behavior scripts must contain a mouseDown/mouseUp or generic handler. This
  restores the old port's click filtering without replacing upstream behavior
  instances. Floor clicks now run the original handlers and visibly move a
  character across the attic in shadPS4. Actor 102 changed position from
  (267,286) to (296,258) during a logged walking animation.

The host shadPS4 audio stream was muted in PipeWire during the initial tests;
this was unmuted separately from the game changes. Runtime screenshots and
logs are under `artifacts/upstream-2026.4.0/jonsson-*`. D7 film loops remain
unsupported; minor rendering artifacts and warnings remain. Walking was also
user-confirmed, but slow; emulator versus engine overhead has not been isolated.
Full gameplay, save/load and physical PS4 validation remain outstanding.


The final non-diagnostic build links with all 126 engines. Clean-source replay
matches all 97 modified files. The live walking test used the same fixes with
temporary diagnostic logging; this logging is removed from the final package.
`pkg_validate` passes and all 239 GP4-listed payloads match a `pkg_makegp4`
extraction. APP_VER 0.04 is staged at
`build/scummvm-2026.4.0-jonsson-release-pkg` and packaged as
`dist/upstream-2026.4.0/jonsson/ScummVM-2026.4.0-Vulkan-Test.pkg`.

- PKG SHA-256: `a6fe6d643c719e4b3941913f48f35cce5f693548b6bf0979f2f44e7819da355a`.
- Packaged eboot SHA-256: `70a3322eac2d43e8b1a4e812b20faf6efb834a137725cbc9362e716e7248839d`.
- Walking evidence: `jonsson-click-filter.log`, `jonsson-click-filter.png`.
- Package/replay evidence: `jonsson-package-validation.log`,
  `jonsson-payload-check.json`, `jonsson-replay-final.log`.

## 3D engine validation (2026-10-15)

All three local English Windows data sets were started sequentially in shadPS4
0.18.0. The baseline was APP_VER 0.04 (eboot hash recorded above). This is a
hardware-renderer test through the Vulkan bridge, not a TinyGL acceptance test.

- [x] Myst III: Exile: menu, New Game, intro and opening panorama now render;
  the user confirmed the corrected panorama. Clicking New Game previously
  called a null `glCopyTexImage2D` entry. The bridge now exports and loads this
  function for RGBA/RGBA8 level-zero 2D/rectangle textures, using GPU copies.
  Unsupported formats/levels fail explicitly. Myst captures RGBA before
  presentation advances the swapchain. `glDrawArrays` now accepts a nonzero
  first vertex, preserving indices and validating buffer bounds; the old
  rejection discarded five of the six panorama faces. Initial pointer trouble
  was not established as an engine bug: pointer motion worked on retest; the
  click failure was the framebuffer-copy crash. Save/load and later areas remain
  untested. Local evidence: `myst3-null-call.log`, `myst3-fix4-guest.log`,
  `myst3-fix4-skip.png`, `myst3-fix4-eboot.sha256` under
  `artifacts/upstream-2026.4.0/`. The tested binary includes temporary pointer
  diagnostics; these are removed from the source checkpoint.
- [ ] Grim Fandango: baseline intro runs with a small, misplaced viewport.
  `3d-engines.patch` makes the PS4 shader renderer use the backend's negotiated
  dimensions. Runtime confirmation is still pending at this checkpoint.
- [ ] The Longest Journey: baseline fails compiling `stark_actor.vertex`.
  PS4 shader variants use mat4 normal transforms and separate light-field arrays;
  helper parameters avoid shadowing the color uniform. The bridge accepts
  bounded arrays up to 128 entries for Stark's 70-bone skeleton, with the existing
  8 KiB stage budget retained. All six adapted shader pairs pass host compilation,
  linking and reflection. Runtime confirmation remains pending.

Bridge state/copy/texture-cache tests pass with ASan/UBSan, including nonzero
vertex offsets, out-of-range rejection, GPU-copy allocation and invalid-call
storage preservation. GLSL regression tests pass, including a 70-element named
array and rejection above 128. Evidence: `3d-copy-tests.log`, `3d-glsl-host.log`,
`stark-shader-check.log`. Clean-source replay matches 104 modified files.
The experiment configure script rebuilds changed bridge objects and declares
link dependencies, avoiding stale executables when only the bridge changes.
Physical PS4 testing remains separate. Commercial data is neither packaged nor
committed. The previous 0.04 release package is preserved.

### Grim follow-up: viewport and frozen frames

The bounded PS4 color-FBO route presents Grim at its original 4:3 aspect ratio.
The first gameplay frame previously failed with `VK_ERROR_FEATURE_NOT_PRESENT`: the
native fetch-shader path rejects nonzero `firstVertex`. PS4GL now lowers those
array draws to explicit indices while retaining the original vertex IDs.
The frame-owner/PS4GL ASan and UBSan checks pass, including offset triangle indices.
A shadPS4 run reaches Manny's office without the presentation failure
(`artifacts/upstream-2026.4.0/grim-fix3.log`, `grim-fix3-scene.png`).
This is not full gameplay or physical PS4 validation. Offscreen depth/stencil
storage is still missing from the color-FBO path and needs follow-up.

The PS4 SDL D-pad now maps button indices 13–16 through the normal keymapper.
Hat synthesis is suppressed to avoid duplicate raw events: `pushEvent()` bypasses
the mapper, so enabling hats alone did not trigger Grim's arrow-key actions.
Verified in shadPS4: Up produces key 273 down/up and moves Manny in the office
(`grim-dpad-mapped.log`, `grim-dpad-before.png`, `grim-dpad-after.png` under
`artifacts/upstream-2026.4.0`). The test executable includes temporary engine
input tracing; exported source patches omit that tracing.

### The Longest Journey: missing untextured actor parts

The opening dialogue scene exposed a PS4GL descriptor restriction: the actor
shader has a texture sampler even when `textured` is false, and the engine binds
texture zero for solid-color materials. The bridge rejected the entire draw,
omitting arms, hands and legs. The 2D sampler path now supplies a cached opaque
black 1x1 descriptor for an unbound texture, allowing the inactive sampling branch
to retain the material color. Bound incomplete textures and non-2D targets remain
subject to the existing validation. No skeletal-animation workaround was needed.

The frame-owner/PS4GL ASan and UBSan suite passes, including fallback pixel content,
100-frame cache retention and teardown. Native build passes. A controlled shadPS4
start renders complete actors in the opening cabin scene (`tlj-after-fallback.png`
and `tlj-fallback-probe.log` under `artifacts/upstream-2026.4.0`); the prior log
`tlj-actor-probe2.log` records rejected program-5 draws with texture zero.
Temporary direct-start changes were reverted before producing the normal candidate
(`tlj-fallback-clean-build.log`). The running diagnostic still uses that direct
start, but the staged eboot uses normal menus. Input, later scenes, save/load and
physical PS4 behavior remain unverified. Intro playback was slow in this test.

### Additional local-game visual checks (2026-10-15)

Seven more titles were launched individually with the normal `7d933fb` candidate
in shadPS4 0.18.0. Each used a separate emulator profile and empty save directory;
existing saves and game data were not modified. The executable SHA-256 was
`aa47efd6c619e40c29e5fda1da8c253f604efb8518a34c36c6099358cd8df8f0`.
The package metadata still says 0.04: this is a staged executable test, not a new
published package. All tests use the Vulkan presentation backend; 2D engines
render their game image in software before GPU presentation.

| Game | Observed result | Limit or blocker |
| --- | --- | --- |
| Day of the Tentacle | Animated LucasArts intro, correct palette | Gameplay not reached; observed progression was slow |
| Thimbleweed Park | Game detected; error dialog before menu | New engine requires `OpenGLContext.framebufferObjectSupported`, which the bounded PS4 bridge does not advertise |
| Broken Sword 2.5 | Loading screen, title menu, moving pointer, quit confirmation | Delayed input; gameplay scene not verified |
| Buried in Time | Studio logos, animated title and credits | Gameplay not reached |
| Prisoner of Ice | Infogrames logo and credits movie | Skip input and first room not confirmed |
| Shadow of the Comet | Illustrated intro advances with readable dialogue | Gameplay not reached |
| Penumbra: Overture | Intro text and gamma/settings screen | With `show_intro=false`, `show_pre_menu=false`, `show_menu=false`, scene startup crashes at `NewtonCreateBox` |

Penumbra's crash reports guest PC `0x1dc02cd`; subtracting the guest load bias
`0x400000` gives ELF address `0x19c02cd`, resolved against this build to
`NewtonCreateBox`. This locates the fault, not its underlying cause.

Local evidence is under `artifacts/upstream-2026.4.0/extra-smoke/`: `results.json`,
per-game logs and PNGs, plus copied INI/bootstrap logs. Representative captures:
`dott-scene.png`, `twp-start.png`, `sword25-menu.png`, `buried-progress.png`,
`pice-progress.png`, `sotc-progress.png`, and `penumbra-menu.png`.
`penumbra-scene.log` records the crash. Intro captures are not gameplay acceptance;
audio, save/load, later areas, performance and physical PS4 remain unverified.
No engine fixes or new package release were made during this test round.

### FTP-ready hardware candidate 0.05 (2026-10-15)

`dist/ps4-ftp-2026.4.0-005/ScummVM-2026.4.0-Vulkan-Test.pkg` uses
SCVM00265 / APP_VER 0.05. Package SHA-256:
`c57282afcdbdf0f49a1afc87c6dfc8a31c0ab661545f717cd890ee109ecc9ba2`.
Eboot SHA-256: `418ea5ba0377fabb91ff1dd9e91e20feef154e7f7bcabf59a716b3f58e09bcfc`.
Startup now creates the shared `/data/scummvm/games` directory actually exposed
by the storage browser. Internal storage precedes USB 0–7 in the root list;
registered game paths remain unchanged (no automatic path migration).
Settings/saves remain in `/data/scummvm-2026-vulkan`.

Build and host bridge regression tests passed. PkgTool validation passed,
and extracted payload hashes match the staging manifest. Evidence:
`artifacts/upstream-2026.4.0/ftp-005-{build,package,tests,validate,extract}.log`
and `ftp-005-payload.json`. The distribution folder includes Swedish FTP
instructions and a checksum file. No commercial games are bundled.
FTP transfer, installation and physical-console acceptance remain pending.
The Thimbleweed Park and Penumbra blockers above remain unresolved.

### Gabriel Knight II / The Beast Within (2026-10-16)

All six English DOS CD images extracted locally. Test game directory:
`artifacts/Gabriel Knight II/game` (local symlinks into extracted discs).
Each disc's RESOURCE.AUD/SFX is exposed as RESAUD.00N/RESSFX.00N for SCI's
multi-disc audio handling; original disc files remain unchanged.

Tested the 0.05 FTP candidate in a visible shadPS4 window with a separate
`emulator-gk2` profile. Detection matches all six resource volumes.
Sierra logo, title transition and Chapter One / Continue screen were observed;
the user then confirmed that the game runs. Full playthrough, later disc
content, save/load and physical PS4 remain unverified. Evidence and extraction
helpers: `artifacts/upstream-2026.4.0/gk2/`; final run log `shad-complete.log`,
Chapter One screenshot `after-skip.png`. The earlier incomplete-data run
`shad.log` is superseded by this complete six-disc setup.

### Release gate agreed 2026-10-16

The user requires the Thimbleweed Park framebuffer-object (FBO) blocker to
be resolved before release. Version 0.05 remains a hardware-test candidate,
not a release-approved build. Advertising FBO support alone is insufficient:
the required framebuffer behavior must work and Thimbleweed Park must be
retested. The separate Penumbra startup crash remains tracked above.

### Physical-console FTP delivery (2026-10-16)

0.05 delivered to `/data/pkg/ScummVM-2026.4.0-0.05.pkg`. All 15 inventoried
local games copied to `/data/scummvm/games`, with a new 15-game configuration
in `/data/scummvm-2026-vulkan/scummvm.ini`. Existing older-port settings/saves
were not replaced. 29231 files (16.56 GiB)
were fully read back over FTP and SHA-256 matched. Evidence is local in
`artifacts/ps4-ftp-2026-10-16/delivery.json` and `verified.jsonl`.
Installation and physical-console gameplay are still pending. FBO support
remains a required release gate; this delivery does not approve a release.

### Thimbleweed Park color FBO fix / 0.06 (2026-10-16)

The bridge now advertises `GL_PS4_color_framebuffer` for its existing RGBA
2D texture attachment implementation. TWP accepts this capability on PS4
because its render targets do not require depth/stencil renderbuffers.
Full `GL_ARB_framebuffer_object` support remains deliberately unadvertised;
other engines retain their existing capability checks. The host regression
checks both extension exposure and the absence of the full ARB claim.

The new build passes the previous startup gate and renders the animated
tutorial with background, actors and UI in shadPS4. The user confirmed it
works and authorized commit/push plus FTP delivery. Evidence:
`artifacts/upstream-2026.4.0/twp-fbo-start.png`, `twp-fbo-run.log`,
`twp-fbo-tests.log` and `twp-fbo-replay.log`. Longer gameplay, save/load
and physical PS4 acceptance remain pending; the release gate is not yet
fully closed by emulator evidence. Penumbra is unchanged.

0.06 package: `dist/ps4-ftp-2026.4.0-006/ScummVM-2026.4.0-Vulkan-Test.pkg`.
SHA-256: `d570a1c2efb799de9e53ad184ca31bbd1737f83b5f6e7dd50b0bd80286eac2a2`.
PkgTool validation passed and all 239 extracted payloads match staging.

### shadPS4 direct-mouse test (2026-10-16)

The user reported incorrect mouse direction after leaving/re-entering the
emulator window. Increasing mouse-to-stick sensitivity did not help and was
reverted. In a separate profile with `Input.use_mice_as_mice=true` and the
local test stage's `scummvm-controller-only.txt` marker removed, the user
confirmed normal mouse behavior without F7 and broken movement with F7.
F7 is shadPS4's mouse-to-joystick toggle, not a generic focus command.

For this direct-mouse profile, remove the F7 mouse-to-joystick hotkey and the
mouse-to-stick/leftbutton-to-cross/rightbutton-to-circle bindings. Restart
the emulator to clear any previously enabled emulation mode. Local launcher:
`bash artifacts/upstream-2026.4.0/run-twp-directmouse.sh`. Profile:
`.tools/upstream-2026.4.0/emulator-twp-directmouse`. This is an emulator-only
configuration test; the delivered 0.06 controller-only PKG is unchanged.
Evidence: `artifacts/upstream-2026.4.0/twp-directmouse.log`,
`twp-directmouse-start.png`, and `twp-directmouse-final.log`.

### Native mouse package 0.07 (2026-10-16)

The user confirmed the separate direct-mouse shadPS4 run works after removing
the F7 mouse-to-stick toggle. Package 0.07 now enables the existing native
`sceMouse` input path by omitting `scummvm-controller-only.txt`, while retaining
controller input. `SCUMMVM_CONTROLLER_ONLY=1` remains an explicit opt-out.
No new engine or graphics binary changes were needed after 0.06.

For an existing isolated emulator profile, run:

```sh
python3 experiments/scummvm-2026.4.0/configure-shadps4-mouse.py /path/to/data/shadPS4
```

The helper backs up the original configuration, enables native mice, removes
mouse-to-stick toggling and duplicate mouse-button mappings, and retains
controller/keyboard bindings. Restart shadPS4 afterwards. These host settings
are not embedded in the PS4 PKG. Native USB mouse behavior on physical PS4
still requires verification; the user's acceptance was in shadPS4.

0.07 package: `dist/ps4-ftp-2026.4.0-007/ScummVM-2026.4.0-Vulkan-Test.pkg`.
SHA-256: `e8d0b2c1933763e213cb91a9db39cd787072906812faa43a7a0b1f27416f4121`.
PkgTool validation passed; all 238 extracted payloads match staging and the
controller-only marker is absent. Eboot matches delivered 0.06:
`1fab07543608059d48ace41ce031c5ff4d0f71ba9465c2dda3c7d3883f1a2ae1`.

### Separate launcher/game stick settings and TWP analog input (0.08 candidate)

TWP's default left-stick bindings generated arrow-key events; its engine
then moved the cursor a fixed four pixels per rendered frame, bypassing
`kbdmouse_speed` and analog deadzone/precision. On PS4 those four default
left-stick bindings are now left to the backend virtual mouse. Keyboard
arrows, D-pad verb selection, mouse input and other controller actions remain.
Custom user key mappings are preserved.

The existing separate configuration domains are now labelled `Menu stick`
(global launcher/default) and `Game stick` (current game's override).
OK/Apply continues to flush these settings to the INI immediately. The
virtual mouse recalculates velocity from current settings on each update,
so a changed setting/domain also applies without another axis report.

Validation: shadPS4 game setting changed via Control tab + OK from raw 0
(display 3) to raw 7 (display 20); the global raw 7 remained unchanged and
the game raw 7 was read back from the INI. A 0.5-second stick input at the
low setting moved approximately 250 screenshot pixels; a 0.1-second input
at high moved about 330 pixels. After restarting, the same high setting
remained effective (about 160 pixels in the half-sized test window).
Evidence: `artifacts/upstream-2026.4.0/twp-stick-*`. Clean patch replay
matched all 106 modified files. Physical PS4 sensitivity acceptance pending.

0.08 package: `dist/ps4-ftp-2026.4.0-008/ScummVM-2026.4.0-Vulkan-Test.pkg`.
SHA-256: `e8008b453fafe67bb9a379203fb012b9ee2ea58f01ee9138826b4d2dcd91e921`.
PkgTool validation and extracted payload verification passed. Delivered to
`/data/pkg/ScummVM-2026.4.0-0.08.pkg` on the PS4 FTP server; full FTP readback
matched the local SHA-256. Delivery does not establish installation or physical
controller acceptance. Evidence: `artifacts/upstream-2026.4.0/ftp-008-delivery.json`.

### Grim actor depth correction (0.09 candidate)

The physical-PS4 report showed missing/overlapping surfaces on Manny's suit.
The backend Renderer3D target previously had only color storage, and PS4GL
explicitly disabled depth and stencil on all offscreen targets. This let
actor surfaces overwrite each other in draw order.

Renderer3D now opts into private D32F/S8 storage for its color target. PS4GL
retains depth/stencil across pass switches, honors clear/write masks and
uses depth-enabled pipeline variants for these targets. Storage is retired
with the color target and charged to the target budget. Ordinary color-only
FBOs (including TWP's effects) retain their existing behavior. This does not
advertise general OpenGL renderbuffer or ARB framebuffer support.

Rebuild every PS4GLContext consumer together, including SDL, frame/display
and main: mixing old SDL object layouts with the new bridge caused the first
local test's black screen. The configure wrapper now rebuilds these consumers.

Host bridge ASan/UBSan regression passed, including opted-in depth/stencil
queries, clears and write-mask checks. Patch replay matched 106 modified
files. In shadPS4, the intro, office scene and walking/camera change worked;
Manny's suit and arms appeared solid. The user accepted the visible test.
Evidence: `grim-depth-run2.log`, `grim-depth-game2.png`, `grim-depth-walk.png`,
`grim-depth-tests.log`, `grim-depth-replay.log` under
`artifacts/upstream-2026.4.0`. Physical PS4 acceptance remains pending.

0.09 package: `dist/ps4-ftp-2026.4.0-009/ScummVM-2026.4.0-Vulkan-Test.pkg`.
SHA-256: `db9b4c0c20fac288d386da247ebe00f338ff7c99eb53501c0a4b9644c85f9c93`.
Eboot SHA-256: `8ec3002ff113fd3cab91d06726318ef498cac59ac2a7713caba39e01029a94f2`.
PkgTool validation passed and all 238 extracted payloads match staging.
This includes the 0.08 menu/game stick sensitivity changes. The final build
also moves the initial depth-image barrier after fallible allocation work.
Delivered to PS4 FTP `/data/pkg/ScummVM-2026.4.0-0.09.pkg`; full readback
matched the package SHA-256. Evidence: `ftp-009-delivery.json` in the same
artifact directory. Installation and physical rendering acceptance pending.

### Return to launcher and sensitivity minimum (0.10 candidate)

The user confirmed 0.09 works on PS4, then requested launcher return for PICE,
SOTC and Jonssonligan and a lower minimum stick sensitivity.

PICE and SOTC now advertise return-to-launcher support; their existing
`shouldQuit()` loops also cover the event manager's return flag. On PS4 SOTC
leaves Options/JOY_START to ScummVM's global menu instead of binding it to
its own Escape menu; touchpad/JOY_BACK still opens the game's menu.
For the opt-in PS4 Jonssonligan 1 target, Director now enables the launcher button and
honors the return flag in the engine, window and Lingo execution loops.
Other Director titles keep the previous feature gate.

PS4 sensitivity's lowest existing INI value (raw 0) now means displayed 1
and speed factor 0.1, instead of displayed 3/factor 0.25. Other saved values,
10 as normal, and separate global/game domains are unchanged.

Build passed; clean replay matched 108 modified files. shadPS4 exercised
Jonssonligan -> launcher -> PICE, PICE return during its intro and restart,
and SOTC Options -> launcher -> Jonssonligan -> launcher -> Jonssonligan.
Evidence is in `artifacts/upstream-2026.4.0/rtl-*`. This is startup/return
coverage, not a full gameplay or physical-PS4 acceptance test.

The repeated Director start initially exposed a stale `g_lingo` pointer:
`Window::processSysEvent` used it during new stage initialization before the
new current window existed. The destructor now clears `g_lingo` after its
window cleanup, while retaining `g_director` for subsequent Archive member
destruction. The corrected run (`rtl-run4.log`, `rtl4-launcher.png`,
`rtl4-restarted2.png`) returned to the launcher and started the intro again
in the same process. Replay after this correction still matched 108 files.

0.10 package: `dist/ps4-ftp-2026.4.0-010/ScummVM-2026.4.0-Vulkan-Test.pkg`.
SHA-256: `7a4cf55a997fcb5d0a1b6b7c509a1af448bf565e731886ac2b90562e7d654722`.
Eboot SHA-256: `682b1a2511bf91aa4875e4c2f2a70b559a88ace2fbd85bd9f45d7302965036a4`.
PkgTool validation passed and all 238 extracted payloads match staging.
Delivered to `/data/pkg/ScummVM-2026.4.0-0.10.pkg` on the PS4 FTP server;
full readback matched SHA-256 (`ftp-010-delivery.json`). Physical-PS4
acceptance of these launcher/input changes remains pending.

### 2026-10-17: Jonssonligan 2 local shadPS4 investigation

The opt-in `ps4_director7_experimental` compatibility path now covers both
`jonssonligan1` and `jonssonligan2`. Preferences use each game's ID as their
prefix; existing Jonssonligan 1 names remain compatible.

The TMS Tools virtual drive list now includes its terminating CR. The sequel
removes the final empty line before scanning drives; without that terminator
it removed `D` and reported a missing CD. The game's `Data/inventory.cst`
probe now resolves to the existing local data.

Digital-video cast loading initializes both scale percentages to 100. Missing
initialization produced a 320x24459 destination for a 320x240 AVI frame.
Lingo blend 0 now retains the blending flag for full transparency; disabling
that flag made the menu's black fade layer opaque. shadPS4 run 13 showed the
menu after these corrections. Run 9 showed the corrected intro; a separate
Jonssonligan 1 smoke run reached its intro with the same shared fixes.

The subsequent RGB/shape candidate preserves raw D7 RGB components, updates
the correct copyback flags for D7 fields, applies blending to drawn shapes,
and copies shape foreground/background colors when duplicating cast members.
The user confirmed that the pointer and nameplate/highlight now look correct.
Run 14 was the graphics candidate; its build and clean replay matched 112
modified files. Full gameplay coverage remains pending.

Local evidence and launcher: `artifacts/upstream-2026.4.0/jonsson2-*`,
`jonsson1-regression.*`, and `run-jonsson2-test.sh`. The local test stage is
`build/scummvm-2026.4.0-jonsson2-test-pkg` (still labelled 0.10); it is not a
new release PKG and has not been sent to the PS4. No physical-console or
complete-game acceptance is implied. Director 7 film loops and some Lingo
operations still produce unsupported-operation warnings.

The new-game investigation adds bytecode `swap` (0x21) and `setContents`,
including assignment to text-bearing Xtra cast members. The sequel uses
these to clear its player-name field. Previously the authored test text
remained in that field. Preferences are isolated by game ID.

Run 20 started the new-game intro five times and returned with Escape four
times in the same process. The reported persistent black lock was not
reproduced in this controlled sequence. C maps to Escape in the emulator;
the game's AVSLUTA_QT handler explicitly returns to the main menu on Escape,
so this is cancellation rather than a skip directly into gameplay.
Build 22 removes temporary flow diagnostics. Clean patch replay matched
113 modified files (`jonsson2-flow22-replay.log`). The corrected empty-name entry was verified in run 23, including entering
a new name with the touchpad virtual keyboard and starting the intro.

Letting the intro finish in run 20 exposed a separate fatal parser error:
`readSpriteDataD7(): Miscomputed field position: 48` while loading `game.dir`.
The channel splitter used an 8-bit sprite index despite D7 supporting more
than 256 channels. Run 23 widens that index to 16 bits. The regression probe
`python3 tests/director7/check-channel-split.py <prepared-source>` compiles
the actual splitter against bounded stream callbacks and checks 12,000 delta
starts plus a header crossing. It passes with the fix and fails its boundary
assertion against the original upstream splitter. Build 23 and clean replay
passed; the latter matched 113 modified files (`jonsson2-flow23-replay.log`).

Run 23 passed the channel-index failure and then exposed a delta beginning
at offset 34 (the low half of a skew field). Build 24 preserves untouched
bytes when applying partial 32-bit sprite-list/rotation/skew updates and
accepts partial padding deltas. `check-partial-u32.py` exercises all 20
byte-order/offset/length combinations against the actual helper. Both parser
probes pass on clean replay (113 files, `jonsson2-flow24-replay.log`).
A direct `Data/game.dir` load in run 24 passes parsing but shows a behavior
error at frame 1; this probe bypasses normal menu initialization. The normal
startup route is restored for run 25.


The normal new-game route in run 25 reproduced the behavior failure seen in
run 24. Behaviors now receive their owning `spriteNum` before `beginSprite`;
a declared VOID property previously shadowed the dynamic fallback. Further
local probes add D7 indexed property/chunk access, `environment.runMode`,
chunk-variable references (0x6d/0xad), `keyPressed`, chunk `delete`, and the
sprite behavior-list getter. Run 38 reached and drew the first game room;
the user confirmed room graphics but reported slow updates and missing
players. This is **not yet playable or accepted**. Run 39 is a normal-menu
regression candidate; the quick Escape sequence left only the menu logo,
so run 40 repeats startup without those early inputs.

An initial KePath Xtra implementation provides grid A* for the game's flat
walkable-cell lists. Its host probe (`tests/director7/check-kepath.cpp`)
checks obstacle routing, unreachable/missing endpoints and diagonal steps.
Original Path/Path2 corner rules and actual character movement remain
unverified. Temporary local path diagnostics remain in this candidate.
The channel splitter probe now covers 24,000 starts across 1,000 channels
(the game uses channels above 800). Both parser probes and the pathfinding
probe passed; clean patch replay for build 39 matched 123 modified files.
The emulator configuration has been restored to normal projector startup.


Run 40 completed the normal projector/menu/new-name/intro route and reached
the room, reproducing missing players independently of the direct-load probe.
Run 42 fixes property-list `.count` lookup: the missing count had skipped the
three-player update loop. All three actors then appeared. The user reports
that control works reasonably, albeit slowly, and confirms the colored marker
follows the selected character. Treat it as selection feedback, not an alpha
bug. Full-game and physical-PS4 validation remain pending.

Build 43 removes temporary actor/channel dumps, complete KePath map/path dumps,
and the expanded assignment diagnostic. Normal warnings are retained; repeated
sound-channel-zero and string warnings remain visible and have not been
explained away as harmless. This is a diagnostic-cleanup comparison, not a
verified performance improvement. Run 43 uses the same direct `Data/game.dir`
probe as run 42 so the user can compare the same room without replaying the
intro. The normal projector path is `toInstall/Jönssonligan går på djupet.exe`.
The candidate remains local and uncommitted; no release package/FTP delivery.


Run 43 exposed disappearing player cards on repeated hover/selection. Its log
contains `c_delete: bad chunk ref field type: STRING` followed by missing
`*_crawl_1_r_r` graphic names. Generic method dispatch eagerly replaced a
variable reference with its string value before `getPropRef` built the chunk
reference, so mouseLeave could not remove the hover suffix. Build 44 preserves
the argument reference for D7 getPropRef/setContents/delete while evaluating
a separate value for method dispatch. The local test is restarted as run 44;
repeated card hover/selection acceptance remains pending.


Run 44 still lost cards. Run 45's targeted deletion diagnostics confirmed
correct suffix removal on multiple mouseLeave calls, but duplicate `_r_r`
names still appeared later; reference preservation alone is insufficient.
Build/run 46 publishes the new hovered channel before dispatching mouseLeave,
so nested event processing cannot observe the previous hover target and
repeat the transition. This reentrancy fix is a candidate, not yet user
accepted. Temporary deletion diagnostics are removed. The visible emulator
has been restarted for repeated card-selection testing.


The user reports run 46 improves card stability, but selection does not bring
its card to the front. Inspection of the local card behavior shows its
exitFrame assigns locZ 2,000,000 to the selected card and 1,000,000 to other
cards; mouseEnter itself changes only the graphic suffix. Those sprite
exitFrame handlers were not dispatched. Build 47 broadcasts enterFrame and
exitFrame to attached behaviors for both opt-in PS4 Jonsson games, then sends
each frame/movie notification once. This extends the existing prepareFrame
broadcast. Visible run 47 is the validation candidate; first-game regression
and physical hardware validation are still pending.


Run 47's direct-scene probe hides the cards once their exitFrame runs. The
projector's startup script explicitly initializes gPolaroid=1 and gBubblor=1;
direct Data/game.dir loading bypasses both (and audio settings). This invalidates
the direct probe for card visibility and speech-bubble acceptance. The profile
is restored to normal projector startup and run 48 starts from the beginning.
Do not bypass the projector when testing UI/audio settings. Card depth and
speech-bubble rendering still require confirmation via the normal route.


Runs 48/49 did not establish a startup-only explanation: the user still
reported missing cards, speech bubbles and floor clicks after normal startup.
Run 50 rolled back the enterFrame/exitFrame broadcast for comparison; a
direct-scene diagnostic confirmed the three card behavior parameters are
correct (997=sickan, 998=vanis, 999=harry). Build/run 51 restores only sprite
exitFrame delivery and logs the card globals for the first twelve updates.
It starts via the normal projector; enterFrame remains on its prior path.
These bounded J2CARD/J2CARDUPDATE diagnostics are temporary and must be
removed before preparing a release. The maintained patch still reflects
build 47; export/replay is pending the investigation result.

The user's original-game reference and the local card behavior agree: the
selected card belongs in front, and clicking it again cycles crawl, Run,
sneak, walk. The card exitFrame updates both movement artwork and depth.
Hover alone changes the artwork suffix, not depth. Stale crawl artwork is
therefore not evidence that every character is actually crawling. Run 51
acceptance for card cycling, selection depth, floor clicks and speech bubbles
is pending. No new commit, package or FTP delivery has been made.


Runs 51-57: the normal startup log confirms gPolaroid=1 and gBubblor=1,
so initialization does not explain the continuing missing UI. Card exitFrame
changed sickan_crawl_1 to sickan_walk_ (losing the suffix), then repeatedly
requested missing artwork. Probes 52-54 narrowed this to UTF-32 substring
construction: the suffix _1 had length 1 despite the source retaining both
characters. Disassembly of common/str-base.o showed Clang replacing the
char32_t cStrLen loop with wcslen; the target runtime behaves as a 16-bit scan.
The PS4-only scan now uses volatile element-width reads to prevent that
transformation. This shared Common fix is carried by vulkan.patch and is
not restricted to a particular game. No runtime library override is used.

Run 55 preserves sickan_walk_1, vanis_walk_0 and harry_walk_0. Run 56 confirms
valid card cast IDs, visible=1, on-screen rectangles and selected priority
2,000,000 versus 1,000,000. The user saw cards appear after a delay but reports
remaining interaction problems. The regression test
`python3 tests/director7/check-ps4-string-length.py SOURCE` runs the actual
scan against a simulated 16-bit wcslen: it fails on clean upstream and passes
on the patched source. The PS4 object no longer references wcslen. Existing
channel-split and partial-uint32 regression checks also pass.

Build 57 removes all temporary card/string diagnostics and diagnostic-only
direct-scene globals, restores enterFrame plus exitFrame behavior delivery,
and starts via the normal projector. Build/link passed. Card interaction,
floor clicks and speech bubbles remain pending user verification; this is
not a claim of full gameplay acceptance. No commit/package/FTP was performed.


Run 58 confirms the remaining interaction failure is the authored cutscene
lock: mouse down/up handlers execute with pSparr=1 and gDialog=VOID. Cards
reappear/highlight but neither selection nor walking is allowed. This must
be fixed by completing the dialogue, not forcing the lock to zero.

The first linked AIFF contains twelve MARK entries (11 dialogue positions
and a final slut marker). Linked sound loading did not import these marks;
existing cue dispatch also omitted the channel/number/name arguments and
was not routed to the movie handler. Candidate 60 imports linked AIFF/AIFF-C
MARK chunks, converts sample-frame positions to milliseconds using COMM,
and dispatches cuePassed(channel, oneBasedNumber, name). It also preserves
cupt metadata after embedded ediM audio and allows text chunk reads from
text-bearing cast Xtras. The cue cursor resets with new playback and advances
before callbacks, which may replace sounds. Candidate 61 additionally drains
pending cues after natural completion; explicit stop clears lastPlayedSound
and does not synthesize remaining dialogue cues. These candidates retain
bounded J2INPUT/J2CUES diagnostics plus J2CUE dispatch logging for validation.

The actual AIFF reader passes nine synthetic cases for marker order, Pascal
string padding, sample timing, AIFF-C, and truncated/invalid chunks using
`check-aiff-cues.py`. A read-only local audit found 620 AIFF files, 462 with
markers, and 443 with a slut marker. This establishes broad data relevance,
not gameplay acceptance. Run 60 is the visible normal-startup candidate;
61 is built separately pending whether the terminal cue is missed.


Run 60 still delivered no cues and remained locked. Disassembly identifies
`sound playFile` as the actual dialogue playback route. Build 62 adds the
same AIFF reader to playFile, keeping times/names on SoundChannel rather
than requiring a cast member. Build 63 corrects cue lifetime: reset only on
actual stream replacement or explicit stop, not on setLastPlayedSound, which
is also used by a zero score-sound update while playFile continues.

Visible run 63 uses normal projector startup. Build/link and the nine AIFF
reader cases pass. Maintained patches now include this candidate, with
J2INPUT/J2CUE diagnostic logging still present. User acceptance for dialogue
text, final cue unlocking floor/card input, and later conversations is
PENDING. No hard-coded release of pSparr or fabricated dialogue events was
introduced. Earlier run 57's clean replay covered 124 files; the larger
current cue changes require a new replay before commit/release.


Run 67 isolates two further cue-delivery blockers: a playFile channel uses
SoundID with cast member zero, and same-frame Lingo jumps skipped cue polling.
File cues now remain selected for the zero-member case; opt-in Jönsson cue
polling runs outside the no-jump branch, with movie-change/stopped/frozen
guards. All twelve first-scene markers reach movie script 23/cast 3. The
authored final `slut` handler changes pSparr and gDialog from 1 to 0.

Normal projector run 68 confirms the same terminal unlock and visible speech
bubbles (also confirmed by the user). Direct-scene startup bypassed bubble
preferences and was not a valid visibility test. The remaining reported issue
is text/bubble sizing: Text Xtra retained its authored 127x12 single-line
rectangle when replacing a longer line. Candidate 69 measures wrapped text
with the rendering font before returning member.height, allowing the game's
own bubble script to size its graphics. This is restricted to the opt-in
Jönsson games. Authored Paige font/style decoding is still incomplete.

Build/link 69 passes; the AIFF parser's nine synthetic cases, file/cast cue
lifecycle tests (including callback mutation), and PS4 string-length regression
pass. Run 69 uses normal projector startup. Long-reply visual fit, later
dialogues, and physical PS4 verification remain pending. Temporary J2 cue/input
diagnostics are retained; no commit, package or FTP delivery was requested.


The user accepted bubble sizing in run 69, then found another disappearing
card before publication. The first missing-member diagnostic is
`sickan_walk_0_r_r`: the hover suffix is applied twice, after which card
name updates operate on an invalid member. Publication/delivery was held.
Candidate 70 queues the complete mouseLeave/mouseEnter transition through
the input queue for both opt-in Jönsson titles, deferring callbacks while a
script or suspended input handler is active. This avoids direct reentrant
Lingo execution from event polling/updateStage. A regression extracted from
the actual branch passes busy/suspended script, nested transition, duplicate
movement and 100 card-cycle cases. Build/link passes; visible run 70 is active
for repeated figure/hover testing. No claim of visual acceptance yet.

Candidate 71 is the same hover/layout implementation with temporary J2
input/cue/call-stack logging removed. Build/link, all seven Director regression
checks (including KePath), whitespace checking, and a fresh patch replay
(129 matching modified files) pass. Visible run 70 is intentionally left
running for the user's card test. FTP connectivity to the requested console
is verified, but commit/push/package/upload remain pending that test.

The user reports run 70 now works after repeated card testing. The hover fix
is accepted for packaging. Context cursor changes remain a visual test point:
script 41 renders an animated sprite (channel 248), with cursor_titta,
cursor_ta, cursor_talk, cursor_exit and other interaction states. Static
script inspection confirms intended behavior, not runtime acceptance of
every hover target. Candidate 71 removes diagnostics only.


### Post-release Jönsson pointer investigation (candidates 73–75)

The user reports jerky left-stick movement in both Jönsson games on physical
PS4, while the launcher and many other games are smooth. Script 41 in Jönsson 2
hides the system cursor and positions sprite 248 only during its Update method.
The candidate follows visible max-priority cursor_* bitmaps between score frames,
using the authored eight-pixel stage margins. This sprite convention is applied
only to Jönsson 2; Jönsson 1 does not use those member names.

Both opt-in Jönsson games also receive a shared Lingo input/presentation timer.
The original execute-local timestamp started at zero for every call, allowing
repeated presentation across short behavior calls. The candidate services input
on script entry and every 100 instructions, with one shared 16ms time threshold.
No script tempo or simulation speed is changed. These are candidate fixes, not
confirmed physical-console improvements.

The user still found keyboard/full-stick movement too sensitive in run 73. The
emulator profile had kbdmouse_speed=3. Temporary run 74 timing over successive
120 service calls measured input 1–6ms total, widgets 4–6ms total, and draw
302–909ms total; this does not establish the cause of all long update gaps.
Candidate 75 removes that timing log. Build/link, hover/cue regressions, new
actual-code input-clock and sprite-cursor tests, and patch replay (130 matching
modified files) pass. Source changes remain uncommitted. Everslop and FTP 0.11
remain untouched. No claim that sensitivity alone fixes the stutter.

Visible run 75 is a DIRECT-SCENE pointer probe with kbdmouse_speed=0 only in
the local [jonssonligan2] domain, to separate speed from stutter. This bypasses
normal startup and is not a dialogue/card acceptance run. The previous complete
normal-startup config is retained locally at
artifacts/upstream-2026.4.0/jonsson2-before-cursor73.ini. Restore it after stopping
the owned probe, before any ordinary gameplay test. Run log:
artifacts/upstream-2026.4.0/jonsson2-run75-low-speed.log. Physical PS4 confirmation
of the input candidate and testing Jönsson 1 remain outstanding.

On user request, clean candidate 75 was packaged as APP_VER 0.12 and delivered
to /data/pkg/ScummVM-2026.4.0-0.12.pkg. Validation and all 238 extracted payload
comparisons passed; full FTP readback matched SHA-256
3de5d9b3bd1bfaec6d0c778da7c3c59dc2c086398bb6b88c43c0f354c56c7f68.
The local low-speed INI and direct-scene override are NOT packaged. Existing
console settings remain in effect. Physical left-stick testing in both titles
via normal startup is pending; no commit/push or Everslop release replacement
was performed. Build identity is recorded locally in ftp-012-build-identity.json.

### PS4 Jonsson pointer candidate 0.13

Both Jonsson game IDs now apply a 0.2 multiplier to analog pointer speed outside ScummVM dialogs, retaining the saved sensitivity setting and cubic response. Other games and ScummVM dialogs retain their current speed. Physical PS4 confirmation is pending. Press the touchpad to open the virtual keyboard (SDL button 17 maps to JOY_BACK); Share is not exposed by this driver.

### Jonsson performance pass / candidate 0.14

Physical feedback supersedes the sensitivity hypothesis: both games remain slow, and the 0.13 pointer multiplier makes control too slow. The 0.2 multiplier has been removed; existing saved user settings are retained.

Hit testing previously rebuilt a quadratic insertion-sorted channel list for every query. The PS4 Jonsson path now caches an ascending (locZ, channel index) order, verifies a priority snapshot on each use, and sorts only when priorities or channel count change. Hit queries traverse it backwards; rendering uses ascending order, with editable sprites still appended in original channel order. Direct priority writers, scene replacement and overlapping-card ties remain covered. Non-Jonsson queries use their original channel order without allocating a temporary list. Lingo's Jonsson compatibility decision is evaluated once per execute call rather than once per opcode.

Instrumented shadPS4 direct-scene comparison (not physical PS4 or full-game acceptance): median hit-test time fell from 2710.8 to 43.3 microseconds/call across 21 baseline and 15 optimized sampling windows. Median main-loop steps rose from 1.30 to 11.79/s; this is not a rendered-FPS counter. The direct-scene test skips required startup state and later exited on a missing Lingo value; it is useful only for the performance comparison. A normal-start test was subsequently launched separately.

Validation: extracted production query methods matched the original ordering in 800 randomized scenes with three mutation passes each, including ties, locZ changes, growing/shrinking channel lists, transparent holes, editable layers, and non-Jonsson behavior; ASan/UBSan enabled. Eight existing Director regressions passed. Temporary aggregate timers were removed from the package build. See `artifacts/upstream-2026.4.0/perf-pass1/` for local measurements and logs. Physical PS4 confirmation of 0.14 remains pending.

### Candidate 0.15: script cursor origin

The J2 script-cursor follower incorrectly clamped window-local mouse coordinates against the movie rectangle including its saved origin. The local game.dir DRCF rectangle has left=192, top=144 and size 640x480. This left logical hover/click input operational beyond an artificial visual cursor boundary. Clamp to local width/height instead, retaining the existing eight-pixel margin. Regression cases now include positive and negative movie origins. The performance improvements and original pointer speed are retained.

### Interaction candidate: invisible quadrilateral hotspots

J2 assigns `sprite.quad` to transparent `Ruta` bitmap sprites for interactive objects. This property was previously ignored, leaving the original one-pixel sprite dimensions for hit testing. The PS4 Jonsson path now stores four-point hit geometry separately from rendering, translates it with sprite position, and uses polygon containment for pointer hits. Releasing a puppet clears its geometry. This is limited hotspot support, not general textured-quad rendering.

The clean Vulkan build and extracted-production ASan/UBSan geometry regression pass (bounds, translation, winding, visibility, member restriction, reset and degenerate input). Normal-start emulator tracing confirms that the game assigns the hotspot polygons and reaches the room. Cursor feedback and actual object interaction still require runtime confirmation; no new package has been delivered for this candidate.

### Interaction follow-up (2026-10-17)

User emulator testing confirms Harry's glowing interaction cursor and opening the safe after adding custom sprite behavior property access, setProp expression return handling, and named property indexing. Leaving the room then stopped in Lingo: `b_setAt` rejected a SYMBOL index, followed by `getAt` on VOID. The next test candidate adds named PARRAY writes to match existing named reads. This candidate builds; room transition remains unverified. Local logs: `perf-pass1/property-index-run.log` and `named-setat-run.log`. Temporary PROPTRACE diagnostics remain in the test binary/source and must be removed before packaging. No hardware package or release acceptance is implied.

### Jönsson save/load, room transitions and NPC dialogue (0.16 candidate)

This supersedes the pending-test notes above. User testing in shadPS4 confirms
save loading with restored doors, complete courtyard and street backgrounds,
object interaction, and conversation with Doris. Physical PS4 acceptance of
these changes is still pending; other NPC conversations are not all verified.

The Director compatibility delta includes D7 bytecode variable addressing,
chunk-property and named-list access, CP1252 FileIO decoding, save overwrite
handling and save-folder enumeration. Released Escape no longer redirects a
loaded game to the menu. Singular text-count properties let scene construction
finish. CuePassed can probe the three-player list for an NPC without aborting
before the original room-NPC fallback; this exception is limited to that handler
and list shape.

Large-text UTF-8 decoding and line/item offsets use bounded, content-keyed
caches. Both contents and delimiter changes invalidate offset reuse. In separate
emulator runs the slot-6 menu parse fell from 1256 ms to 244 ms after indexing;
these are individual observations, not physical-console benchmarks or total
load times. Existing channel-order and shared input-clock improvements remain.
The additional archive-buffer experiment is excluded from this package.

Temporary save, player, background and event traces and forced script dumps
have been removed. No game scripts, save files or private game assets are
included. The patch replay matches 132 modified files against a separately
prepared upstream tree. Local build/test/package evidence is under
`artifacts/upstream-2026.4.0/perf-pass1/pkg016-*`.

0.16 validation: all 15 Director Python regressions passed, Vulkan build/link
passed, PKG validation passed and all 238 extracted payloads match.
Package: `dist/ps4-ftp-2026.4.0-016/ScummVM-2026.4.0-Vulkan-Test.pkg`.
SHA-256: `e7e132fcd58238647f02df4ac77c7cd96b37857ea515c21f893865bd85688c7a`.
Physical-console installation and gameplay verification remain pending.

### 0.17: embedded Jönsson dialogue font

The user confirmed 0.16 on physical PS4 and accepted the new dialogue rendering
in shadPS4. Sand is now decoded directly from the game's embedded PFR1 XMED
resource, without external font files or player-side conversion. Speech uses
black antialiased text on white, ordinary word spacing and measured wrapping.
Physical-console verification of this font change remains pending.

The LibreShockwave PFR1 subset is AGPL-3.0; its license and notice are included
in the package. Corresponding changes and provenance are in director7.patch.
No proprietary font or game data is distributed. See font-tools/README.md.

Validation: all 15 existing Director Python regressions and the new PFR1
ASan/UBSan conversion/glyph test pass. Patch replay matches 143 modified files.
PKG validation and extracted-payload comparison pass.
Package SHA-256: `d09e993e0f71bcda8a18f63e90c812783c1401e4f5b3e39e90706ec1d680ddc7`.

### Penumbra startup investigation (2026-10-18, after 0.17)

Package 0.17 reproduces the hardware-reported startup failure in shadPS4 at
NewtonCreateBox (guest 0x1dd3b3d). Its public float-array argument was cast to
an aligned dgMatrix, emitting MOVAPS for an array without that guarantee.
Seven primitive constructors now copy matrix elements without that cast.
The rebuilt binary emits MOVUPS for the external matrix and passes the former
crash site in the isolated Penumbra test. No hardware fix is claimed yet.

A second blocker appears during loading: flexible memory runs out (0x110000
available, 0x404000 requested), followed by a null output write in
Graphics::PixelFormat::colorToARGB (guest 0x79b585). Diagnostic emulator trials
with 256/1024 MiB extra flexible memory instead stop inside shadPS4 MapMemory
with 'Failed to map physical memory'. The profile's memory settings were
restored afterward. First playable scene has NOT been reached.

Logs and the downloaded hardware dump are retained locally under
artifacts/penumbra-hardware-20261018/. Intro/menu skipping is confined to this
existing isolated test profile. Next work is CPU heap/memory allocation;
the existing wrapper still forwards malloc to musl's flexible-memory heap.
No new PKG has been delivered for this experiment.

### Penumbra first room reached (follow-up)

The direct-heap experiment and further Newton API matrix fixes reach the
textured boat cabin with introductory text in shadPS4, using normal memory
limits (extra_fmem_in_mbytes=0). This supersedes the startup blocker above;
movement, interactions, later rooms and physical PS4 remain unverified.
The visible test remains running. Evidence: matrix-api-run.log and
first-room.png under artifacts/penumbra-hardware-20261018/.

Opt-in `SCUMMVM_LARGE_DIRECT_HEAP=1` when configuring Vulkan routes malloc/calloc
buffers >=256 KiB to CPU write-back Onion direct memory, with libc fallback.
A fixed registry tracks ownership for free/realloc; failed growth preserves
the original buffer. Existing libc pointers remain with libc. This is an
experimental global allocator option and is OFF by default; PKG 0.17 is
unchanged. The current local test binary was linked with this option enabled.
Host ASan/UBSan tests cover growth/shrink/preservation, registry exhaustion,
tombstones, mapping failure cleanup and libc fallback:
`cc -std=c11 -O1 -g -fsanitize=address,undefined -I. tests/ps4/direct-heap.c -o /tmp/direct-heap-test && /tmp/direct-heap-test`.
Newton's const float-matrix API readers now copy into aligned local matrices
before calling internal SIMD routines, including collision queries.

Input follow-up: user confirms control in the cabin. The opening message
intentionally disables the player until Interact/Examine acknowledges it.
PS4 bindings now map left stick to movement, right stick to virtual mouse,
cross to Interact, circle to Examine, triangle to Jump and L1 to Crouch.
The isolated shadPS4 profile maps captured PC mouse to the RIGHT stick;
WASD moves, IJKL looks, N/left-click interacts, B/right-click examines.
Mouse deltas accumulate rather than being replaced by later motion/button
events. HPL's reversed Examine/Interact mouse defaults match its internal
actions now. Temporary input/message traces were removed from source.

Known rendering issue: the user observes the bunk bed turning black at some
view angles, also visible in local testing. Startup/control acceptance does
not establish rendering correctness or physical-console compatibility.
