#!/usr/bin/env bash
# Build an isolated PS4 decoder test with generated, non-game media.
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
sdk=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
export OO_PS4_TOOLCHAIN="$sdk"
build="$root/build/ps4-codecs-probe"
stage="$build/stage"
for dep in flac vpx a52; do "$root/scripts/build-ps4-$dep.sh"; done
mkdir -p "$stage/sce_module"
ffmpeg -v error -y -f lavfi -i 'sine=frequency=440:sample_rate=48000:duration=0.1' -ac 2 -f s16le "$build/sine.pcm"
ffmpeg -v error -y -f s16le -ar 48000 -ac 2 -i "$build/sine.pcm" -c:a flac "$build/sine.flac"
ffmpeg -v error -y -f s16le -ar 48000 -ac 2 -i "$build/sine.pcm" -c:a ac3 -b:a 192k "$build/sine.ac3"
ffmpeg -v error -y -f lavfi -i 'color=c=red:s=64x48:r=1' -frames:v 1 -c:v libvpx -f ivf "$build/red.ivf"
python3 - "$build" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1])
with (p/'codec-fixtures.h').open('w') as f:
 for name, filename in [('pcm','sine.pcm'),('flac','sine.flac'),('ac3','sine.ac3'),('ivf','red.ivf')]:
  f.write('static const unsigned char fixture_'+name+'[] = {' + ','.join(map(str,(p/filename).read_bytes())) + '};\n')
PY
flags=(--target=x86_64-pc-freebsd12-elf -O2 -fPIC -D__PS4__
 -include "$root/compat/ps4/include/ps4-abi.h" -isysroot "$sdk" -isystem "$sdk/include"
 -mno-red-zone -fno-stack-protector -Wall -Wextra -I"$build")
libs=()
for dep in flac vpx a52; do flags+=(-I"$root/build/ps4-$dep/install/include"); done
libs+=("$root/build/ps4-flac/install/lib/libFLAC.a" "$root/build/ps4-vpx/install/lib/libvpx.a" "$root/build/ps4-a52/install/lib/liba52.a")
clang "${flags[@]}" -c "$root/tests/ps4/codecs-probe.c" -o "$build/probe.o"
ld.lld -m elf_x86_64 -pie --script "$sdk/link.x" --eh-frame-hdr \
 "$sdk/lib/crt1.o" "$build/probe.o" "${libs[@]}" -L"$sdk/lib" -lc -lkernel -o "$build/probe.elf"
"$sdk/bin/linux/create-fself" -in="$build/probe.elf" -out="$build/probe.oelf" \
 --eboot "$stage/eboot.bin" --paid 0x3800000000000011
cp "$sdk/samples/SDL2/sce_module/"{libc.prx,libSceFios2.prx} "$stage/sce_module/"
echo "Run in shadPS4: $stage/eboot.bin"
echo 'Expected stdout: PS4 CODECS: FLAC exact PCM=PASS VP8 frame=PASS A52 audio=PASS'
