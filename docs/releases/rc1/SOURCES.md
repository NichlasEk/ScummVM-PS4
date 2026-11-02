# RC1 source companion

The release source archives contain the prepared ScummVM 2026.4.0 source,
the port repository snapshot, generated build configuration, and locally
patched dependency sources. Each archive has a per-file SHA-256 manifest.
The repository release tag supplies the port history as well.

`ScummVM-PS4-2026.4.0-RC1-source.tar.gz` contains `port/`, `scummvm-src/`
and `build-config/`. Host paths in the configuration snapshot are replaced
with `/path/to/port`; regenerate configuration rather than using that snapshot
as a Makefile. Patch backups/rejects, Git databases, build executables, private
games, emulator profiles and test saves are excluded.

`ScummVM-PS4-2026.4.0-RC1-dependency-sources.tar.gz` contains the patched
OpenGNM/Vulkan/PSBC stack and headers, glslang, SDL, libjbc, codec/font source
archives, generated SDL adaptation source and available OpenOrbis support
sources. `OpenOrbis-runtime-source-supplement.tar.gz` supplies the previously
recovered musl/LLVM/SDL/toolchain source snapshots. Component notices accompany
the package in `ScummVM-PS4-2026.4.0-RC1-notices.tar.gz`.

## Provenance

The SDK inputs are checked against the recorded official OpenOrbis v0.5.3
LLVM 18 distribution. LLVM and base SDL match their published release assets.
Musl revision `779f95174b44d39e6a8a788b36289cf4768944a9` remains inferred from the
historical workflow and matching headers, without an independent historical
rebuild. See OPENORBIS-PROVENANCE.md and SDK-INPUTS.json in the release.

The current patched SDL audio code is included; the older source supplement
alone is not the complete SDL input for RC1. The package keeps the exact 0.32
program, including diagnostic build paths, with the maintainer's approval.
No fresh-machine or byte-identical rebuild is claimed.

## Build order

Use Linux with an OpenOrbis SDK, clang/LLVM/lld, make, CMake, Ninja, Python 3,
pkg-config and autotools. Extract both RC1 source archives into one directory.
Copy `dependency-cache/downloads`, `dependency-cache/ps4-libjbc` and
`dependency-cache/sdl-input-src` into `port/.tools/`. The patched Git-based
snapshots are included for inspection; bootstrap scripts fetch pinned sources
and apply the port's patches. Network access is needed for that bootstrap.

From `port/`:

```sh
export OO_PS4_TOOLCHAIN=/path/to/OpenOrbis/PS4Toolchain
bash scripts/build-vulkan-probe.sh --sdlgl
SCUMMVM_ENGINES=scumm bash scripts/build-scummvm-vulkan.sh
```

This prepares native dependencies and the older bridge build reused by the
2026.4.0 configuration; its older package is not RC1. Copy the supplied
`scummvm-src/` into `port/.tools/upstream-2026.4.0/ps4-vulkan/`, then run:

```sh
SCUMMVM_ENGINES='' bash experiments/scummvm-2026.4.0/configure-vulkan.sh
make -C build/scummvm-2026.4.0-vulkan -j8
SCUMMVM_APP_VERSION=0.32 SCUMMVM_TITLE='ScummVM PS4' \
  bash experiments/scummvm-2026.4.0/package-vulkan.sh
```

The output is a **new local build**, not the previously tested RC1 artifact.
The packaging script uses its historical output filename by default. RC1 was
created by changing only the SFO title in the already validated 0.32 staging
manifest, rebuilding the PKG and comparing every extracted file. The release's
BUILD-IDENTITY.json records the resulting identity.

For clean-upstream patch preparation, see
`experiments/scummvm-2026.4.0/README.md`. The verified upstream archive hash is
`cd660b34104da7f85ed0a11b7c6c2320c629bb87ee297935711ebc1e09731f12`.
