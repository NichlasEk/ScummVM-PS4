#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
sdk=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
archive="$root/.tools/downloads/libmpeg2-0.5.1.tar.gz"
source_dir="$root/.tools/libmpeg2-0.5.1"
build="$root/build/ps4-mpeg2"
prefix="$build/install"
mkdir -p "$root/.tools/downloads" "$build" "$prefix/lib" "$prefix/include/mpeg2dec" "$prefix/share/licenses"
if [[ ! -f "$archive" ]]; then
 curl -fL --retry 3 https://download.videolan.org/pub/contrib/libmpeg2/libmpeg2-0.5.1.tar.gz -o "$archive.tmp"
 mv "$archive.tmp" "$archive"
fi
printf '%s  %s\n' dee22e893cb5fc2b2b6ebd60b88478ab8556cb3b93f9a0d7ce8f3b61851871d4 "$archive" | sha256sum -c -
if [[ ! -d "$source_dir" ]]; then tar -xf "$archive" -C "$root/.tools"; fi
fingerprint=$( { sha256sum "$root/scripts/build-ps4-mpeg2.sh" "$root/compat/ps4/include/ps4-abi.h"; printf '%s\n' "$sdk"; clang --version; } | sha256sum | cut -d ' ' -f 1)
if [[ -f "$prefix/lib/libmpeg2.a" && -f "$build/.build-id" ]] && [[ $(cat "$build/.build-id") == "$fingerprint" ]]; then exit 0; fi
# Portable decoder only. No architecture macros: use C IDCT/motion compensation,
# no CPU probing, assembly, player, encoder, or unnecessary RGB conversion library.
cat > "$build/config.h" <<'CONFIG'
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_BUILTIN_EXPECT 1
#define ATTRIBUTE_ALIGNED_MAX 64
CONFIG
objects=()
for unit in alloc header decode slice motion_comp idct cpu_accel cpu_state; do
 clang --target=x86_64-pc-freebsd12-elf -std=gnu99 -O2 -fPIC -Werror \
  -D__PS4__ -include "$root/compat/ps4/include/ps4-abi.h" \
  -isysroot "$sdk" -isystem "$sdk/include" -mno-red-zone -fno-stack-protector \
  -I"$build" -I"$source_dir/include" -c "$source_dir/libmpeg2/$unit.c" -o "$build/$unit.o"
 objects+=("$build/$unit.o")
done
rm -f "$prefix/lib/libmpeg2.a"
llvm-ar rcs "$prefix/lib/libmpeg2.a" "${objects[@]}"
cp "$source_dir/include/mpeg2.h" "$prefix/include/mpeg2dec/"
cp "$source_dir/COPYING" "$prefix/share/licenses/libmpeg2-COPYING"
printf '%s\n' "$fingerprint" > "$build/.build-id"
