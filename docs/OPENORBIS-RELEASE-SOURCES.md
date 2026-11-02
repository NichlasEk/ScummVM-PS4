# OpenOrbis source and notice audit for a PS4 release

## Update: recovered provenance, 2026-09-30

The online investigation now identifies the installed SDK tar **exactly** as
the official v0.5.3 LLVM 18 asset. LLVM v1.2 and SDL-PS4 v1.0 dependency archives
also byte-match the installed libraries. Their published source tags and
runtime source supplement are recorded in
[the Vulkan 1.26 provenance report](releases/vulkan-1.26/OPENORBIS-PROVENANCE.md).
Musl revision 779f951 remains inferred from the CI recipe, chronology and 183
matching headers. This update supersedes the unknown-artifact/source status
below; older standard-package hashes and notice inventory remain historical.

Status: working release document, 2026-09-28. Scope: the default
`ScummVM-PS4-2.9.1.pkg` and the OpenOrbis path used to build it. This is an
inventory and a short list of unresolved release items, not a declaration that
the package is ready to publish. The experimental Vulkan package and any
`--with-dott` package need separate manifests.

An older OpenOrbis version is fine: age does not change whether its source is
open. The local evidence points to an open-source toolchain path and no Sony
SDK binaries in this package. The revision work below is for matching source
and notices to a distributed binary, not for proving that OpenOrbis is open.

## Release boundary

OpenOrbis's [toolchain README](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain)
describes building without Sony's official SDK. Our `scripts/package-scummvm.sh`
copies three OpenOrbis-built modules into the package. The application also
contains code from SDK static archives. The SDK `.so` files named after PS4
services are link stubs; the actual services come from the console firmware
(or shadPS4 during emulator tests). Those `.so` files and the host packaging
tools are not copied into the PKG.

The local candidate inspected here is `dist/ScummVM-PS4-2.9.1.pkg`, SHA-256
`c519b288b663d4f7b5db0e86b23a2a7031f1dacc97984edaf9b807f3738191e1`.
It was built before this audit. Recompute the hash and manifest for the final
candidate; do not carry this value forward automatically.

## OpenOrbis files in the package

The three SDK sample binaries below match the corresponding built binaries
under `/opt/openorbis/OpenOrbis/PS4Toolchain/src/modules/` byte for byte.
Their visible source is in the SDK's `src/modules` tree. The SDK's
[source overview](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/master/src/README.md)
calls `libc` and `libSceFios2` empty stub modules.

| Package path | SHA-256 in local SDK | Source function |
| --- | --- | --- |
| `sce_module/libc.prx` | `39ad53672bb0b14895f8465eb1619478ab935f5afe5dbf69c15f02cfc0a75ce6` | One dummy export; no C library implementation. |
| `sce_module/libSceFios2.prx` | `3f8236c5996cf8e9917b8d27706d9743f6603d89de3b9859eacbb9f466b19c01` | One dummy export; no FIOS file I/O implementation. |
| `sce_sys/about/right.sprx` | `e3416f9c6152276b494982baf7b65ebcd8052274ffc42f95a776318022842b64` | OpenOrbis logo and attribution for the shell Information screen. |

Run `python3 tools/ps4-runtime-inventory/inventory.py` to recheck the module
hashes against another SDK installation. The matching source-build hash is
strong evidence about these particular files; the SDK archive's exact Git
revision is still unknown because this installation has no `.git` metadata or
recorded commit manifest.

