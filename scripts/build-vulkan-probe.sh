#!/usr/bin/env bash
# Independent native Vulkan probe; never replaces the stable ScummVM package.
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
case "${1:-}" in
 ""|--resources|--upload|--composite|--sprites|--readback|--rectclear) ;;
 *) echo 'Usage: build-vulkan-probe.sh [--resources|--upload|--composite|--sprites|--readback|--rectclear]' >&2; exit 2 ;;
esac
export OO_PS4_TOOLCHAIN=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
"$root/scripts/build-opengnm-probe.sh"
probe="$root/build/vulkan-probe"
reports="$root/artifacts/vulkan"
mkdir -p "$probe" "$reports"
cp "$root/tests/ps4/vulkan/draw.c" "$probe/draw.c"
cp "$root/tests/ps4/vulkan/resources.h" "$probe/resources.h"
cp "$root/tests/ps4/vulkan/readback.h" "$probe/readback.h"
cp "$root/tests/ps4/vulkan/sprites.h" "$probe/sprites.h"
cp "$root/tests/ps4/vulkan/composite.h" "$probe/composite.h"
if [[ "${1:-}" == --composite || "${1:-}" == --readback || "${1:-}" == --rectclear ]]; then
 sed -i '1i#define COMPOSITE_PROBE 1' "$probe/draw.c"
fi
if [[ "${1:-}" == --resources || "${1:-}" == --upload || "${1:-}" == --sprites ]]; then
 sed -i '1i#define RESOURCE_PROBE 1' "$probe/draw.c"
fi
if [[ "${1:-}" == --upload ]]; then
 sed -i '1i#define UPLOAD_PROBE 1' "$probe/draw.c"
fi
if [[ "${1:-}" == --sprites ]]; then
 sed -i '1i#define SPRITES_PROBE 1' "$probe/draw.c"
fi
if [[ "${1:-}" == --readback || "${1:-}" == --rectclear ]]; then
 sed -i '1i#define READBACK_PROBE 1' "$probe/draw.c"
fi
if [[ "${1:-}" == --rectclear ]]; then
 sed -i '1i#define RECTCLEAR_PROBE 1' "$probe/draw.c"
fi
python3 "$root/scripts/prepare-vulkan-shaders.py" "$probe" "$@"
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
package=ScummVM-PS4-Vulkan-Probe.pkg
if [[ "${1:-}" == --resources ]]; then
 package=ScummVM-PS4-Vulkan-Resources.pkg
fi
if [[ "${1:-}" == --upload ]]; then
 package=ScummVM-PS4-Vulkan-Upload.pkg
fi
if [[ "${1:-}" == --composite ]]; then
 package=ScummVM-PS4-Vulkan-Composite.pkg
fi
if [[ "${1:-}" == --sprites ]]; then
 package=ScummVM-PS4-Vulkan-Sprites.pkg
fi
if [[ "${1:-}" == --readback || "${1:-}" == --rectclear ]]; then
 package=ScummVM-PS4-Vulkan-Readback.pkg
fi
if [[ "${1:-}" == --rectclear ]]; then
 package=ScummVM-PS4-Vulkan-RectClear.pkg
fi
cp "$driver/$content.pkg" "$root/dist/$package"
cd "$root/dist"
sha256sum "$package" > "$package.sha256"
cat "$package.sha256"
