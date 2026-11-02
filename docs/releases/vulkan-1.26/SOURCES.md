# Source snapshots for Vulkan Experimental 1.26

`ScummVM-PS4-Vulkan-1.26-source.tar.gz` contains:

- `scummvm-src/`: the actual prepared ScummVM 2.9.1 source tree used by this
  build, including the PS4 patch, Vulkan overlay, PICE and SOTC. Upstream pin:
  `9d9b9e93108a276c551aeffa390169ccc5148e15`.
- `port/`: the current port source files, scripts, patches, tests, assets and
  documentation. The snapshot includes working-tree documentation edits; it is
  not labeled as a clean Git commit. Its base commit and per-file hashes are
  recorded in the archive's `SOURCE-MANIFEST.json`.
- `build-config/`: the exact generated config.h/config.mk and configuration
  output. Absolute paths in these records identify the original build location;
  regenerate configuration at your own location.

`ScummVM-PS4-Vulkan-1.26-dependency-sources.tar.gz` contains the codec/font
source archives used by the pinned build scripts, the locally patched sources
for OpenGNM, PSBC, Vulkan-PS4 and glslang, Vulkan/SPIR-V headers, the SDL source
used for the replaced objects, original and adapted libjbc sources, and the
available OpenOrbis module/startup/header sources. Git metadata, host profiles,
saves, commercial game data, SDK libraries and packaged executables are omitted.
Each included file has a SHA-256 entry in its source manifest.

## Build

Use a Linux build host with clang/LLVM, lld, make, CMake, Ninja, Python 3,
pkg-config, the normal autotools prerequisites and an OpenOrbis PS4 toolchain.
The code-generation requirements are in `port/scripts/psbc-codegen-requirements.txt`.
The scripts contain the precise dependency URLs, revisions and archive hashes.

Extract both archives into the same parent directory. Copy
`dependency-cache/downloads/`, `dependency-cache/ps4-libjbc/` and
`dependency-cache/sdl-input-src/` into `port/.tools/`. Copy the prepared
`scummvm-src/` directory to `port/.tools/scummvm-vulkan-src/`. Leave the Git-based
dependency snapshots outside `port/.tools/`: the scripts initialize fresh
checkouts at their pinned revisions and apply the included patches. Those
patched snapshots are supplied for inspection and independent builds.

From `port/`, with the external toolchain configured:

```sh
export OO_PS4_TOOLCHAIN=/path/to/OpenOrbis/PS4Toolchain
scripts/build-vulkan-probe.sh --sdlgl
SCUMMVM_CONTROLLER_ONLY=1 SCUMMVM_SERIAL_SUBMIT=0 SCUMMVM_OPERATION_SUBMIT=0 \
SCUMMVM_APP_VERSION=1.26 SCUMMVM_TITLE='ScummVM Vulkan Experimental 1.26' \
scripts/build-scummvm-vulkan.sh
```

Set up the pinned native checkouts before copying their patched snapshots, or
let the bootstrap fetch and patch them. These instructions have not been tested
as an independent clean-room rebuild. The release was built in the maintainer's
existing checkout; successful packaging does not establish clean-room rebuilds.

## Recovered OpenOrbis source references

Online artifact comparisons identify the installed SDK as **OpenOrbis v0.5.3,
LLVM 18**, its prebuilt C++/unwind/compiler-rt libraries as **OpenOrbis LLVM
v1.2**, and base SDL as **SDL-PS4 v1.0**. The installed files byte-match the
official downloaded assets. SDL's published source tag is also the base of
our replacement objects. Source tags, archive hashes, workflow details and
remaining limitations are recorded in [OPENORBIS-PROVENANCE.md](OPENORBIS-PROVENANCE.md)
and its JSON report.

`OpenOrbis-runtime-source-supplement.tar.gz` supplies the toolchain v0.5.3
source/build recipes, LLVM v1.2 runtime source projects and their CMake support,
pristine SDL v1.0 source, and the inferred musl source. The supplement's manifest
records its exact source scope; the full LLVM compiler monorepo can be obtained
at the linked tag. Matching component notices have been added under `licenses/`.

The likely musl revision is `779f95174b44d39e6a8a788b36289cf4768944a9`:
the last default-branch commit before the February 2025 build, with 183 matching
shipped headers. This remains an inference because historical CI checkout logs
are unavailable and no independent rebuild was performed. The older LLVM
workflow also fetched unpinned build-environment dependencies. Published source
references and verified binary origins do not imply byte-reproducible rebuilds.

The application PKG has been retained byte-for-byte. Additional runtime notices
are supplied beside it, without replacing its functioning runtime libraries.
Sony firmware libraries are supplied by the console and are not included here.
