# Isolated ScummVM 2026.4.0 PS4 experiment

Checkpoint 2026-10-15: both software and Vulkan builds with SCUMM, PICE and
SOTC link, produce validated PS4 packages and visibly display the 2026.4.0
launcher in fresh shadPS4 profiles. Vulkan reports two successful presents.
These are emulator launcher checks, not gameplay/input/save/load or physical
PS4 validation. The combined Vulkan build now links with **126 top-level engines**, including
Watchmaker and Macs2, and its launcher is also visually verified in a fresh
shadPS4 profile. See the full-candidate result below.

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
