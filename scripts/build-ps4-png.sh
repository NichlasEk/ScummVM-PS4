#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
sdk=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
build="$root/build/ps4-png"
prefix="$build/install"
mkdir -p "$root/.tools/downloads" "$build"
fetch() {
 local name=$1 url=$2 hash=$3 archive="$root/.tools/downloads/$1.tar.gz"
 if [[ ! -f "$archive" ]]; then
  curl -fL --retry 3 "$url" -o "$archive.tmp"
  mv "$archive.tmp" "$archive"
 fi
 printf '%s  %s\n' "$hash" "$archive" | sha256sum -c -
 if [[ ! -d "$root/.tools/$name" ]]; then tar -xf "$archive" -C "$root/.tools"; fi
}
fetch zlib-1.3.2 https://zlib.net/fossils/zlib-1.3.2.tar.gz bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16
fetch libpng-1.6.58 https://github.com/pnggroup/libpng/archive/refs/tags/v1.6.58.tar.gz a9d4df463d36a6e5f9c29bd6f4967312d17e996c1854f3511f833924eb1993cf
fingerprint=$( { sha256sum "$root/scripts/build-ps4-png.sh" "$root/compat/ps4/include/ps4-abi.h"; printf '%s\n' "$sdk"; clang --version; } | sha256sum | cut -d ' ' -f 1)
if [[ -f "$prefix/lib/libpng16.a" && -f "$prefix/lib/libz.a" && -f "$build/.build-id" ]] && [[ $(cat "$build/.build-id") == "$fingerprint" ]]; then exit 0; fi
options=(
 -DCMAKE_SYSTEM_NAME=FreeBSD -DCMAKE_SYSTEM_PROCESSOR=x86_64
 -DCMAKE_C_COMPILER=clang -DCMAKE_C_COMPILER_TARGET=x86_64-pc-freebsd12-elf
 -DCMAKE_SYSROOT="$sdk" -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY
 -DCMAKE_AR="$(command -v llvm-ar)" -DCMAKE_RANLIB="$(command -v llvm-ranlib)"
 -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON
 "-DCMAKE_C_FLAGS=-D__PS4__ -include $root/compat/ps4/include/ps4-abi.h -isystem $sdk/include -mno-red-zone -fno-stack-protector"
 -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_INSTALL_LIBDIR=lib
)
cmake --fresh -S "$root/.tools/zlib-1.3.2" -B "$build/zlib" "${options[@]}" \
 -DZLIB_BUILD_SHARED=OFF -DZLIB_BUILD_STATIC=ON -DZLIB_BUILD_TESTING=OFF
cmake --build "$build/zlib" -j "${BUILD_JOBS:-12}"
cmake --install "$build/zlib"
cmake --fresh -S "$root/.tools/libpng-1.6.58" -B "$build/png" "${options[@]}" \
 -DZLIB_INCLUDE_DIR="$prefix/include" -DZLIB_LIBRARY="$prefix/lib/libz.a" \
 -DPNG_SHARED=OFF -DPNG_STATIC=ON -DPNG_TESTS=OFF -DPNG_TOOLS=OFF \
 -DPNG_HARDWARE_OPTIMIZATIONS=OFF
cmake --build "$build/png" -j "${BUILD_JOBS:-12}"
cmake --install "$build/png"
mkdir -p "$prefix/share/licenses"
cp "$root/.tools/zlib-1.3.2/LICENSE" "$prefix/share/licenses/zlib-LICENSE"
cp "$root/.tools/libpng-1.6.58/LICENSE" "$prefix/share/licenses/libpng-LICENSE"
printf '%s\n' "$fingerprint" > "$build/.build-id"
