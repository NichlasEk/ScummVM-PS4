#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
sdk=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
export OO_PS4_TOOLCHAIN="$sdk"
"$root/scripts/build-ps4-gif.sh"
build="$root/build/ps4-gif-smoke"
mkdir -p "$build/stage/sce_module" "$build/stage/sce_sys"
python3 - "$build" <<'PY'
from pathlib import Path
from PIL import Image
import sys
b=Path(sys.argv[1]); p=b/'stage'
palette=[v for i in range(256) for v in (i,(i*3)%256,(i*7)%256)]
for name in ['palette','interlaced','local','transparent','offset']:
 size=(28,18) if name=='offset' else (32,24)
 im=Image.new('P',size); im.putpalette(palette)
 im.putdata([(x+y*17)%256 for y in range(size[1]) for x in range(size[0])])
 options=dict(optimize=False,interlace=name=='interlaced')
 if name in ['transparent','offset']: options['transparency']=0
 im.save(p/f'{name}.gif',**options)
 data=bytearray((p/f'{name}.gif').read_bytes()); pos=13+3*(2**((data[10]&7)+1))
 while data[pos]==0x21:
  pos+=2
  while data[pos]: pos+=1+data[pos]
  pos+=1
 assert data[pos]==0x2c
 assert bool(data[pos+9]&0x40)==(name=='interlaced')
 if name=='local':
  table=data[13:781]; data[pos+9]|=0x87
  data[pos+10:pos+10]=table
  del data[13:781]; data[10]&=0x7f
 if name=='offset':
  data[6:10]=bytes([32,0,24,0]); data[pos+1:pos+5]=bytes([2,0,3,0])
 (p/f'{name}.gif').write_bytes(data)
 with Image.open(p/f'{name}.gif') as decoded:
  assert decoded.size==(32,24)
  rgba=decoded.convert('RGBA').tobytes(); (p/f'{name}.rgba').write_bytes(rgba)
 expected=2166136261
 for value in rgba: expected=((expected^value)*16777619)&0xffffffff
 with (b/'expected.txt').open('w' if name=='palette' else 'a') as output:
  output.write(f'{name} rgba-fnv1a={expected:08x} samples=3072 PASS\n')
data=(p/'palette.gif').read_bytes()
(p/'truncated.gif').write_bytes(data[:len(data)//2]); (p/'invalid.gif').write_bytes(b'not a GIF')
with (b/'expected.txt').open('a') as output: output.write('truncated rejected\ninvalid rejected\nPASS\n')
PY
# Link the actual ScummVM GIFDecoder and Common/Graphics implementations. Only
# the entrypoint is replaced, so no SDL window or game data is needed.
export SCUMMVM_BUILD_DIR="$build/scummvm" SCUMMVM_ENGINES=qdengine
"$root/scripts/prepare-scummvm.sh"
"$root/scripts/configure-scummvm.sh" > "$build/configure.log" 2>&1
make -C "$SCUMMVM_BUILD_DIR" --eval="gif-smoke-object: ; \$(CXX) \$(CXXFLAGS) \$(CPPFLAGS) \$(INCLUDES) -DHAVE_CONFIG_H -I. -I$root/.tools/scummvm-src -c $root/tests/ps4/gif-smoke.cpp -o $build/smoke.o" gif-smoke-object
printf '\nLDFLAGS += -Wl,--wrap=main\nLIBS += %s\n' "$build/smoke.o" >> "$SCUMMVM_BUILD_DIR/config.mk"
rm -f "$SCUMMVM_BUILD_DIR/scummvm"
make -C "$SCUMMVM_BUILD_DIR" -j"${BUILD_JOBS:-12}" > "$build/build.log" 2>&1
"$sdk/bin/linux/create-fself" -in="$SCUMMVM_BUILD_DIR/scummvm" -out="$build/smoke.oelf" --eboot "$build/stage/eboot.bin" --paid 0x3800000000000011
cp "$sdk/samples/SDL2/sce_module/"{libc.prx,libSceFios2.prx} "$build/stage/sce_module/"
cp "$root/build/scummvm-pkg/sce_sys/param.sfo" "$build/stage/sce_sys/"
sha256sum "$build/stage/"*.gif "$build/stage/"*.rgba > "$build/input.sha256"
cat "$build/expected.txt"
