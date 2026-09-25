#!/usr/bin/env bash
# Independent native Vulkan probe; never replaces the stable ScummVM package.
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
export OO_PS4_TOOLCHAIN=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
"$root/scripts/build-opengnm-probe.sh"
probe="$root/build/vulkan-probe"
reports="$root/artifacts/vulkan"
mkdir -p "$probe" "$reports"
cp "$root/tests/ps4/vulkan/draw.c" "$probe/draw.c"
python3 "$root/scripts/prepare-vulkan-shaders.py" "$probe"
content=IV0000-SCVM00009_00-SCUMMVMVULKAN001
driver="$root/.tools/opengnm-stack/vulkan-ps4"
if [[ -f "$driver/sce_sys/param.sfo" ]]; then
 mv "$driver/sce_sys/param.sfo" "$driver/sce_sys/param.sfo.previous"
fi
make -C "$driver" -f Makefile.orbis -j"${JOBS:-8}" pkg \
 TEST_SRC="$probe/draw.c" TEST_OBJ="$probe/draw.o" \
 PKG_TITLE_ID=SCVM00009 PKG_CONTENT_ID="$content" PKG_TITLE='ScummVM Vulkan Probe' \
 SAMPLE_DIR="$OO_PS4_TOOLCHAIN/samples/SDL2" > "$reports/package.log" 2>&1
readelf -d "$driver/test_triangle_ps4.elf" > "$reports/imports.txt"
if rg 'NEEDED.*(Piglet|Shacc)' "$reports/imports.txt"; then
 echo 'Unexpected proprietary graphics dependency' >&2; exit 1
fi
cp "$driver/$content.pkg" "$root/dist/ScummVM-PS4-Vulkan-Probe.pkg"
cd "$root/dist"
sha256sum ScummVM-PS4-Vulkan-Probe.pkg > ScummVM-PS4-Vulkan-Probe.pkg.sha256
cat ScummVM-PS4-Vulkan-Probe.pkg.sha256
