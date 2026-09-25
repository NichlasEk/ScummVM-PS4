#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
sdk=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
export OO_PS4_TOOLCHAIN="$sdk"
build="$root/build/ps4-opengl-probe"
stage="$build/stage"
content=IV0000-SCVM00005_00-SCUMMVMGLPROBE01
mkdir -p "$stage/sce_sys/about" "$stage/sce_module" "$root/dist" "$root/artifacts/opengl"
# Verify the pixel oracle on a host EGL pbuffer first. This is not a PS4 result.
clang -std=gnu11 -O2 -Wall -Wextra "$root/tests/ps4/opengl-probe.c" \
 -lEGL -lGLESv2 -ldl -o "$build/host-probe"
EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1 "$build/host-probe" \
 > "$root/artifacts/opengl/host-oracle.log" 2>&1
flags=(--target=x86_64-pc-freebsd12-elf -std=gnu11 -O2 -fPIC -D__PS4__
 -include "$root/compat/ps4/include/ps4-abi.h" -isysroot "$sdk" -isystem "$sdk/include"
 -I"$root/compat/ps4/graphics" -mno-red-zone -fno-stack-protector -Wall -Wextra)
clang "${flags[@]}" -c "$root/compat/ps4/graphics/piglet-context.c" -o "$build/piglet-context.o"
clang "${flags[@]}" -c "$root/tests/ps4/opengl-probe.c" -o "$build/probe.o"
ld.lld -m elf_x86_64 -pie --script "$sdk/link.x" --eh-frame-hdr \
 "$sdk/lib/crt1.o" "$build/probe.o" "$build/piglet-context.o" \
 -L"$sdk/lib" -lc -lkernel -o "$build/probe.elf"
python3 - "$build/probe.elf" <<'PY'
import re, subprocess, sys
needed = re.findall(r'\(NEEDED\).*?\[(.*?)\]', subprocess.check_output(['readelf', '-d', sys.argv[1]], text=True))
if set(needed) != {'libkernel.so'}:
 sys.exit('Unexpected imports (probe must boot without Piglet): ' + repr(needed))
print('Probe imports:', needed)
PY
"$sdk/bin/linux/create-fself" -in="$build/probe.elf" -out="$build/probe.oelf" \
 --eboot "$stage/eboot.bin" --paid 0x3800000000000011
cp "$sdk/samples/SDL2/sce_module/"{libc.prx,libSceFios2.prx} "$stage/sce_module/"
cp "$sdk/samples/piglet/sce_sys/about/right.sprx" "$stage/sce_sys/about/"
cp "$root/assets/icon0.png" "$stage/sce_sys/"
pkgtool="$sdk/bin/linux/PkgTool.Core"
sfo="$stage/sce_sys/param.sfo"
if [[ -f "$sfo" ]]; then mv "$sfo" "$sfo.previous"; fi
"$pkgtool" sfo_new "$sfo"
for pair in APP_TYPE:1 ATTRIBUTE:0 DOWNLOAD_DATA_SIZE:0 SYSTEM_VER:0; do
 "$pkgtool" sfo_setentry "$sfo" "${pair%%:*}" --type Integer --maxsize 4 --value "${pair#*:}"
done
for key in APP_VER VERSION; do "$pkgtool" sfo_setentry "$sfo" "$key" --type Utf8 --maxsize 8 --value 1.00; done
"$pkgtool" sfo_setentry "$sfo" CATEGORY --type Utf8 --maxsize 4 --value gd
"$pkgtool" sfo_setentry "$sfo" CONTENT_ID --type Utf8 --maxsize 48 --value "$content"
"$pkgtool" sfo_setentry "$sfo" TITLE_ID --type Utf8 --maxsize 12 --value SCVM00005
"$pkgtool" sfo_setentry "$sfo" TITLE --type Utf8 --maxsize 128 --value 'ScummVM PS4 OpenGL Probe'
cd "$stage"
"$sdk/bin/linux/create-gp4" -out pkg.gp4 --content-id="$content" \
 --files 'eboot.bin sce_sys/param.sfo sce_sys/icon0.png sce_sys/about/right.sprx sce_module/libc.prx sce_module/libSceFios2.prx'
"$pkgtool" pkg_build pkg.gp4 .
cp "$content.pkg" "$root/dist/ScummVM-PS4-OpenGL-Probe.pkg"
cd "$root/dist"
sha256sum ScummVM-PS4-OpenGL-Probe.pkg > ScummVM-PS4-OpenGL-Probe.pkg.sha256
cat ScummVM-PS4-OpenGL-Probe.pkg.sha256
echo 'Target report: /data/scummvm-opengl-probe.log (exit 0 = pass, exit 2 = not ready).'
