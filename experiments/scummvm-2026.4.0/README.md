# Isolated ScummVM 2026.4.0 PS4 experiment

Checkpoint 2026-10-15: both software and Vulkan builds with SCUMM, PICE and
SOTC link, produce validated PS4 packages and visibly display the 2026.4.0
launcher in fresh shadPS4 profiles. Vulkan reports two successful presents.
These are emulator launcher checks, not gameplay/input/save/load or physical
PS4 validation. The broader engine build is being investigated separately.

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
