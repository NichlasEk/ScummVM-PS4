#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
sdk=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
export OO_PS4_TOOLCHAIN="$sdk"
"$root/scripts/build-ps4-vorbis.sh"
build="$root/build/ps4-vorbis-smoke"
prefix="$root/build/ps4-vorbis/install"
mkdir -p "$build/stage/sce_module" "$build/stage/sce_sys"
# Synthetic fixtures contain no game audio. Retain hashes of the exact inputs.
ffmpeg -v error -y -f lavfi -i 'sine=frequency=440:sample_rate=22050:duration=2' -c:a libvorbis -q:a 4 "$build/stage/mono.ogg"
ffmpeg -v error -y -f lavfi -i 'sine=frequency=317:sample_rate=44100:duration=3' -f lavfi -i 'sine=frequency=911:sample_rate=44100:duration=3' -filter_complex '[0:a][1:a]amerge=inputs=2' -c:a libvorbis -q:a 4 "$build/stage/stereo.ogg"
sha256sum "$build/stage/"*.ogg > "$build/input.sha256"
cmake -S "$root/.tools/libogg-1.3.6" -B "$build/host-ogg" -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DINSTALL_DOCS=OFF -DCMAKE_INSTALL_PREFIX="$build/host-install" -DCMAKE_INSTALL_LIBDIR=lib
cmake --build "$build/host-ogg" -j "${BUILD_JOBS:-12}"
cmake --install "$build/host-ogg"
cmake -S "$root/.tools/libvorbis-1.3.7" -B "$build/host-vorbis" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS=-ffp-contract=off -DBUILD_SHARED_LIBS=OFF -DOGG_INCLUDE_DIR="$build/host-install/include" -DOGG_LIBRARY="$build/host-install/lib/libogg.a"
cmake --build "$build/host-vorbis" -j "${BUILD_JOBS:-12}"
clang -O2 "$root/tests/ps4/vorbis-smoke.c" -I"$build/host-install/include" -I"$root/.tools/libvorbis-1.3.7/include" "$build/host-vorbis/lib/"{libvorbisfile.a,libvorbis.a} "$build/host-install/lib/libogg.a" -lm -o "$build/host-smoke"
"$build/host-smoke" "$build/stage" "$build/expected.txt"
clang --target=x86_64-pc-freebsd12-elf -fPIC -O2 -D__PS4__ -include "$root/compat/ps4/include/ps4-abi.h" -isysroot "$sdk" -isystem "$sdk/include" -I"$prefix/include" -c "$root/tests/ps4/vorbis-smoke.c" -o "$build/smoke.o"
ld.lld -m elf_x86_64 -pie --script "$sdk/link.x" --eh-frame-hdr "$sdk/lib/crt1.o" "$build/smoke.o" "$prefix/lib/"{libvorbisfile.a,libvorbis.a,libogg.a} -L"$sdk/lib" -lc -lkernel -o "$build/smoke.elf"
"$sdk/bin/linux/create-fself" -in="$build/smoke.elf" -out="$build/smoke.oelf" --eboot "$build/stage/eboot.bin" --paid 0x3800000000000011
cp "$sdk/samples/SDL2/sce_module/"{libc.prx,libSceFios2.prx} "$build/stage/sce_module/"
cp "$root/build/scummvm-pkg/sce_sys/param.sfo" "$build/stage/sce_sys/"
cat "$build/expected.txt"
echo "Run $build/stage/eboot.bin in an isolated shadPS4 profile; compare data/vorbis-smoke.txt with expected.txt."
