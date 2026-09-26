#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
sdk=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
export OO_PS4_TOOLCHAIN="$sdk"
"$root/scripts/build-ps4-mpeg2.sh"
build="$root/build/ps4-mpeg2-smoke"
prefix="$root/build/ps4-mpeg2/install"
src="$root/.tools/libmpeg2-0.5.1"
mkdir -p "$build/stage/sce_module" "$build/stage/sce_sys" "$build/host"
for name in clip420 clip422 interlaced; do
 format=yuv420p; extra=()
 if [[ $name == clip422 ]]; then format=yuv422p; fi
 if [[ $name == interlaced ]]; then extra=(-vf setfield=tff -flags +ilme+ildct); fi
 ffmpeg -v error -y -f lavfi -i 'testsrc2=size=162x98:rate=25:duration=2' \
  -pix_fmt "$format" -an -c:v mpeg2video -q:v 4 -g 12 -bf 2 "${extra[@]}" -f mpeg2video "$build/stage/$name.m2v"
 # Explicitly terminate the finite fixture (FFmpeg's elementary muxer may omit
 # sequence_end); trailing bytes allow streaming parsers to consume the code.
 python3 - "$build/stage/$name.m2v" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1]); p.write_bytes(p.read_bytes()+b'\x00\x00\x01\xb7'+bytes(8))
PY
 ffmpeg -v error -y -i "$build/stage/$name.m2v" -fps_mode passthrough \
  -pix_fmt "$format" -f rawvideo "$build/stage/$name.yuv"
done
python3 - "$build/stage" <<'PY'
from pathlib import Path
import json, subprocess, sys
p=Path(sys.argv[1])
for name, size in [('clip420',162*98*3//2),('clip422',162*98*2),('interlaced',162*98*3//2)]:
 assert (p/f'{name}.yuv').stat().st_size==size*50
 probe=json.loads(subprocess.check_output(['ffprobe','-v','error','-select_streams','v:0','-show_entries',
  'stream=codec_name,width,height,pix_fmt,field_order,r_frame_rate','-of','json',str(p/f'{name}.m2v')]))
 stream=probe['streams'][0]
 assert stream['codec_name']=='mpeg2video' and stream['r_frame_rate']=='25/1'
 assert stream['field_order']==('tt' if name=='interlaced' else 'progressive')
 assert stream['pix_fmt']==('yuv422p' if name=='clip422' else 'yuv420p')
 (p.parent/f'{name}-probe.json').write_text(json.dumps(probe,indent=2)+'\n')
data=(p/'clip420.m2v').read_bytes()
(p/'truncated.m2v').write_bytes(data[:len(data)//2])
(p/'invalid.m2v').write_bytes(b'not an MPEG stream')
PY
sha256sum "$build/stage/"*.m2v "$build/stage/"*.yuv > "$build/input.sha256"
objects=()
for unit in alloc header decode slice motion_comp idct cpu_accel cpu_state; do
 clang -O2 -g -fsanitize=address -fno-omit-frame-pointer -Werror \
  -I"$root/build/ps4-mpeg2" -I"$src/include" -c "$src/libmpeg2/$unit.c" -o "$build/host/$unit.o"
 objects+=("$build/host/$unit.o")
done
clang -O2 -g -fsanitize=address -Wall -Wextra -Werror \
 "$root/tests/ps4/mpeg2-smoke.c" -I"$prefix/include" "${objects[@]}" -o "$build/host-smoke"
"$build/host-smoke" "$build/stage" "$build/expected.txt"
clang --target=x86_64-pc-freebsd12-elf -fPIC -O2 -D__PS4__ -Wall -Wextra -Werror \
 -mno-red-zone -fno-stack-protector -include "$root/compat/ps4/include/ps4-abi.h" \
 -isysroot "$sdk" -isystem "$sdk/include" -I"$prefix/include" \
 -c "$root/tests/ps4/mpeg2-smoke.c" -o "$build/smoke.o"
ld.lld -m elf_x86_64 -pie --script "$sdk/link.x" --eh-frame-hdr \
 "$sdk/lib/crt1.o" "$build/smoke.o" "$prefix/lib/libmpeg2.a" \
 -L"$sdk/lib" -lc -lkernel -o "$build/smoke.elf"
"$sdk/bin/linux/create-fself" -in="$build/smoke.elf" -out="$build/smoke.oelf" --eboot "$build/stage/eboot.bin" --paid 0x3800000000000011
cp "$sdk/samples/SDL2/sce_module/"{libc.prx,libSceFios2.prx} "$build/stage/sce_module/"
cp "$root/build/scummvm-pkg/sce_sys/param.sfo" "$build/stage/sce_sys/"
cat "$build/expected.txt"
echo "Run $build/stage/eboot.bin in an isolated shadPS4 profile; compare data/mpeg2-smoke.txt with expected.txt."
