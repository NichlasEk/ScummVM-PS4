# RC2 source companion

The release source archives contain the prepared ScummVM 2026.4.0 source,
the port repository snapshot, generated build configuration, and locally
patched dependency sources. Each archive has a per-file SHA-256 manifest.
The repository release tag supplies the port history as well.

`ScummVM-PS4-2026.4.0-RC2-source.tar.gz` contains `port/`, `scummvm-src/`
and `build-config/`. Host paths in the configuration snapshot are replaced
with `/path/to/port`; regenerate configuration rather than using that snapshot
as a Makefile. Patch backups/rejects, Git databases, build executables, private
games, emulator profiles and test saves are excluded.

`ScummVM-PS4-2026.4.0-RC2-dependency-sources.tar.gz` contains the patched
OpenGNM/Vulkan/PSBC stack and headers, glslang, SDL, libjbc, codec/font source
archives, generated SDL adaptation source and available OpenOrbis support
sources. `OpenOrbis-runtime-source-supplement.tar.gz` supplies the previously
recovered musl/LLVM/SDL/toolchain source snapshots. Component notices accompany
the package in `ScummVM-PS4-2026.4.0-RC2-notices.tar.gz`.

## Provenance

The SDK inputs are checked against the recorded official OpenOrbis v0.5.3
LLVM 18 distribution. LLVM and base SDL match their published release assets.
Musl revision `779f95174b44d39e6a8a788b36289cf4768944a9` remains inferred from the
historical workflow and matching headers, without an independent historical
rebuild. See OPENORBIS-PROVENANCE.md and SDK-INPUTS.json in the release.

The current patched SDL audio code is included; the older source supplement
alone is not the complete SDL input. RC2 preserves the exact TLJ executable
accepted in shadPS4, including its diagnostic build paths. It includes the
committed Director fixes through 0.42 and the mirrored-repeat correction.
No fresh-machine or byte-identical rebuild is claimed.

## Build order

Use Linux with an OpenOrbis SDK, clang/LLVM/lld, make, CMake, Ninja, Python 3,
pkg-config and autotools. Extract both RC2 source archives into one directory.
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
2026.4.0 configuration; its older package is not RC2. Copy the supplied
`scummvm-src/` into `port/.tools/upstream-2026.4.0/ps4-vulkan/`, then run:

```sh
SCUMMVM_ENGINES='' bash experiments/scummvm-2026.4.0/configure-vulkan.sh
make -C build/scummvm-2026.4.0-vulkan -j8
SCUMMVM_APP_VERSION=0.43 SCUMMVM_TITLE='ScummVM PS4' \
  bash experiments/scummvm-2026.4.0/package-vulkan.sh
```

The output is a **new local build**. The packaging script uses its historical
test filename. RC2 was assembled from the existing resource manifest with the
exact accepted executable, title "ScummVM PS4" and APP_VER/VERSION 0.43.
BUILD-IDENTITY.json identifies the release executable, package and source commit.
`scripts/assemble-rc2-sources.py` assembles the source companions from the
explicit source, build, staging and dependency-workspace paths without relinking.

For clean-upstream patch preparation, see
`experiments/scummvm-2026.4.0/README.md`. The verified upstream archive hash is
`cd660b34104da7f85ed0a11b7c6c2320c629bb87ee297935711ebc1e09731f12`.
