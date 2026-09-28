#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
sdk=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
archive="$root/.tools/downloads/a52dec-0.7.4.tar.gz"
source_dir="$root/.tools/a52dec-0.7.4"
build="$root/build/ps4-a52"
prefix="$build/install"
mkdir -p "$root/.tools/downloads" "$build"
if [[ ! -f "$archive" ]]; then
 curl -fL --retry 3 'https://deb.debian.org/debian/pool/main/a/a52dec/a52dec_0.7.4.orig.tar.gz' -o "$archive.tmp"
 mv "$archive.tmp" "$archive"
fi
printf '%s  %s\n' a21d724ab3b3933330194353687df82c475b5dfb997513eef4c25de6c865ec33 "$archive" | sha256sum -c -
if [[ ! -d "$source_dir" ]]; then tar -xf "$archive" -C "$root/.tools"; fi
fingerprint=$( { sha256sum "$root/scripts/build-ps4-a52.sh" "$root/compat/ps4/include/ps4-abi.h"; printf '%s\n' "$sdk"; clang --version; } | sha256sum | cut -d ' ' -f 1)
if [[ -f "$prefix/lib/liba52.a" && -f "$build/.build-id" ]] && [[ $(cat "$build/.build-id") == "$fingerprint" ]]; then exit 0; fi
# Decoder only, portable C and float samples (the ScummVM ABI).
printf '%s\n' '#define RETSIGTYPE void' > "$build/config.h"
flags=(--target=x86_64-pc-freebsd12-elf -O2 -fPIC -fno-strict-aliasing
 -D__PS4__ -include "$root/compat/ps4/include/ps4-abi.h"
 -isysroot "$sdk" -isystem "$sdk/include" -mno-red-zone -fno-stack-protector
 -I"$build" -I"$source_dir/include" -I"$source_dir/liba52")
objects=()
for unit in bitstream imdct bit_allocate parse downmix; do
 clang "${flags[@]}" -c "$source_dir/liba52/$unit.c" -o "$build/$unit.o"
 objects+=("$build/$unit.o")
done
mkdir -p "$prefix/lib" "$prefix/include/a52dec" "$prefix/share/licenses"
llvm-ar rcs "$prefix/lib/liba52.a" "${objects[@]}"
cp "$source_dir/include/"{a52.h,mm_accel.h} "$prefix/include/a52dec/"
cp "$source_dir/COPYING" "$prefix/share/licenses/liba52-COPYING"
printf '%s\n' "$fingerprint" > "$build/.build-id"
