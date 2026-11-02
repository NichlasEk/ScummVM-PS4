#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
export OO_PS4_TOOLCHAIN=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
sdk=$OO_PS4_TOOLCHAIN
stage=${SCUMMVM_STAGE_DIR:-"$root/build/scummvm-pkg"}
build_dir=${SCUMMVM_BUILD_DIR:-"$root/build/scummvm"}
source_dir=${SCUMMVM_SOURCE_DIR:-"$root/.tools/scummvm-src"}
dist=${SCUMMVM_DIST_DIR:-"$root/dist"}
content_id=${SCUMMVM_CONTENT_ID:-IV0000-SCVM00002_00-SCUMMVMPS4000001}
title_id=${SCUMMVM_TITLE_ID:-SCVM00002}
app_title=${SCUMMVM_TITLE:-"ScummVM PS4"}
output_name=${SCUMMVM_PACKAGE_NAME:-ScummVM-PS4-2.9.1.pkg}
app_version=${SCUMMVM_APP_VERSION:-1.17}
controller_only=${SCUMMVM_CONTROLLER_ONLY:-0}
[[ "$controller_only" == 0 || "$controller_only" == 1 ]] || exit 2
[[ "$app_version" =~ ^[0-9]+\.[0-9]+$ ]] || exit 2
if [[ "$controller_only" == 1 && ${SCUMMVM_EXPERIMENTAL_VULKAN:-0} != 1 ]]; then
 echo 'Controller-only marker is supported by the experimental Vulkan backend only' >&2; exit 2
