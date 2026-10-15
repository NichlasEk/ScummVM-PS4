# ScummVM 2026.4.0: PS4 upgrade assessment

Initial audit: 2026-10-15. Follow-up: isolated software and Vulkan test packages
now link and show the 2026.4.0 launcher in shadPS4, including a full Vulkan
candidate with 126 engines (Watchmaker and Macs2 included). See the
[build experiment and current evidence](../experiments/scummvm-2026.4.0/README.md).
The initial audit below is retained as the pre-port baseline, not current build status. Existing build scripts, maintained engines, patches and
packages were not changed.

## Download and isolated directories

Latest stable release verified on the [official download page](https://www.scummvm.org/it/downloads):
2026.4.0, "Never-slop". Bundled NEWS.md dates the release 2026-10-10.
Our current source baseline is 2.9.1, pinned by `scripts/prepare-scummvm.sh`
to `9d9b9e93108a276c551aeffa390169ccc5148e15`.

- Clean upstream source: `.tools/upstream-2026.4.0/scummvm-2026.4.0/`
- Download: `.tools/upstream-2026.4.0/scummvm-2026.4.0.tar.xz`
- Disposable, partially patched audit copy: `.tools/upstream-2026.4.0/ps4-audit/`
- Separate build: `build/scummvm-2026.4.0-audit/`
- Logs, checksum and generated configure recipe: `artifacts/upstream-2026.4.0/`

Downloaded from:
https://downloads.scummvm.org/frs/scummvm/2026.4.0/scummvm-2026.4.0.tar.xz

SHA-256 matched the official published value:
`cd660b34104da7f85ed0a11b7c6c2320c629bb87ee297935711ebc1e09731f12`.
These generated directories are already covered by the repository's ignore rules.

## Verified results

The existing configure arguments work with 2026.4.0 and installed OpenOrbis
headers/libraries. The isolated audit recipe omits dependency rebuilds, copies
PICE/SOTC into the audit tree, and uses the normal software SDL configuration.
C++11 remains sufficient for configure; SDL2 is still supported.
All required codec checks passed: PNG, zlib, Theora, MPEG2, GIF, FLAC, VPX,
A52 and MT32Emu. PICE, SOTC and Director are enabled as static engines.

Five selected objects compiled: SDL backend, SDL window, SDL surface graphics,
PICE metaengine and SOTC metaengine. This does not validate missing PS4 patches.
A broader build of both local engine libraries produced `engines/sotc/libsotc.a`.
PICE failed at three `CursorMan.replaceCursor` calls in `interface.cpp` (55, 57)
and `input.cpp` (186): the old seventh `bool` argument is now a PixelFormat pointer.
Adapt these calls to the new cursor scaling API and check the resulting cursor
size/hotspot; simply compiling the metaengine does not prove full compatibility.

No complete executable link, package generation, emulator run or physical PS4
test was performed for this version.

## Patch migration

Dry runs used GNU patch with `--dry-run --batch --forward --fuzz=0 -p1` against
the clean release. Offsets were permitted; fuzzy matching was disabled.

| Patch | Files in patch | Failed hunks | Ignored hunks (missing files) |
| --- | ---: | ---: | ---: |
| PS4 base | 26 | 8 | 4 |
| Director 7 experimental | 30 | 52 | 12 |
| Watchmaker experimental | 38 | 4 | 0 |
| HPL1 experimental | 45 | 3 | 0 |

The base patch conflicts affect SDL window creation/mouse locking, surface
graphics, browser initialization, SDL initialization and GIF handling.
`backends/events/sdl/sdl-events.cpp` no longer exists: events are split into
`sdl-common-events.cpp` and SDL1/SDL2/SDL3 implementations. Port the four skipped
input hunks to the appropriate new implementations.

The saved Vulkan overlay contains 160 file sections and includes the three
experimental engine patches above. Its direct clean-source dry run reported
83 failed and 20 ignored hunks, but it normally requires the base PS4 patch first.
After applying only the successful base hunks to the disposable audit copy,
76 overlay hunks still failed. These figures overlap and must not be added
together or interpreted as independent defects. The audit copy contains `.rej`
files and is intentionally incomplete.

Rebase Director 7 especially carefully: many files/contexts changed. Review
upstream behavior before retaining old title-specific fixes. Regenerate the
Vulkan overlay from maintained scripts; its Python replacements/assertions
also depend on old source text. Retest GL capability reporting, fixed framebuffer
size, menu cursor, shader paths and the native Vulkan bridge.

## Recommended implementation order

1. Add a version-pinned preparation path and configurable isolated Vulkan
   source/build/stage/output paths. The current Vulkan entry point unconditionally
   selects its old directories, while preparation records the old revision.
   Do not point the existing preparation script at the pristine release tree.
2. Rebase the base PS4 patch, including event dispatch, controller/mouse handling,
   USB browsing, settings/saves, framebuffer and GIF changes. Finish a small
   software-rendered SCUMM build and link before enabling the full engine set.
3. Adapt the three PICE cursor calls and test both local engines, including
   save/load and controls. SOTC archive compilation is only a build gate.
4. Rebase the Vulkan overlay and Director/Watchmaker/HPL1 changes incrementally.
   The software audit skipped HPL1, Macs2, TWP and Watchmaker, as well as the
   intentionally excluded TestBed/Playground3D. Do not equate successful configure
   with complete engine coverage.
5. Review new engine dependencies and refresh the engine/data/shader inventory.
   There are 17 upstream engine directories absent from our old prepared tree:
   agds, alcachofa, alg, awe, bolt, colony, eem, fool, gamos, got, harvester,
   macs2, mediastation, pelrock, phoenixvr, tot and waynesworld. PICE/SOTC names
   do not collide with this release. Directory presence does not imply stability.
6. Update package naming (the software default still says 2.9.1), separately
   choose the PS4 APP_VER, and isolate candidate output from the existing package.
   Vulkan packaging currently enforces its fixed title ID and filename.
   Refresh themes, engine data, licenses and corresponding source archives.
7. Validate full link/imports and package extraction, then launcher, controller,
   audio, USB browsing and save/load in shadPS4. Physical PS4 startup/rendering
   and gameplay remain a separate required gate.

Evidence: `configure.log`, `compile-probe.log`, `local-engines-keep-going.log`,
`patch-summary.json`, per-patch dry-run logs and `vulkan-after-partial-base.log`
under the artifact directory above. The generated `configure-audit.sh` is a local
recipe with absolute paths, not a replacement production build script.
