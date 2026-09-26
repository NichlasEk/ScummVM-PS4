#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
sdk=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
version=6.1.3
archive="$root/.tools/downloads/giflib-$version.tar.gz"
source_dir="$root/.tools/giflib-$version"
build="$root/build/ps4-gif"
prefix="$build/install"
mkdir -p "$root/.tools/downloads" "$build" "$prefix/lib" "$prefix/include" "$prefix/share/licenses"
if [[ ! -f "$archive" ]]; then
 curl -fL --retry 3 "https://downloads.sourceforge.net/project/giflib/giflib-6.x/giflib-$version.tar.gz" -o "$archive.tmp"
 mv "$archive.tmp" "$archive"
fi
printf '%s  %s\n' b65b66b99f0424b93525f987386f22fc5efb9da2bfc92ad4a532249aaffbab0e "$archive" | sha256sum -c -
if [[ ! -d "$source_dir" ]]; then tar -xf "$archive" -C "$root/.tools"; fi
fingerprint=$( { sha256sum "$root/scripts/build-ps4-gif.sh" "$root/compat/ps4/include/ps4-abi.h"; printf '%s\n' "$sdk"; clang --version; } | sha256sum | cut -d ' ' -f 1)
if [[ -f "$prefix/lib/libgif.a" && -f "$build/.build-id" ]] && [[ $(cat "$build/.build-id") == "$fingerprint" ]]; then exit 0; fi
# Decoder and shared allocation helpers only; use the bundled overflow-checked
# reallocarray implementation, keeping encoder/CLI/font code out of the port.
objects=()
for unit in dgif_lib gifalloc gif_err openbsd-reallocarray; do
 clang --target=x86_64-pc-freebsd12-elf -std=gnu99 -O2 -fPIC -Werror \
  -D__PS4__ -include "$root/compat/ps4/include/ps4-abi.h" \
  -isysroot "$sdk" -isystem "$sdk/include" -mno-red-zone -fno-stack-protector \
  -I"$source_dir" -c "$source_dir/$unit.c" -o "$build/$unit.o"
 objects+=("$build/$unit.o")
done
rm -f "$prefix/lib/libgif.a"
llvm-ar rcs "$prefix/lib/libgif.a" "${objects[@]}"
cp "$source_dir/gif_lib.h" "$prefix/include/"
cp "$source_dir/COPYING" "$prefix/share/licenses/giflib-COPYING"
printf '%s\n' "$fingerprint" > "$build/.build-id"
