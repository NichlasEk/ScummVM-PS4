#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
sdk=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
version=2.14.3
archive="$root/.tools/downloads/freetype-$version.tar.xz"
source_dir="$root/.tools/freetype-$version"
build="$root/build/ps4-freetype"
prefix="$build/install"
mkdir -p "$(dirname "$archive")" "$build"
if [[ ! -f "$archive" ]]; then
 curl -fL --retry 3 "https://download.savannah.gnu.org/releases/freetype/freetype-$version.tar.xz" -o "$archive.tmp"
 mv "$archive.tmp" "$archive"
fi
printf '%s  %s\n' 36bc4f1cc413335368ee656c42afca65c5a3987e8768cc28cf11ba775e785a5f "$archive" | sha256sum -c -
if [[ ! -d "$source_dir" ]]; then
 tar -xf "$archive" -C "$root/.tools"
fi
fingerprint=$( { sha256sum "$root/scripts/build-ps4-freetype.sh" "$root/compat/ps4/include/ps4-abi.h"; printf '%s\n' "$sdk"; clang --version; } | sha256sum | cut -d ' ' -f 1)
if [[ -f "$prefix/lib/libfreetype.a" && -f "$build/.build-id" ]] && [[ $(cat "$build/.build-id") == "$fingerprint" ]]; then
 exit 0
fi
# Generic selects FreeType's portable stdio backend. Its Unix mmap backend
# requires file mappings that OpenOrbis/shadPS4 do not implement reliably.
cmake --fresh -S "$source_dir" -B "$build" \
 -DCMAKE_SYSTEM_NAME=Generic -DCMAKE_SYSTEM_PROCESSOR=x86_64 \
 -DCMAKE_C_COMPILER=clang -DCMAKE_C_COMPILER_TARGET=x86_64-pc-freebsd12-elf \
 -DCMAKE_SYSROOT="$sdk" -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
 -DCMAKE_AR="$(command -v llvm-ar)" -DCMAKE_RANLIB="$(command -v llvm-ranlib)" \
 -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
 -DCMAKE_C_FLAGS="-D__PS4__ -include $root/compat/ps4/include/ps4-abi.h -isystem $sdk/include -mno-red-zone -fno-stack-protector" \
 -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_INSTALL_LIBDIR=lib \
 -DBUILD_SHARED_LIBS=OFF -DFT_DISABLE_ZLIB=ON -DFT_DISABLE_BZIP2=ON \
 -DFT_DISABLE_PNG=ON -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BROTLI=ON
cmake --build "$build" -j "${BUILD_JOBS:-12}"
cmake --install "$build"
mkdir -p "$prefix/share/licenses/freetype"
cp "$source_dir/docs/FTL.TXT" "$source_dir/docs/GPLv2.TXT" "$source_dir/LICENSE.TXT" "$prefix/share/licenses/freetype/"
printf '%s\n' "$fingerprint" > "$build/.build-id"
