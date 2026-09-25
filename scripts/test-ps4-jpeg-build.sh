#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
sdk=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
export OO_PS4_TOOLCHAIN="$sdk"
"$root/scripts/build-ps4-jpeg.sh"
source_dir="$root/.tools/libjpeg-turbo-3.2.0"
build="$root/build/ps4-jpeg-smoke"
mkdir -p "$build/stage/sce_module" "$build/stage/sce_sys"
cmake -S "$source_dir" -B "$build/host" -DENABLE_SHARED=OFF -DWITH_SIMD=OFF -DWITH_TURBOJPEG=OFF -DWITH_TOOLS=OFF -DWITH_TESTS=OFF
cmake --build "$build/host" --target jpeg-static -j "${BUILD_JOBS:-12}"
cc "$root/tests/ps4/jpeg-smoke.c" -I"$source_dir/src" -I"$build/host" "$build/host/libjpeg.a" -lm -o "$build/host-smoke"
"$build/host-smoke" "$source_dir/testimages" "$build/expected.txt"
clang --target=x86_64-pc-freebsd12-elf -fPIC -O2 -D__PS4__ \
 -include "$root/compat/ps4/include/ps4-abi.h" -isysroot "$sdk" -isystem "$sdk/include" \
 -I"$root/build/ps4-jpeg/install/include" -c "$root/tests/ps4/jpeg-smoke.c" -o "$build/smoke.o"
ld.lld -m elf_x86_64 -pie --script "$sdk/link.x" --eh-frame-hdr \
 "$sdk/lib/crt1.o" "$build/smoke.o" "$root/build/ps4-jpeg/install/lib/libjpeg.a" \
 -L"$sdk/lib" -lc -lkernel -o "$build/smoke.elf"
"$sdk/bin/linux/create-fself" -in="$build/smoke.elf" -out="$build/smoke.oelf" --eboot "$build/stage/eboot.bin" --paid 0x3800000000000011
cp "$sdk/samples/SDL2/sce_module/"{libc.prx,libSceFios2.prx} "$build/stage/sce_module/"
cp "$root/build/scummvm-pkg/sce_sys/param.sfo" "$build/stage/sce_sys/"
cp "$source_dir/testimages/"{testorig.jpg,testimgint.jpg,testimgari.jpg} "$build/stage/"
cat "$build/expected.txt"
echo "Run $build/stage/eboot.bin in an isolated shadPS4 profile. Compare its data/jpeg-smoke.txt with $build/expected.txt."
