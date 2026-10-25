# ScummVM for PlayStation 4

An experimental OpenOrbis port of ScummVM with controller input, software
rendering and a PS4 graphics bridge backed by Vulkan/OpenGNM. The current
compatibility work uses **ScummVM 2026.4.0**, test title **SCVM00265**.
The older 2.9.1 port remains available through its separate build scripts.

**Latest local candidate: 0.30, checked on 2026-10-25.** Jönssonligan: Jakten
på Mjölner now keeps the attic music playing, displays all three opening
clips and clears the previous clip behind the next one in shadPS4. See the
[0.30 findings and package](docs/PS4-JONSSON-030.md). Physical PS4 retesting
and prolonged gameplay remain pending.

**Compatibility baseline: 0.29, checked on 2026-10-24.** Grim Fandango,
Escape from Monkey Island, Rise of the Dragon and Dark Side have been tested
in visible shadPS4 sessions. The exact coverage and remaining checks are listed
below. This is an experimental port, with no full-game compatibility guarantee.

Default packages contain **no commercial game data or MT-32 ROMs**. Supply your
own game files separately. Local test packages and private test assets are not
included in this repository.

## Current status

| Area | Verified behavior | Remaining checks |
| --- | --- | --- |
| Launcher and input | Launcher, mouse pointer and controller input work in shadPS4; DOTT was previously user-confirmed on physical PS4 | Input and gameplay vary by engine |
| MT-32 | User-confirmed working after the SDL audio pacing correction; the compatibility work preserves the fixed SDL archive | Recheck audio on the physical console with 0.29 |
| Scalers | Edge 2x and 3x reported working by community testers | This does not establish support for every scaler or shader |
| 3D rendering | Several engines reach menus and scenes through the PS4 graphics bridge | The bridge implements a subset of OpenGL behavior; engine-specific limitations remain |
| Freescape | Dark Side reaches a controllable 3D scene using TinyGL in shadPS4 | Physical PS4 performance and later gameplay |
| Candidate delivery | 0.29 package validated, extracted payloads compared, FTP delivery fully read back | Installation and physical gameplay were deferred by the tester |

Build success, emulator gameplay, FTP delivery, installation and physical PS4
behavior are separate validation steps. A checked scene is not a complete
playthrough. See also the [direct GPU command-memory hardware findings](docs/DIRECT-PM4-HARDWARE.md).

## Compatibility checks: October 23–24

| Game / tested edition | Engine / renderer | Observed result | Still unverified |
| --- | --- | --- | --- |
| Grim Fandango — Windows, English | Grim / OpenGL shaders through the PS4 bridge | Intro, office and movement worked even in 0.26. The reproducible Save Game freeze was fixed in 0.28; a thumbnail and save file were created, then the game returned to the office | Reloading that save, later areas and the new fixes on physical PS4 |
| Escape from Monkey Island — Windows, English | Grim / OpenGL shaders through the PS4 bridge | Visible test accepted by the tester after the 0.29 color-format and screen-copy fixes; intro and graphics progressed | Full playthrough, save/load and physical PS4 performance |
| Rise of the Dragon — DOS VGA, German | DGDS | Starts and appears functional with the controller mapping fix; tester confirmed the visible session | English/EGA editions, extended input/save testing and physical PS4 |
| Dark Side — DOS EGA, English | Freescape / TinyGL | Title screen, 3D scene and directional controls verified in shadPS4; tester confirmed it appears functional | Extended gameplay, save/load and physical PS4 |
| Driller | Freescape / TinyGL selected by the same engine change | No game test performed; deliberately deferred | All runtime behavior |

Grim's Save Game test used the 0.28 candidate. The accepted Monkey Island test
used the 0.29 development executable before packaging relinked the build.
Dragon and Dark Side used the executable extracted from the final 0.29 package.
These distinctions are retained in the local test journal.

### What was fixed

- **Grim save-screen freeze:** the native Vulkan command buffer had 16 image-copy
  scratch slots, while the save menu recorded 30 copies before submission. The
  limit is now 32, with all allocation cleanup paths using the same constant.
  An ASan/UBSan lifetime test exercises destruction and trimming of every slot.
