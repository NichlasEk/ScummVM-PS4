#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [[ $# != 1 || ! -f "$1" ]]; then echo "Usage: $0 INPUT.mp3" >&2; exit 2; fi
sdk=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
export OO_PS4_TOOLCHAIN="$sdk"
"$root/scripts/build-ps4-mad.sh"
source_dir="$root/build/ps4-mad/libmad-0.15.1b"
build="$root/build/ps4-mad-smoke"
mkdir -p "$build/stage/sce_module" "$build/stage/sce_sys"
objects=()
for name in version fixed bit timer stream frame synth decoder layer12 layer3 huffman; do
 clang -O2 -fwrapv -DHAVE_CONFIG_H -DFPM_64BIT -I"$source_dir" -c "$source_dir/$name.c" -o "$build/$name.o"
 objects+=("$build/$name.o")
done
clang -O2 -I"$source_dir" "$root/tests/ps4/mad-smoke.c" "${objects[@]}" -o "$build/host-smoke"
"$build/host-smoke" "$1" "$build/expected.txt"
clang --target=x86_64-pc-freebsd12-elf -fPIC -O2 -D__PS4__ \
 -include "$root/compat/ps4/include/ps4-abi.h" -isysroot "$sdk" -isystem "$sdk/include" \
 -I"$root/build/ps4-mad/install/include" -c "$root/tests/ps4/mad-smoke.c" -o "$build/smoke.o"
ld.lld -m elf_x86_64 -pie --script "$sdk/link.x" --eh-frame-hdr \
 "$sdk/lib/crt1.o" "$build/smoke.o" "$root/build/ps4-mad/install/lib/libmad.a" \
 -L"$sdk/lib" -lc -lkernel -o "$build/smoke.elf"
"$sdk/bin/linux/create-fself" -in="$build/smoke.elf" -out="$build/smoke.oelf" --eboot "$build/stage/eboot.bin" --paid 0x3800000000000011
cp "$sdk/samples/SDL2/sce_module/"{libc.prx,libSceFios2.prx} "$build/stage/sce_module/"
cp "$root/build/scummvm-pkg/sce_sys/param.sfo" "$build/stage/sce_sys/"
cp "$1" "$build/stage/sample.mp3"
sha256sum "$1" > "$build/input.sha256"
cat "$build/expected.txt"
echo "Run $build/stage/eboot.bin in an isolated shadPS4 profile; compare data/mad-smoke.txt with $build/expected.txt."
