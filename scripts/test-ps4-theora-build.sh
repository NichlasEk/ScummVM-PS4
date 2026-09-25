#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
sdk=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
export OO_PS4_TOOLCHAIN="$sdk"
"$root/scripts/build-ps4-theora.sh"
build="$root/build/ps4-theora-smoke"
prefix="$root/build/ps4-theora/install"
ogg="$root/build/ps4-vorbis/install"
mkdir -p "$build/stage/sce_module" "$build/stage/sce_sys" "$build/host"
for format in 420 422 444; do
 # Non-macroblock-aligned dimensions also test picture cropping.
 ffmpeg -v error -y -f lavfi -i 'testsrc2=size=162x98:rate=12:duration=2' \
  -pix_fmt "yuv${format}p" -an -c:v libtheora -q:v 6 -g 6 "$build/stage/clip$format.ogv"
 ffmpeg -v error -y -i "$build/stage/clip$format.ogv" -fps_mode passthrough \
  -pix_fmt "yuv${format}p" -f rawvideo "$build/clip$format.yuv"
done
python3 - "$build" <<'PY'
from pathlib import Path
import sys
b = Path(sys.argv[1]); expected = []
for fmt, enum, size in [(420, 0, 162*98*3//2), (422, 2, 162*98*2), (444, 3, 162*98*3)]:
 data = (b/f'clip{fmt}.yuv').read_bytes()
 assert len(data) == size * 24
 h = 2166136261
 for v in data: h = ((h ^ v) * 16777619) & 0xffffffff
 expected.append(f'clip{fmt}.ogv 162x98 fmt={enum} frames=24 yuv-fnv1a={h:08x} end=2.000')
data = (b/'stage/clip420.ogv').read_bytes()
(b/'stage/truncated.ogv').write_bytes(data[:len(data)//2])
(b/'stage/invalid.ogv').write_bytes(b'not an Ogg stream')
expected += ['truncated.ogv rejected', 'invalid.ogv rejected', 'PASS']
(b/'expected.txt').write_text('\n'.join(expected)+'\n')
PY
sha256sum "$build/stage/"*.ogv > "$build/input.sha256"
# A native build of the same portable decoder is compared to FFmpeg first.
(cd "$build/host" && "$root/.tools/libtheora-1.2.0/configure" \
 --disable-shared --enable-static --disable-asm --disable-encode \
 --disable-examples --disable-doc --disable-spec)
make -C "$build/host/lib" -j "${BUILD_JOBS:-12}" libtheoradec.la
clang -O2 -Wall -Wextra "$root/tests/ps4/theora-smoke.c" -I"$root/.tools/libtheora-1.2.0/include" \
 "$build/host/lib/.libs/libtheoradec.a" -logg -lm -o "$build/host-smoke"
"$build/host-smoke" "$build/stage" "$build/host-result.txt"
diff -u "$build/expected.txt" "$build/host-result.txt"
clang --target=x86_64-pc-freebsd12-elf -fPIC -O2 -D__PS4__ \
 -include "$root/compat/ps4/include/ps4-abi.h" -isysroot "$sdk" -isystem "$sdk/include" \
 -I"$prefix/include" -I"$ogg/include" -c "$root/tests/ps4/theora-smoke.c" -o "$build/smoke.o"
ld.lld -m elf_x86_64 -pie --script "$sdk/link.x" --eh-frame-hdr \
 "$sdk/lib/crt1.o" "$build/smoke.o" "$prefix/lib/libtheoradec.a" "$ogg/lib/libogg.a" \
 -L"$sdk/lib" -lc -lkernel -o "$build/smoke.elf"
"$sdk/bin/linux/create-fself" -in="$build/smoke.elf" -out="$build/smoke.oelf" --eboot "$build/stage/eboot.bin" --paid 0x3800000000000011
cp "$sdk/samples/SDL2/sce_module/"{libc.prx,libSceFios2.prx} "$build/stage/sce_module/"
cp "$root/build/scummvm-pkg/sce_sys/param.sfo" "$build/stage/sce_sys/"
cat "$build/expected.txt"
echo "Run $build/stage/eboot.bin in an isolated shadPS4 profile; compare data/theora-smoke.txt with expected.txt."