- **Grim/EMI screenshot capture:** PS4 captures the stored display through a
  framebuffer and `glReadPixels`, replacing the unavailable `glGetTexImage`
  path. Queued work is submitted before readback; failures include diagnostics.
- **Monkey Island colors:** normalized byte color attributes are converted to
  floats on PS4 to match the graphics bridge's supported vertex formats.
- **Monkey Island screen transitions:** eight screen tiles previously caused
  1,536 synchronous scanline readbacks. Each tile now uses one readback, with
  explicit padding for the parts extending beyond the screen. This removes a
  concrete source of stalls; it is not a measured overall speedup claim.
- **DGDS controls:** its engine-specific keymap now includes PS4 mouse clicks,
  menu navigation and activation. Optional diagnostics record action events and
  mouse coordinates. No speculative mouse-coordinate transform was added.
- **Freescape renderer selection:** PS4 selects TinyGL because the hardware
  path requires polygon offset and line-width functions absent from the bridge.
  This applies to the engine, including Dark Side and Driller, but only Dark Side
  was exercised in this test round.

The engine changes live in
[`3d-engines.patch`](experiments/scummvm-2026.4.0/3d-engines.patch), the native
copy-slot change in [`vulkan-runtime.patch`](patches/opengnm/vulkan-runtime.patch),
and the lifetime check in [`pm4-lifetime.c`](tests/ps4/pm4-lifetime.c).
The engine patch replayed cleanly against the pinned source and matched all
12 modified files. Compatibility code was committed as `25ce4d7`.

### Audio preservation

The earlier MT-32 problem was traced to SDL draining audio with a null
`sceAudioOutOutput` call after each submitted buffer. The corrected wait path
leaves pacing to normal buffer submission. The compatibility fixes above do
not change that audio path; the linked SDL archive remained byte-identical
through these tests. This preserves the working fix without treating a new
package transfer as a new hardware audio test.

## Install candidate 0.29 and add games

The current local artifact is:

```text
dist/ps4-compat-029/ScummVM-2026.4.0-Vulkan-Test.pkg
```

| Package field | Value |
| --- | --- |
| Application | ScummVM 2026.4.0 Vulkan Test |
| Title ID | `SCVM00265` |
| Content ID | `IV0000-SCVM00265_00-SCUMMVM2026VK001` |
| APP_VER | `0.29` |
| Size | 250,216,448 bytes |
| SHA-256 | `39f640f68bbb474a6204cb57c9ac889acf469e0429547f05a1b924728d5de52f` |

PKG validation passed, and all **241 packaged payloads** matched the staging
files after extraction. In the latest private console delivery, the package
was placed at `/data/pkg/ScummVM-2026.4.0-0.29.pkg`; a separate copy of its
`eboot.bin` was also transferred. Install the **PKG** using your console's
package installer. The loose executable is not needed for normal installation.
The latest delivery was **not installed or launched**.

1. Install the intended `SCVM00265` package. The older software and Vulkan
   experiments use different title IDs; check the package identity before
   replacing an installation.
2. Put each game's data in its own directory, for example
   `/data/scummvm/games/darkside/`.
3. In ScummVM, choose **Add Game** and select that game's directory. Preserve
   required subdirectories, especially for Escape from Monkey Island.
4. Let detection select the edition and language. The tested Dragon archive
   is **German DOS VGA**; forcing English caused a detection error. Sega CD
   BIN/CUE images are not the DOS DGDS data used for this test.
5. Keep configuration and saves when updating. The current title uses
   `/data/scummvm-2026-vulkan/scummvm.ini` and
   `/data/scummvm-2026-vulkan/saves/`.

The private delivery checked **574 files / 2,888,684,381 bytes** by full FTP
readback and SHA-256 comparison, covering four games, the PKG and the loose
executable. Existing Grim files matched; the other games were transferred and
added to the launcher. Existing settings and saves were preserved and backed
up. These game files are not part of a public package or this repository.

### Controller notes

