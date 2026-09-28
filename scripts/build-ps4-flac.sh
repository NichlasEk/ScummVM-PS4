#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
sdk=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
archive="$root/.tools/downloads/flac-1.5.0.tar.xz"
source_dir="$root/.tools/flac-1.5.0"
build="$root/build/ps4-flac"
prefix="$build/install"
mkdir -p "$root/.tools/downloads" "$build"
if [[ ! -f "$archive" ]]; then
 curl -fL --retry 3 'https://downloads.xiph.org/releases/flac/flac-1.5.0.tar.xz' -o "$archive.tmp"
 mv "$archive.tmp" "$archive"
fi
printf '%s  %s\n' f2c1c76592a82ffff8413ba3c4a1299b6c7ab06c734dee03fd88630485c2b920 "$archive" | sha256sum -c -
if [[ ! -d "$source_dir" ]]; then tar -xf "$archive" -C "$root/.tools"; fi
fingerprint=$( { sha256sum "$root/scripts/build-ps4-flac.sh" "$root/compat/ps4/include/ps4-abi.h"; printf '%s\n' "$sdk"; clang --version; } | sha256sum | cut -d ' ' -f 1)
if [[ -f "$prefix/lib/libFLAC.a" && -f "$build/.build-id" ]] && [[ $(cat "$build/.build-id") == "$fingerprint" ]]; then exit 0; fi
# Static cross-build: no host tools, encoder utilities, C++ wrapper or pthreads.
cmake -S "$source_dir" -B "$build" \
 -DCMAKE_SYSTEM_NAME=FreeBSD -DCMAKE_SYSTEM_PROCESSOR=x86_64 \
 -DCMAKE_C_COMPILER=clang -DCMAKE_C_COMPILER_TARGET=x86_64-pc-freebsd12-elf \
 -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_COMPILER_TARGET=x86_64-pc-freebsd12-elf \
 -DCMAKE_SYSROOT="$sdk" -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
 -DCMAKE_AR="$(command -v llvm-ar)" -DCMAKE_RANLIB="$(command -v llvm-ranlib)" \
 -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
 -DCMAKE_C_FLAGS="-D__PS4__ -include $root/compat/ps4/include/ps4-abi.h -isystem $sdk/include -mno-red-zone -fno-stack-protector" \
 -DBUILD_SHARED_LIBS=OFF -DBUILD_CXXLIBS=OFF -DBUILD_PROGRAMS=OFF \
 -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF -DBUILD_DOCS=OFF \
 -DWITH_OGG=OFF -DWITH_ASM=OFF -DENABLE_MULTITHREADING=OFF \
 -DWITH_FORTIFY_SOURCE=OFF -DWITH_STACK_PROTECTOR=OFF
cmake --build "$build" --target FLAC -j "${BUILD_JOBS:-12}"
mkdir -p "$prefix/lib" "$prefix/include/FLAC" "$prefix/share/licenses"
cp "$build/src/libFLAC/libFLAC.a" "$prefix/lib/"
cp "$source_dir/include/FLAC/"*.h "$prefix/include/FLAC/"
cp "$source_dir/COPYING.Xiph" "$prefix/share/licenses/flac-COPYING.Xiph"
printf '%s\n' "$fingerprint" > "$build/.build-id"
