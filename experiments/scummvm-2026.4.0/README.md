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