Launcher controls and engine controls can differ. On the tested DGDS build:

| PS4 input | Action |
| --- | --- |
| Cross | Left mouse click |
| Circle | Right mouse click |
| D-pad Up / Down | Previous / next menu or dialog choice |
| Square | Activate the selected menu or dialog choice |
| Triangle | Open the game menu |

Freescape has its own directional and action mappings. Use ScummVM's keymapper
for engine-specific adjustments. In the local shadPS4 test profiles, keyboard
`N/B/V/C` represents Cross/Circle/Square/Triangle and arrow keys represent the
D-pad. Those are emulator bindings, not PS4 keyboard requirements.

## Building the current 2026.4.0 experiment

The 2026.4.0 recipes reuse an existing OpenOrbis/SDL/codec and Vulkan/OpenGNM
build environment. They are **not a complete fresh-machine toolchain bootstrap**.
Read the [2026.4.0 experiment instructions](experiments/scummvm-2026.4.0/README.md)
for the pinned source archive, dependency layout and source preparation.

From the repository root, with those dependencies prepared:

```sh
bash experiments/scummvm-2026.4.0/build.sh vulkan all

# Use separate output directories for a new candidate.
SCUMMVM_APP_VERSION=0.30 \
SCUMMVM_STAGE_DIR="$PWD/build/scummvm-compat-030-pkg" \
SCUMMVM_DIST_DIR="$PWD/dist/ps4-compat-030" \
  bash experiments/scummvm-2026.4.0/package-vulkan.sh
```

`0.30` above is an example for the next build, not a tested release. Packaging
is a separate step and may relink the executable; preserve the exact accepted
artifact before rebuilding. The recorded 0.29 package was made from a working
checkout that also contained earlier, uncommitted audio/filter changes. The
compatibility commit alone is not a byte-reproducible source release of that
package. Those unrelated working changes were not included in the compatibility
commit.

| Purpose | Location |
| --- | --- |
| Patched 2026.4.0 source | `.tools/upstream-2026.4.0/ps4-vulkan/` |
| Cross-build | `build/scummvm-2026.4.0-vulkan/` |
| Final 0.29 extracted payload | `build/scummvm-compat-029-verified/` |
| Earlier audio candidate / rollback | `dist/ps4-audio-026/ScummVM-2026.4.0-Vulkan-Test.pkg` |
| Local compatibility logs and screenshots | `artifacts/compat-20261023/` |
| Local transfer manifest, readback hashes and backup | `artifacts/ftp-compat-20261024/` |

Generated builds, packages, private game files and local evidence stay outside
Git. See [runtime module setup](external/ps4-runtime/README.md) for separately
provided runtime components.

## Other engines and earlier checks

The broad experimental build includes the local Prisoner of Ice and Shadow of
the Comet engines, Jönssonligan Director work, Watchmaker/Penumbra adaptations,
and optional audio/video codecs. Compiled engine support does not establish
playability. The [engine inventory](docs/ENGINE-SUPPORT.md) and
[combined-build notes](docs/FULL-BUILD.md) include older build generations;
use the title IDs and paths above for the current 2026.4.0 candidate.

