#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
sdk=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
"$root/scripts/build-ps4-vorbis.sh"
version=1.2.0
archive="$root/.tools/downloads/libtheora-$version.tar.gz"
source_dir="$root/.tools/libtheora-$version"
build="$root/build/ps4-theora"
prefix="$build/install"
ogg="$root/build/ps4-vorbis/install"
mkdir -p "$root/.tools/downloads" "$build"
if [[ ! -f "$archive" ]]; then
 curl -fL --retry 3 "https://downloads.xiph.org/releases/theora/libtheora-$version.tar.gz" -o "$archive.tmp"
 mv "$archive.tmp" "$archive"
fi
printf '%s  %s\n' 279327339903b544c28a92aeada7d0dcfd0397b59c2f368cc698ac56f515906e "$archive" | sha256sum -c -
if [[ ! -d "$source_dir" ]]; then tar -xf "$archive" -C "$root/.tools"; fi
fingerprint=$( { sha256sum "$root/scripts/build-ps4-theora.sh" "$root/compat/ps4/include/ps4-abi.h" "$root/build/ps4-vorbis/.build-id"; printf '%s\n' "$sdk"; clang --version; } | sha256sum | cut -d ' ' -f 1)
if [[ -f "$prefix/lib/libtheoradec.a" && -f "$build/.build-id" ]] && [[ $(cat "$build/.build-id") == "$fingerprint" ]]; then exit 0; fi
cd "$build"
CC=clang AR=llvm-ar RANLIB=llvm-ranlib NM=llvm-nm \
 CFLAGS="--target=x86_64-pc-freebsd12-elf -O2 -fPIC -D__PS4__ -include $root/compat/ps4/include/ps4-abi.h -isysroot $sdk -isystem $sdk/include -mno-red-zone -fno-stack-protector" \
 LDFLAGS="--target=x86_64-pc-freebsd12-elf -fuse-ld=lld -nostdlib -Wl,-pie -Wl,--script=$sdk/link.x -L$sdk/lib $sdk/lib/crt1.o" \
 LIBS="-lc -lkernel" PKG_CONFIG_LIBDIR="$ogg/lib/pkgconfig" PKG_CONFIG_PATH= \
 "$source_dir/configure" --host=x86_64-pc-freebsd --prefix="$prefix" \
 --disable-shared --enable-static --with-pic --disable-asm --disable-encode \
 --disable-examples --disable-doc --disable-spec --disable-oggtest --disable-vorbistest \
 --with-ogg="$ogg" --with-vorbis="$ogg"
# Only the decoder is needed; do not install an encoder or compatibility library.
make -C lib -j "${BUILD_JOBS:-12}" libtheoradec.la
mkdir -p "$prefix/lib" "$prefix/include/theora" "$prefix/share/licenses"
cp lib/.libs/libtheoradec.a "$prefix/lib/"
cp "$source_dir/include/theora/"{codec.h,theoradec.h} "$prefix/include/theora/"
cp "$source_dir/COPYING" "$prefix/share/licenses/libtheora-COPYING"
printf '%s\n' "$fingerprint" > "$build/.build-id"
