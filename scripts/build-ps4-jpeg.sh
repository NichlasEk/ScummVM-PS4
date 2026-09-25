#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
sdk=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
version=3.2.0
archive="$root/.tools/downloads/libjpeg-turbo-$version.tar.gz"
source_dir="$root/.tools/libjpeg-turbo-$version"
build="$root/build/ps4-jpeg"
prefix="$build/install"
mkdir -p "$(dirname "$archive")" "$build"
if [[ ! -f "$archive" ]]; then
 curl -fL --retry 3 "https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/$version/libjpeg-turbo-$version.tar.gz" -o "$archive.tmp"
 mv "$archive.tmp" "$archive"
fi
printf '%s  %s\n' 6f30092cef9fb839779646608f4ee14ae3cbac989c47fa05e841b0841f09878e "$archive" | sha256sum -c -
if [[ ! -d "$source_dir" ]]; then
 tar -xf "$archive" -C "$root/.tools"
fi
fingerprint=$( { sha256sum "$root/scripts/build-ps4-jpeg.sh" "$root/compat/ps4/include/ps4-abi.h"; printf '%s\n' "$sdk"; clang --version; } | sha256sum | cut -d ' ' -f 1)
if [[ -f "$prefix/lib/libjpeg.a" && -f "$build/.build-id" ]] && [[ $(cat "$build/.build-id") == "$fingerprint" ]]; then
 exit 0
fi
cmake -S "$source_dir" -B "$build" \
 -DCMAKE_SYSTEM_NAME=FreeBSD -DCMAKE_SYSTEM_PROCESSOR=x86_64 \
 -DCMAKE_C_COMPILER=clang -DCMAKE_C_COMPILER_TARGET=x86_64-pc-freebsd12-elf \
 -DCMAKE_SYSROOT="$sdk" -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
 -DCMAKE_AR="$(command -v llvm-ar)" -DCMAKE_RANLIB="$(command -v llvm-ranlib)" \
 -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
 -DCMAKE_C_FLAGS="-D__PS4__ -include $root/compat/ps4/include/ps4-abi.h -isystem $sdk/include -mno-red-zone -fno-stack-protector" \
 -DENABLE_SHARED=OFF -DENABLE_STATIC=ON -DWITH_SIMD=OFF \
 -DWITH_TURBOJPEG=OFF -DWITH_TOOLS=OFF -DWITH_TESTS=OFF
cmake --build "$build" --target jpeg-static -j "${BUILD_JOBS:-12}"
# Install only the library and its public headers; never host tools or DSOs.
mkdir -p "$prefix/lib" "$prefix/include" "$prefix/share/licenses/libjpeg-turbo"
cp "$build/libjpeg.a" "$prefix/lib/"
cp "$source_dir/src/"{jpeglib.h,jmorecfg.h,jerror.h} "$build/jconfig.h" "$prefix/include/"
cp "$source_dir/LICENSE.md" "$source_dir/README.ijg" "$prefix/share/licenses/libjpeg-turbo/"
printf '%s\n' "$fingerprint" > "$build/.build-id"
