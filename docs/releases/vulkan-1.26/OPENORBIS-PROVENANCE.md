# Recovered OpenOrbis provenance — 2026-09-30

The installed SDK has now been identified as the official **OpenOrbis v0.5.3,
LLVM 18** distribution. This identification uses downloaded artifact hashes,
not filenames or timestamps. The application PKG remains unchanged.

## Verified artifact chain

The official [v0.5.3 release](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/releases/tag/v0.5.3)
contains `toolchain-llvm-18.2.zip`. Its single member,
`toolchain-llvm-18.tar.gz`, is 158,822,965 bytes and has SHA-256
`562e0bf5088d11041534059ab8963029a355786736d6b28fa62250fe5d02cb2d`.
It matches `/opt/openorbis/toolchain-llvm-18.tar.gz` exactly. All ten inspected
installed startup/library/module inputs also match their members in that tar.
Their hashes and comparisons are in OPENORBIS-PROVENANCE.json.

| Component | Verified binary origin | Source reference / confidence |
| --- | --- | --- |
| SDK modules and CRT support sources | Official v0.5.3 LLVM 18 distribution | Toolchain tag v0.5.3, `6929bafac0b565692ed61e66032a79185ee9cf65`. All five installed module C/header source files match this tag. |
| libc++, libc++abi, libunwind, compiler-rt builtins | Every archive byte-matches `libcxx-full.tar.gz` from OpenOrbis LLVM v1.2 | Published source tag `83e2569f0c062d980ae142d8dc583f6b8733be35`; its release workflow builds these components. No independent rebuild performed. |
| Base SDL archive | Byte-matches `SDL2.tar.gz` from OpenOrbis SDL-PS4 v1.0 | Published source tag `7774cf3e7d04448c44d569c50d1f9a4532a2efe7`, also the base of our replacement SDL objects. No independent rebuild performed. |
| musl libc and crt1.o | Byte-match official v0.5.3 distribution | Inferred source `779f95174b44d39e6a8a788b36289cf4768944a9`, the last default-branch commit before the build. 183 shipped musl headers match; no compared header differs. Historical CI checkout log unavailable. |

## How these binaries were built

The relevant recipe is the tag's
[GitHub Actions toolchain workflow](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/v0.5.3/.github/workflows/toolchain.yml),
not the old `build/build.sh` left in the SDK distribution.

It builds musl with clang, target `x86_64-scei-ps4`, shared libraries disabled,
and C flags `-fPIC -DPS4 -D_LIBUNWIND_IS_BAREMETAL=1`. It installs musl headers,
libraries and startup objects; builds firmware-service link stubs from public
definitions; downloads the latest published LLVM-runtime and SDL binaries; and
merges compiler-rt builtins into `libc.a`. The module Makefiles build libc/Fios
dummy modules and the OpenOrbis attribution module and copy them to the samples.

The [LLVM v1.2 release workflow](https://github.com/OpenOrbis/llvm-project/blob/v1.2/.github/workflows/release-stable.yml)
builds compiler-rt, libunwind, libc++abi and libc++ with static linking, PS4/musl
configuration and exception/unwinder support. It combines libc++, libc++abi and
libunwind into the shipped `libc++.a`; separate ABI/unwind archives are also
preserved. This explains why libc++.a contains unwind and ABI objects.

The inspected object comments show clang **18.1.4** for musl/crt1.o,
Ubuntu clang **11.0.0** for the C++/unwind libraries, and clang **10.0.0** for
SDL. That mix is consistent with the workflow downloading older dependency
releases rather than compiling every library with LLVM 18.

Upstream releases:
[LLVM v1.2](https://github.com/OpenOrbis/llvm-project/releases/tag/v1.2),
[SDL-PS4 v1.0](https://github.com/OpenOrbis/SDL-PS4/releases/tag/v1.0),
[inferred musl commit](https://github.com/OpenOrbis/musl/commit/779f95174b44d39e6a8a788b36289cf4768944a9).

## Included source supplement

`OpenOrbis-runtime-source-supplement.tar.gz` contains the musl candidate source,
the LLVM v1.2 runtime sources and CMake support used by those runtime projects,
the pristine SDL v1.0 source, and the toolchain v0.5.3 source/build recipes.
Git metadata and prebuilt executables are omitted. Per-file hashes and source
references are recorded in its SOURCE-MANIFEST.json. The original release
archives already contain our patched SDL objects' source and adaptation scripts.
Matching musl, LLVM-runtime and SDL notices are also supplied under `licenses/`.

## Remaining limits

Binary origin is verified for the inspected inputs. Matching published source
tags are identified for LLVM and SDL. The musl checkout is inferred from the
documented default-branch checkout, commit chronology and matching headers;
neither a historical checkout log nor an independent source-to-binary rebuild
has been recovered. Header matches alone do not prove all musl implementation
files match a particular binary.

The LLVM release workflow also fetched then-current toolchain headers and musl
build dependencies without pinning all their revisions. Its published runtime
source tag is known; a fully reproducible historical build environment is not.
The supplement contains selected LLVM runtime projects, not the entire LLVM
compiler monorepo. Obtain that full tag for additional compiler/build tooling.

This document records provenance and source availability, not a legal opinion
or a claim of a completed independent rebuild. The maintainer chose to retain
the existing experimental PKG rather than replace working runtime binaries.
