#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
sdk=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
export OO_PS4_TOOLCHAIN="$sdk"
"$root/scripts/build-ps4-png.sh"
build="$root/build/ps4-png-smoke"
prefix="$root/build/ps4-png/install"
mkdir -p "$build/stage/sce_module" "$build/stage/sce_sys"
python3 "$root/tests/ps4/make-png-fixtures.py" "$build/stage"
sha256sum "$build/stage/"*.png > "$build/input.sha256"
cmake -S "$root/.tools/zlib-1.3.2" -B "$build/host-zlib" \
 -DZLIB_BUILD_SHARED=OFF -DZLIB_BUILD_TESTING=OFF \
 -DCMAKE_INSTALL_PREFIX="$build/host-install" -DCMAKE_INSTALL_LIBDIR=lib
cmake --build "$build/host-zlib" -j "${BUILD_JOBS:-12}"
cmake --install "$build/host-zlib"
cmake -S "$root/.tools/libpng-1.6.58" -B "$build/host-png" \
 -DZLIB_INCLUDE_DIR="$build/host-install/include" -DZLIB_LIBRARY="$build/host-install/lib/libz.a" \
 -DPNG_SHARED=OFF -DPNG_TESTS=OFF -DPNG_TOOLS=OFF -DPNG_HARDWARE_OPTIMIZATIONS=OFF
cmake --build "$build/host-png" -j "${BUILD_JOBS:-12}"
clang -O2 -Wall -Wextra "$root/tests/ps4/png-smoke.c" -I"$root/.tools/libpng-1.6.58" \
 -I"$build/host-png" "$build/host-png/libpng16.a" "$build/host-install/lib/libz.a" -lm -o "$build/host-smoke"
"$build/host-smoke" "$build/stage" "$build/host-result.txt"
diff -u "$build/expected.txt" "$build/host-result.txt"
clang --target=x86_64-pc-freebsd12-elf -fPIC -O2 -D__PS4__ \
 -include "$root/compat/ps4/include/ps4-abi.h" -isysroot "$sdk" -isystem "$sdk/include" \
 -I"$prefix/include" -c "$root/tests/ps4/png-smoke.c" -o "$build/smoke.o"
ld.lld -m elf_x86_64 -pie --script "$sdk/link.x" --eh-frame-hdr \
 "$sdk/lib/crt1.o" "$build/smoke.o" "$prefix/lib/"{libpng16.a,libz.a} \
 -L"$sdk/lib" -lc -lkernel -o "$build/smoke.elf"
"$sdk/bin/linux/create-fself" -in="$build/smoke.elf" -out="$build/smoke.oelf" --eboot "$build/stage/eboot.bin" --paid 0x3800000000000011
cp "$sdk/samples/SDL2/sce_module/"{libc.prx,libSceFios2.prx} "$build/stage/sce_module/"
cp "$root/build/scummvm-pkg/sce_sys/param.sfo" "$build/stage/sce_sys/"
echo "Host pixels match the independent oracle for all 30 images; bad inputs rejected."
echo "Run $build/stage/eboot.bin in an isolated shadPS4 profile; compare data/png-smoke.txt with expected.txt."