Earlier shadPS4 observations include Myst III's opening panorama, The Longest
Journey's opening cabin dialogue, Thimbleweed Park's tutorial and startup
screens in DOTT, Broken Sword 2.5, Buried in Time, Prisoner of Ice and Shadow of
the Comet. These were limited scene checks, not full compatibility tests.
See the [dated experiment evidence](experiments/scummvm-2026.4.0/README.md#3d-engine-validation-2026-10-15).
Penumbra remains experimental; consult its [detailed investigation](docs/PS4-PENUMBRA.md)
for later findings instead of treating an old startup crash as the current result.

## Reporting a problem

Include the package version/title ID, game platform and language, renderer,
and whether the test ran on physical PS4 or shadPS4. Describe the exact action
that fails, such as opening **Save Game**, rather than only “does not work”.
Attach the relevant log excerpt and, where useful, a screenshot. Configuration
and logs for the current title are under `/data/scummvm-2026-vulkan/`.
Do not include commercial game data, MT-32 ROMs or credentials in reports.

The next useful checks are physical PS4 gameplay with 0.29, Grim save reload,
longer Monkey Island sessions and DGDS input across more screens. Driller was
explicitly deferred; no runtime result is claimed for it.

## Legacy 2.9.1 emulator workflow

```sh
./scripts/run-emulator.sh
```

The script prepares pinned ScummVM sources, applies the PS4 patch, builds the
PS4 executable and PKG, installs the verified emulator release if needed,
and boots ScummVM. The initial build takes several minutes; later builds
are incremental. No session timeout by default. For a bounded test, use
`EMULATOR_TIMEOUT=120 ./scripts/run-emulator.sh`.

- **F7**: capture/release PC mouse movement.
- **Left/right mouse button**: left/right click.
- **WASD**: left stick / pointer when mouse capture is off.
- **N/B**: Cross/Circle, corresponding to left/right click in the launcher.
- **F8**: reload input bindings after changing sensitivity.
- **F12**: save a screenshot.

Mouse amplification is **3×**. Adjust `mouse_movement_params` in
`.tools/xdg/shadPS4/input_config/SCVM00002.ini`; the middle value controls
amplification. Existing bindings are preserved on subsequent launches.

The list is initially empty because no commercial game data is bundled.
The emulator mounts `build/scummvm-pkg` as `/app0`; game folders can be put
under `build/scummvm-pkg/games` and selected with **Add Game**. This does not
add them to the hardware PKG automatically.

## Legacy 2.9.1 build and artifacts

Requirements: the OpenOrbis SDK with its SDL port at
`/opt/openorbis/OpenOrbis/PS4Toolchain`, host `clang++`, `ld.lld`, LLVM archive
tools, `make`, Python 3, Git, curl, and unzip. Set `OO_PS4_TOOLCHAIN` to use
another installation with the same SDK layout.

```sh
./scripts/build-scummvm.sh
```

- Package: `dist/ScummVM-PS4-2.9.1.pkg`
- Guest executable: `build/scummvm-pkg/eboot.bin`
- Guest settings/saves/logs: `.tools/xdg/shadPS4/data/scummvm/`
- Build logs/evidence: `artifacts/scummvm/`
- Emulator run logs: `artifacts/emulator/`
- Screenshots: `.tools/xdg/shadPS4/screenshots/`

Source preparation uses the pinned clean revision from the earlier checkout
at `/home/developer/ps4-scummvm/scummvm` when available, otherwise GitHub. The
old checkout is read-only reference material. The portable changes live in
`patches/scummvm/ps4.patch`; generated sources/builds stay ignored by Git.

See [ScummVM validation and remaining work](docs/SCUMMVM.md).

## Bundle your DOTT CD data for a PS4 test

```sh
./scripts/build-scummvm.sh
./scripts/package-scummvm.sh --with-dott /path/to/dott-data
```

The directory must contain `tentacle.000`, `tentacle.001`, and `monster.sou`.
The result is `dist/ScummVM-PS4-2.9.1-DOTT.pkg`, with DOTT preselected on a
fresh installation. Existing settings and saves are preserved. See
[DOTT hardware-test package](docs/DOTT-PS4-TEST.md) for verification and setup.

## Diagnostic app

The smaller hardware diagnostic is still available separately:

```sh
./scripts/run-emulator.sh --diagnostic
./scripts/build-diagnostic.sh
```

Its package is under `diagnostic/dist-openorbis/`, title ID `SCVM00001`.
Three green bars indicate initialized video/audio/controller. Cross changes
the background and plays a tone; Circle exits. On hardware the log is
`/data/scummvm-ps4-diagnostic.log`. See [diagnostic evidence](docs/EMULATOR.md).

Project code is GPL-3.0-or-later; generated packages also include separately
licensed OpenOrbis/SDL components. ScummVM retains its upstream license notices.
External prebuilt PS4 modules are not included in Git; see the
[module placeholders and setup instructions](external/ps4-runtime/README.md).
