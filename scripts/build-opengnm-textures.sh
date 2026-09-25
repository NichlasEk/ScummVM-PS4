#!/usr/bin/env bash
# Build an independent texture/sampler GPU test after the baseline stack build.
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
export OO_PS4_TOOLCHAIN=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
"$root/scripts/build-opengnm-probe.sh"
examples="$root/.tools/opengnm-stack/freegnm-examples"
probe="$examples/textures"
reports="$root/artifacts/opengnm/textures"
mkdir -p "$probe/src" "$probe/assets/misc" "$probe/sce_sys" "$reports"
cp "$examples/triangle/Makefile" "$probe/Makefile"
cp "$examples/triangle/sce_sys/icon0.png" "$probe/sce_sys/"
cp "$examples/triangle/assets/misc/tri.vert.glsl" "$probe/assets/misc/"
cp "$examples/triangle/assets/misc/clear.frag.glsl" "$probe/assets/misc/"
cp "$root/tests/ps4/opengnm-textures.frag" "$probe/assets/misc/tri.frag.glsl"
cp "$root/tests/ps4/opengnm-textures.c" "$probe/src/main.c"
compiler="$root/.tools/opengnm-stack/opengnm-psbc/opengnm-psbc -4"
make -C "$probe" -B assets/misc/tri.vert.sb assets/misc/tri.frag.sb assets/misc/clear.frag.sb \
 PSBC="$compiler" > "$reports/shaders.log" 2>&1
if [[ -f "$probe/sce_sys/param.sfo" ]]; then
 mv "$probe/sce_sys/param.sfo" "$probe/sce_sys/param.sfo.previous"
fi
content=IV0000-SCVM00008_00-SCUMMVMGNMTEX001
make -C "$probe" USE_OPENGNM=1 TITLE_ID=SCVM00008 CONTENT_ID="$content" \
 TITLE='ScummVM OpenGNM Textures' PSBC="$compiler" \
 RUNTIME_MODULE_DIR="$OO_PS4_TOOLCHAIN/samples/SDL2/sce_module" > "$reports/package.log" 2>&1
readelf -d "$probe/tri" > "$reports/imports.txt"
if rg 'NEEDED.*(Piglet|Shacc)' "$reports/imports.txt"; then
 echo 'Unexpected proprietary graphics dependency' >&2; exit 1
fi
cp "$probe/$content.pkg" "$root/dist/ScummVM-PS4-OpenGNM-Textures.pkg"
cd "$root/dist"
sha256sum ScummVM-PS4-OpenGNM-Textures.pkg > ScummVM-PS4-OpenGNM-Textures.pkg.sha256
cat ScummVM-PS4-OpenGNM-Textures.pkg.sha256