The installed SDK can be traced to the local archive
`/opt/openorbis/toolchain-llvm-18.tar.gz` (SHA-256
`562e0bf5088d11041534059ab8963029a355786736d6b28fa62250fe5d02cb2d`).
Archive members `lib/libc.a`, `lib/libSDL2.a`, and `src/modules/right/right.sprx`
match the installed copies by SHA-256. This establishes the local artifact's
identity, but the archive has no recorded source commit. It must not be labeled
as a particular OpenOrbis release solely from its filename or file dates.
The [official v0.5.4 release metadata](https://api.github.com/repos/OpenOrbis/OpenOrbis-PS4-Toolchain/releases/tags/v0.5.4)
reports SHA-256 `3c7cd5bb593ca74fa1c13fd59f3938dc0fc07985167f7275063019e63abe4526`
for its similarly named archive, so this local archive is not that release asset.

## SDK code linked into the application

`scripts/configure-scummvm.sh`, `scripts/scummvm-sdl2-config`, and the current
`build/scummvm/config.mk` identify these direct OpenOrbis SDK inputs. These are
build-input hashes, not independently proven source-to-binary matches.

| SDK input | Local SHA-256 | Public source family | Current source match |
| --- | --- | --- | --- |
| `lib/crt1.o` | `4173c4401c0b5d0a72c2641f712982bb38a78e346515278a32fa99d4bbf9dd86` | OpenOrbis startup / musl integration | Exact source revision not recorded. |
| `lib/libc.a` | `dd3f25830490bc0aab6a87a1bce890a502e2ec874ac8c3169e37210c4e49aef7` | [OpenOrbis musl fork](https://github.com/OpenOrbis/musl) | Exact source revision not recorded. |
| `lib/libc++.a` | `9c0ddc37ccba2ab11b2a61a330af941c1bc0c806c8f6fade92db682049836e9a` | [OpenOrbis LLVM fork](https://github.com/OpenOrbis/llvm-project) | Exact source revision not recorded. |
| `lib/libc++abi.a` | `1f4b9aae6bbb78f2a175ac82b9fc5089f134299414c7c96aa71ad721ae041d2f` | [OpenOrbis LLVM fork](https://github.com/OpenOrbis/llvm-project) | Exact source revision not recorded. |
| `lib/libSDL2.a` | `629c87b4287d6865e5577c5e0086715ff38531b26a90a96237d1362469ebea45` | [OpenOrbis SDL-PS4 fork](https://github.com/OpenOrbis/SDL-PS4) | Base SDK archive revision not recorded. |
| `build/ps4-input/libSDL2.a` | `ff1a535e29e89da428b5fb12fc6c74def2ac30eb53bdae8200f8d15907206bc0` | Above archive with our PS4 joystick object replaced | Joystick source pinned to `7774cf3e7d04448c44d569c50d1f9a4532a2efe7`; base archive still unpinned. |

The link line also contains `-lm`; this SDK's `libm.a` is an empty 8-byte
archive (SHA-256 `f0a17a43c74d2fe5474fa2fd29c8f14799e777d7d75a2cc4d11c20a6e7b161c5`).
The six direct ELF `NEEDED` entries are `libkernel.so`, `libSceUserService.so`,
`libSceVideoOut.so`, `libSceAudioOut.so`, `libScePad.so`, and
`libSceSysmodule.so`. Their SDK `.so` files are link stubs and do not appear in
the package file list.

## Notices and corresponding source

The SDK root `LICENSE` is GPLv3. Its README points to separate musl, libc++,
and SDL-PS4 sources and licenses. The pinned SDL source tree used for our
joystick modification contains `COPYING.txt` with the SDL zlib-style license.
For the SDK archives, take the copyright/license files from the **identified
source revisions**, not from a newer checkout. Preserve source changes to the
SDL joystick code and our build script.

The current standard `build/scummvm-pkg/pkg.gp4` includes several codec and
ScummVM notices, but does **not** list a separate OpenOrbis SDK license, musl
copyright notice, libc++/libc++abi license notice, or SDL license notice.
`scripts/package-scummvm.sh` adds an SDL notice only for its experimental
Vulkan variant. The standard package needs its own notice set and a matching
source offer/bundle. GNU's [GPL FAQ](https://www.gnu.org/licenses/gpl-faq.en.html)
explains that source offered with a binary must correspond to that binary's
version; a byte-identical rebuild is not required.

## Remaining work for a weekend candidate

1. Identify the installed SDK release or obtain its build manifest. Pin the
   exact source revisions behind `crt1.o`, musl, libc++, libc++abi, and the
   base SDL archive. If that cannot be established, rebuild the needed static
   archives from pinned public source and relink a fresh candidate.
2. Add the matching OpenOrbis, musl, LLVM/libc++, and SDL notices to the
   standard package and release source bundle. Include the matching module
   source and the local SDL joystick modification/build instructions.
3. Build a fresh default PKG, inspect its generated `pkg.gp4` and package
   contents, and record final SHA-256 hashes. Keep commercial game data out of
   the default manifest. The current manifest lists only `games/README.txt`.
4. Make the exact corresponding source and build scripts available alongside
   the released binary. The older 1.01 source archives in
   `docs/FIRST-PUBLICATION.md` do not establish correspondence to a new PKG.

This audit does not require publishing PS4 firmware or Sony's system libraries;
those are supplied by the target platform. It also does not call for replacing
the three OpenOrbis modules merely because their filenames resemble Sony APIs.
