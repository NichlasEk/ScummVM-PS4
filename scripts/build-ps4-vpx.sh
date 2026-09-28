#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
sdk=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
archive="$root/.tools/downloads/libvpx-1.15.2.tar.gz"
source_dir="$root/.tools/libvpx-1.15.2"
build="$root/build/ps4-vpx"
prefix="$build/install"
mkdir -p "$root/.tools/downloads" "$build"
if [[ ! -f "$archive" ]]; then
 curl -fL --retry 3 'https://codeload.github.com/webmproject/libvpx/tar.gz/refs/tags/v1.15.2' -o "$archive.tmp"
 mv "$archive.tmp" "$archive"
fi
printf '%s  %s\n' 26fcd3db88045dee380e581862a6ef106f49b74b6396ee95c2993a260b4636aa "$archive" | sha256sum -c -
if [[ ! -d "$source_dir" ]]; then tar -xf "$archive" -C "$root/.tools"; fi
fingerprint=$( { sha256sum "$root/scripts/build-ps4-vpx.sh" "$root/compat/ps4/include/ps4-abi.h"; printf '%s\n' "$sdk"; clang --version; } | sha256sum | cut -d ' ' -f 1)
if [[ -f "$prefix/lib/libvpx.a" && -f "$build/.build-id" ]] && [[ $(cat "$build/.build-id") == "$fingerprint" ]]; then exit 0; fi
cd "$build"
if [[ -f Makefile ]]; then make clean; fi
# Generic C avoids host CPU detection/assembler assumptions in the PS4 ABI.
CC=clang CXX=clang++ AR=llvm-ar LD=clang NM=llvm-nm STRIP=llvm-strip \
 CFLAGS="--target=x86_64-pc-freebsd12-elf -O2 -fPIC -D__PS4__ -include $root/compat/ps4/include/ps4-abi.h -isysroot $sdk -isystem $sdk/include -mno-red-zone -fno-stack-protector" \
 LDFLAGS="--target=x86_64-pc-freebsd12-elf -fuse-ld=lld -nostdlib -Wl,-pie -Wl,--script=$sdk/link.x -L$sdk/lib $sdk/lib/crt1.o -lc -lkernel" \
 "$source_dir/configure" --target=generic-gnu --prefix="$prefix" \
 --enable-pic --enable-static --disable-shared --disable-multithread \
 --disable-vp8-encoder --disable-vp9-encoder --disable-runtime-cpu-detect \
 --disable-examples --disable-tools --disable-docs --disable-unit-tests \
 --disable-webm-io --disable-libyuv
make -j "${BUILD_JOBS:-12}" libvpx.a
mkdir -p "$prefix/lib" "$prefix/include/vpx" "$prefix/share/licenses"
cp libvpx.a "$prefix/lib/"
cp "$source_dir/vpx/"*.h "$prefix/include/vpx/"
cp "$source_dir/LICENSE" "$prefix/share/licenses/libvpx-LICENSE"
cp "$source_dir/PATENTS" "$prefix/share/licenses/libvpx-PATENTS"
printf '%s\n' "$fingerprint" > "$build/.build-id"