fi
dott_source=
if [[ ${1:-} == --with-dott && $# == 2 ]]; then
    dott_source=$(CDPATH= cd -- "$2" && pwd)
    stage="$root/build/scummvm-dott-pkg"
    app_title='ScummVM PS4 + DOTT'
    output_name=ScummVM-PS4-2.9.1-DOTT.pkg
elif [[ $# != 0 ]]; then
    echo "Usage: $0 [--with-dott DIRECTORY_CONTAINING_TENTACLE_FILES]" >&2
    exit 2
fi
if [[ ${SCUMMVM_EXPERIMENTAL_VULKAN:-0} == 1 ]]; then
 [[ ( "$title_id" == SCVM00265 && "$output_name" == ScummVM-2026.4.0-Vulkan-Test.pkg ) ||
    ( "$title_id" == SCVM00266 && "$output_name" == ScummVM-MT32-Diagnostic-0.23.pkg && "$content_id" == IV0000-SCVM00266_00-SCUMMVM2026MT001 ) ||
    ( "$title_id" == SCVM00267 && "$output_name" == ScummVM-MT32-Intro-Trace-0.25.pkg && "$content_id" == IV0000-SCVM00267_00-SCUMMVM2026MT002 ) ]] || {
  echo 'Experimental Vulkan package requires its isolated title and filename' >&2; exit 1;
 }
fi
# Fail before packaging if cross-compilation accidentally picks up host libraries.
python3 - "$build_dir/scummvm" "${SCUMMVM_EXPERIMENTAL_VULKAN:-0}" <<'CHECK'
import re, subprocess, sys
needed = re.findall(r'\(NEEDED\).*?\[(.*?)\]', subprocess.check_output(['readelf', '-d', sys.argv[1]], text=True))
allowed = {'libkernel.so', 'libSceUserService.so', 'libSceVideoOut.so',
           'libSceAudioOut.so', 'libScePad.so', 'libSceSysmodule.so'}
if sys.argv[2] == '1':
    allowed.add('libSceGnmDriver.so')
    allowed.add('libSceSystemService.so')
    allowed.add('libSceMouse.so')
unexpected = set(needed) - allowed
if unexpected:
    sys.exit('Unexpected non-target dependencies: ' + ', '.join(sorted(unexpected)))
print('PS4 imports:', ', '.join(needed))
CHECK
mkdir -p "$stage/sce_sys/about" "$stage/sce_module" "$stage/data" "$stage/games" "$dist"
"$sdk/bin/linux/create-fself" -in="$build_dir/scummvm" \
 -out="$stage.oelf" --eboot "$stage/eboot.bin" --paid 0x3800000000000011
cp "$sdk/samples/SDL2/sce_module/"{libc.prx,libSceFios2.prx} "$stage/sce_module/"
cp "$sdk/samples/piglet/sce_sys/about/right.sprx" "$stage/sce_sys/about/"
cp "$root/assets/icon0.png" "$stage/sce_sys/"
cp "$sdk/LICENSE" "$stage/data/OpenOrbis-LICENSE"
cp "$root/build/ps4-jpeg/install/share/licenses/libjpeg-turbo/LICENSE.md" "$stage/data/libjpeg-turbo-LICENSE.md"
cp "$root/build/ps4-jpeg/install/share/licenses/libjpeg-turbo/README.ijg" "$stage/data/libjpeg-turbo-README.ijg"
cp "$root/build/ps4-mad/install/share/licenses/libmad/COPYING" "$stage/data/libmad-COPYING"
cp "$root/build/ps4-mad/install/share/licenses/libmad/COPYRIGHT" "$stage/data/libmad-COPYRIGHT"
cp "$root/build/ps4-freetype/install/share/licenses/freetype/GPLv2.TXT" "$stage/data/freetype-GPLv2.TXT"
cp "$root/build/ps4-freetype/install/share/licenses/freetype/FTL.TXT" "$stage/data/freetype-FTL.TXT"
cp "$root/build/ps4-freetype/install/share/licenses/freetype/LICENSE.TXT" "$stage/data/freetype-LICENSE.TXT"
cp "$root/build/ps4-vorbis/install/share/licenses/"{libogg-COPYING,libvorbis-COPYING} "$stage/data/"
cp "$root/build/ps4-png/install/share/licenses/"{libpng-LICENSE,zlib-LICENSE} "$stage/data/"
cp "$root/build/ps4-theora/install/share/licenses/libtheora-COPYING" "$stage/data/"
cp "$root/build/ps4-mpeg2/install/share/licenses/libmpeg2-COPYING" "$stage/data/"
cp "$root/build/ps4-gif/install/share/licenses/giflib-COPYING" "$stage/data/"
cp "$root/build/ps4-flac/install/share/licenses/flac-COPYING.Xiph" "$stage/data/"
cp "$root/build/ps4-vpx/install/share/licenses/libvpx-LICENSE" "$stage/data/"
cp "$root/build/ps4-vpx/install/share/licenses/libvpx-PATENTS" "$stage/data/"
cp "$root/build/ps4-a52/install/share/licenses/liba52-COPYING" "$stage/data/"
cp "$source_dir/LICENSES/COPYING.LGPL" "$stage/data/mt32emu-COPYING.LGPL"
cp "$source_dir/COPYING" "$stage/data/COPYING"
cp "$source_dir/COPYRIGHT" "$stage/data/COPYRIGHT"
cp "$source_dir/common/lua/COPYRIGHT" "$stage/data/lua-COPYRIGHT"
cp "$source_dir/backends/vkeybd/packs/vkeybd_default.zip" "$stage/data/"
# Build from the patched layout sources, not upstream's prebuilt archive.
(cd "$source_dir/gui/themes" && python3 scummtheme.py make scummremastered)
cp "$source_dir/gui/themes/scummremastered.zip" "$stage/data/"
cp "$source_dir/gui/themes/gui-icons.dat" "$stage/data/"
printf '%s\n' 'Place legally obtained ScummVM game data in subdirectories here.' > "$stage/games/README.txt"
pkgtool="$sdk/bin/linux/PkgTool.Core"
sfo="$stage/sce_sys/param.sfo"
# sfo_new expects a new path.
if [[ -e "$sfo" ]]; then mv "$sfo" "$sfo.previous"; fi
"$pkgtool" sfo_new "$sfo"
for pair in APP_TYPE:1 ATTRIBUTE:0 DOWNLOAD_DATA_SIZE:0 SYSTEM_VER:0; do
 "${pkgtool}" sfo_setentry "$sfo" "${pair%%:*}" --type Integer --maxsize 4 --value "${pair#*:}"
done
"$pkgtool" sfo_setentry "$sfo" APP_VER --type Utf8 --maxsize 8 --value "$app_version"
"$pkgtool" sfo_setentry "$sfo" VERSION --type Utf8 --maxsize 8 --value "$app_version"
"$pkgtool" sfo_setentry "$sfo" CATEGORY --type Utf8 --maxsize 4 --value gd
"$pkgtool" sfo_setentry "$sfo" CONTENT_ID --type Utf8 --maxsize 48 --value "$content_id"
"$pkgtool" sfo_setentry "$sfo" TITLE_ID --type Utf8 --maxsize 12 --value "$title_id"
"$pkgtool" sfo_setentry "$sfo" TITLE --type Utf8 --maxsize 128 --value "$app_title"
package_files=(eboot.bin sce_sys/param.sfo sce_sys/icon0.png sce_sys/about/right.sprx
    sce_module/libc.prx sce_module/libSceFios2.prx data/COPYING data/COPYRIGHT data/OpenOrbis-LICENSE
    data/libjpeg-turbo-LICENSE.md data/libjpeg-turbo-README.ijg data/libmad-COPYING data/libmad-COPYRIGHT
    data/freetype-FTL.TXT data/freetype-LICENSE.TXT data/freetype-GPLv2.TXT
    data/libogg-COPYING data/libvorbis-COPYING data/libpng-LICENSE data/zlib-LICENSE data/libtheora-COPYING data/libmpeg2-COPYING data/giflib-COPYING
    data/flac-COPYING.Xiph data/libvpx-LICENSE data/libvpx-PATENTS data/liba52-COPYING data/mt32emu-COPYING.LGPL
    data/lua-COPYRIGHT data/vkeybd_default.zip data/scummremastered.zip data/gui-icons.dat games/README.txt)
if [[ ${SCUMMVM_EXPERIMENTAL_VULKAN:-0} == 1 ]]; then
 if [[ -f "$source_dir/engines/director/fonts/pfr1/COPYING.AGPL-3.0" ]]; then
  cp "$source_dir/engines/director/fonts/pfr1/COPYING.AGPL-3.0" "$stage/data/LibreShockwave-COPYING.AGPL-3.0"
  cp "$source_dir/engines/director/fonts/pfr1/README" "$stage/data/LibreShockwave-NOTICE"
  package_files+=(data/LibreShockwave-COPYING.AGPL-3.0 data/LibreShockwave-NOTICE)
 fi
 if [[ ${SCUMMVM_OPERATION_SUBMIT:-0} == 1 ]]; then
  printf '1\n' > "$stage/scummvm-operation-submit.txt"
  package_files+=(scummvm-operation-submit.txt)
 else
  rm -f "$stage/scummvm-operation-submit.txt"
 fi
 if [[ ${SCUMMVM_SERIAL_SUBMIT:-0} == 1 ]]; then
  printf '1\n' > "$stage/scummvm-serial-submit.txt"
  package_files+=(scummvm-serial-submit.txt)
 else
  rm -f "$stage/scummvm-serial-submit.txt"
 fi
 if [[ "$controller_only" == 1 ]]; then
  printf '1\n' > "$stage/scummvm-controller-only.txt"
  package_files+=(scummvm-controller-only.txt)
 else
  rm -f "$stage/scummvm-controller-only.txt"
 fi
 for spec in 'opengnm/LICENSE:OpenGNM-LICENSE' 'opengnm-psbc/LICENSE:PSBC-LICENSE' 'vulkan-ps4/LICENSE:Vulkan-PS4-LICENSE'; do
  cp "$root/.tools/opengnm-stack/${spec%%:*}" "$stage/data/${spec#*:}"
  package_files+=("data/${spec#*:}")
 done
 cp "$root/.tools/glslang/LICENSE.txt" "$stage/data/glslang-LICENSE.txt"
 cat "$root/.tools/glslang/glslang/Include/glslang_c_interface.h" >> "$stage/data/glslang-LICENSE.txt"
 cp "$root/.tools/sdl-input-src/COPYING.txt" "$stage/data/SDL-LICENSE.txt"
 package_files+=(data/glslang-LICENSE.txt data/SDL-LICENSE.txt)
fi
# Use upstream's conditional data manifest for the engines actually compiled.
# Only these support files are packaged, never arbitrary staged game folders.
engine_data=$(make -s --no-print-directory -C "$build_dir" \
    --eval='ps4-engine-data: ; @printf "%s\n" $(DIST_FILES_ENGINEDATA) $(DIST_FILES_ENGINEDATA_BIG)' ps4-engine-data)
while IFS= read -r resource; do
    [[ -n "$resource" ]] || continue
    cp "$resource" "$stage/data/"
    package_files+=("data/$(basename "$resource")")
done <<< "$engine_data"
if [[ ${SCUMMVM_EXPERIMENTAL_VULKAN:-0} == 1 ]]; then
 python3 "$root/scripts/package-ps4-shaders.py" "$stage/data/shaders.dat"
 package_files+=(data/shaders.dat)
fi

shader_data=$(make -s --no-print-directory -C "$build_dir" \
    --eval='ps4-shader-data: ; @printf "%s\n" $(DIST_FILES_SHADERS)' ps4-shader-data)
while IFS= read -r resource; do
    [[ -n "$resource" ]] || continue
    mkdir -p "$stage/data/shaders"
    cp "$resource" "$stage/data/shaders/"
    package_files+=("data/shaders/$(basename "$resource")")
done <<< "$shader_data"
if [[ -n "$dott_source" ]]; then
    mkdir -p "$stage/games/dott"
    # Include only the full game's data, not DOS executables or unrelated demos.
    python3 - "$dott_source" "$stage/games/dott" <<'DOTT'
import pathlib, shutil, sys
source, destination = map(pathlib.Path, sys.argv[1:])
files = {p.name.lower(): p for p in source.iterdir() if p.is_file()}
for name in ('tentacle.000', 'tentacle.001', 'monster.sou'):
    if name not in files or files[name].stat().st_size == 0:
        sys.exit('Missing DOTT CD data file: ' + str(source / name))
    shutil.copyfile(files[name], destination / name)
DOTT
    cp "$root/config/scummvm-dott.ini" "$stage/scummvm-default.ini"
    package_files+=(games/dott/tentacle.000 games/dott/tentacle.001 games/dott/monster.sou scummvm-default.ini)
fi
cd "$stage"
"$sdk/bin/linux/create-gp4" -out pkg.gp4 --content-id="$content_id" \
 --files "${package_files[*]}"
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
cp "$content_id.pkg" "$dist/$output_name"
sha256sum eboot.bin "$dist/$output_name"
(cd "$dist" && sha256sum "$output_name" > "$output_name.sha256")
# Preserve the original generic artifact path used by existing local workflows.
if [[ -z "$dott_source" ]]; then
    cp "$dist/$output_name" "$dist/$content_id.pkg"
fi
