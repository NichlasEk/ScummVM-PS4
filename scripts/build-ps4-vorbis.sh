#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
sdk=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
build="$root/build/ps4-vorbis"
prefix="$build/install"
mkdir -p "$root/.tools/downloads" "$build"
fetch() {
 local name=$1 family=$2 hash=$3 archive="$root/.tools/downloads/$1.tar.xz"
 if [[ ! -f "$archive" ]]; then
  curl -fL --retry 3 "https://downloads.xiph.org/releases/$family/$name.tar.xz" -o "$archive.tmp"
  mv "$archive.tmp" "$archive"
 fi
 printf '%s  %s\n' "$hash" "$archive" | sha256sum -c -
 if [[ ! -d "$root/.tools/$name" ]]; then tar -xf "$archive" -C "$root/.tools"; fi
}
fetch libogg-1.3.6 ogg 5c8253428e181840cd20d41f3ca16557a9cc04bad4a3d04cce84808677fa1061
fetch libvorbis-1.3.7 vorbis b33cc4934322bcbf6efcbacf49e3ca01aadbea4114ec9589d1b1e9d20f72954b
fingerprint=$( { sha256sum "$root/scripts/build-ps4-vorbis.sh" "$root/compat/ps4/include/ps4-abi.h"; printf '%s\n' "$sdk"; clang --version; } | sha256sum | cut -d ' ' -f 1)
if [[ -f "$prefix/lib/libvorbisfile.a" && -f "$prefix/lib/libvorbis.a" && -f "$prefix/lib/libogg.a" && -f "$build/.build-id" ]] && [[ $(cat "$build/.build-id") == "$fingerprint" ]]; then exit 0; fi
options=(
 -DCMAKE_POLICY_VERSION_MINIMUM=3.5
 -DCMAKE_SYSTEM_NAME=Generic -DCMAKE_SYSTEM_PROCESSOR=x86_64
 -DCMAKE_C_COMPILER=clang -DCMAKE_C_COMPILER_TARGET=x86_64-pc-freebsd12-elf
 -DCMAKE_SYSROOT="$sdk" -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
 -DCMAKE_AR="$(command -v llvm-ar)" -DCMAKE_RANLIB="$(command -v llvm-ranlib)"
 -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON
 -DCMAKE_C_FLAGS="-D__PS4__ -include $root/compat/ps4/include/ps4-abi.h -isystem $sdk/include -mno-red-zone -fno-stack-protector -ffp-contract=off"
 -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_INSTALL_LIBDIR=lib
 -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF)
cmake --fresh -S "$root/.tools/libogg-1.3.6" -B "$build/ogg" "${options[@]}" -DINSTALL_DOCS=OFF
cmake --build "$build/ogg" -j "${BUILD_JOBS:-12}"
cmake --install "$build/ogg"
# Explicit target paths prevent CMake from discovering the host's Ogg library.
PKG_CONFIG_LIBDIR="$prefix/lib/pkgconfig" PKG_CONFIG_PATH= cmake --fresh \
 -S "$root/.tools/libvorbis-1.3.7" -B "$build/vorbis" "${options[@]}" \
 -DOGG_INCLUDE_DIR="$prefix/include" -DOGG_LIBRARY="$prefix/lib/libogg.a"
cmake --build "$build/vorbis" -j "${BUILD_JOBS:-12}"
cmake --install "$build/vorbis"
mkdir -p "$prefix/share/licenses"
cp "$root/.tools/libogg-1.3.6/COPYING" "$prefix/share/licenses/libogg-COPYING"
cp "$root/.tools/libvorbis-1.3.7/COPYING" "$prefix/share/licenses/libvorbis-COPYING"
printf '%s\n' "$fingerprint" > "$build/.build-id"
