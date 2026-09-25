#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
sdk=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
export OO_PS4_TOOLCHAIN="$sdk"
"$root/scripts/build-ps4-freetype.sh"
source_dir="$root/.tools/freetype-2.14.3"
build="$root/build/ps4-freetype-smoke"
mkdir -p "$build/stage/sce_module" "$build/stage/sce_sys"
if [[ $# != 1 ]]; then echo "Usage: $0 FONT.ttf" >&2; exit 2; fi
cp "$1" "$build/stage/test.ttf"
sha256sum "$build/stage/test.ttf" > "$build/input.sha256"
cmake -S "$source_dir" -B "$build/host" -DBUILD_SHARED_LIBS=OFF \
 -DFT_DISABLE_ZLIB=ON -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_PNG=ON \
 -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BROTLI=ON
cmake --build "$build/host" -j "${BUILD_JOBS:-12}"
cc "$root/tests/ps4/freetype-smoke.c" -I"$build/host/include" -I"$source_dir/include" "$build/host/libfreetype.a" -lm -o "$build/host-smoke"
"$build/host-smoke" "$build/stage/test.ttf" "$build/expected.txt"
clang --target=x86_64-pc-freebsd12-elf -fPIC -O2 -D__PS4__ \
 -include "$root/compat/ps4/include/ps4-abi.h" -isysroot "$sdk" -isystem "$sdk/include" \
 -I"$root/build/ps4-freetype/install/include/freetype2" -c "$root/tests/ps4/freetype-smoke.c" -o "$build/smoke.o"
ld.lld -m elf_x86_64 -pie --script "$sdk/link.x" --eh-frame-hdr \
 "$sdk/lib/crt1.o" "$build/smoke.o" "$root/build/ps4-freetype/install/lib/libfreetype.a" \
 -L"$sdk/lib" -lc -lkernel -o "$build/smoke.elf"
"$sdk/bin/linux/create-fself" -in="$build/smoke.elf" -out="$build/smoke.oelf" --eboot "$build/stage/eboot.bin" --paid 0x3800000000000011
cp "$sdk/samples/SDL2/sce_module/"{libc.prx,libSceFios2.prx} "$build/stage/sce_module/"
cp "$root/build/scummvm-pkg/sce_sys/param.sfo" "$build/stage/sce_sys/"
cat "$build/expected.txt"
echo "Run $build/stage/eboot.bin in an isolated shadPS4 profile; compare data/freetype-smoke.txt with expected.txt."
