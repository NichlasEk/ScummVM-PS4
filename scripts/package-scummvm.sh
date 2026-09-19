#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
export OO_PS4_TOOLCHAIN=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
sdk=$OO_PS4_TOOLCHAIN
stage="$root/build/scummvm-pkg"
dist="$root/dist"
content_id=IV0000-SCVM00002_00-SCUMMVMPS4000001
# Fail before packaging if cross-compilation accidentally picks up host libraries.
python3 - "$root/build/scummvm/scummvm" <<'CHECK'
import re, subprocess, sys
needed = re.findall(r'\(NEEDED\).*?\[(.*?)\]', subprocess.check_output(['readelf', '-d', sys.argv[1]], text=True))
allowed = {'libkernel.so', 'libSceUserService.so', 'libSceVideoOut.so',
           'libSceAudioOut.so', 'libScePad.so', 'libSceSysmodule.so'}
unexpected = set(needed) - allowed
if unexpected:
    sys.exit('Unexpected non-target dependencies: ' + ', '.join(sorted(unexpected)))
print('PS4 imports:', ', '.join(needed))
CHECK
mkdir -p "$stage/sce_sys/about" "$stage/sce_module" "$stage/data" "$stage/games" "$dist"
"$sdk/bin/linux/create-fself" -in="$root/build/scummvm/scummvm" \
 -out="$root/build/scummvm/scummvm.oelf" --eboot "$stage/eboot.bin" --paid 0x3800000000000011
cp "$sdk/samples/SDL2/sce_module/"{libc.prx,libSceFios2.prx} "$stage/sce_module/"
cp "$sdk/samples/piglet/sce_sys/about/right.sprx" "$stage/sce_sys/about/"
cp "$sdk/samples/SDL2/sce_sys/icon0.png" "$stage/sce_sys/"
cp "$root/.tools/scummvm-src/COPYING" "$stage/data/COPYING"
printf '%s\n' 'Place legally obtained SCUMM game data in subdirectories here.' > "$stage/games/README.txt"
pkgtool="$sdk/bin/linux/PkgTool.Core"
sfo="$stage/sce_sys/param.sfo"
# sfo_new expects a new path.
if [[ -e "$sfo" ]]; then mv "$sfo" "$sfo.previous"; fi
"$pkgtool" sfo_new "$sfo"
for pair in APP_TYPE:1 ATTRIBUTE:0 DOWNLOAD_DATA_SIZE:0 SYSTEM_VER:0; do
 "${pkgtool}" sfo_setentry "$sfo" "${pair%%:*}" --type Integer --maxsize 4 --value "${pair#*:}"
done
"$pkgtool" sfo_setentry "$sfo" APP_VER --type Utf8 --maxsize 8 --value 1.00
"$pkgtool" sfo_setentry "$sfo" VERSION --type Utf8 --maxsize 8 --value 1.00
"$pkgtool" sfo_setentry "$sfo" CATEGORY --type Utf8 --maxsize 4 --value gd
"$pkgtool" sfo_setentry "$sfo" CONTENT_ID --type Utf8 --maxsize 48 --value "$content_id"
"$pkgtool" sfo_setentry "$sfo" TITLE_ID --type Utf8 --maxsize 12 --value SCVM00002
"$pkgtool" sfo_setentry "$sfo" TITLE --type Utf8 --maxsize 128 --value 'ScummVM PS4'
cd "$stage"
"$sdk/bin/linux/create-gp4" -out pkg.gp4 --content-id="$content_id" \
 --files 'eboot.bin sce_sys/param.sfo sce_sys/icon0.png sce_sys/about/right.sprx sce_module/libc.prx sce_module/libSceFios2.prx data/COPYING games/README.txt'
# This SDK's create-gp4 emits a fixed sample directory tree. Derive it from
# the actual files so data/games and future asset directories are included.
python3 - <<'GP4'
import pathlib, xml.etree.ElementTree as ET
p = pathlib.Path('pkg.gp4')
tree = ET.parse(p)
root = tree.getroot()
rootdir = root.find('rootdir')
rootdir.clear()
for file in root.find('files'):
    node = rootdir
    for part in pathlib.PurePosixPath(file.attrib['targ_path']).parts[:-1]:
        child = next((d for d in node if d.attrib.get('targ_name') == part), None)
        if child is None:
            child = ET.SubElement(node, 'dir', targ_name=part)
        node = child
ET.indent(tree)
tree.write(p, encoding='utf-8', xml_declaration=True)
GP4
"$pkgtool" pkg_build pkg.gp4 .
cp "$content_id.pkg" "$dist/$content_id.pkg"
sha256sum eboot.bin "$dist/$content_id.pkg"
