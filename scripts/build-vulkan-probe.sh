#!/usr/bin/env bash
# Independent native Vulkan probe; never replaces the stable ScummVM package.
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
case "${1:-}" in
 ""|--resources|--upload|--composite|--sprites|--readback|--rectclear|--twp|--glstate|--gldraw|--glvbo|--glindex|--gluniform|--glarray|--gltexture|--glblend) ;;
 *) echo 'Usage: build-vulkan-probe.sh [--resources|--upload|--composite|--sprites|--readback|--rectclear|--twp|--glstate|--gldraw|--glvbo|--glindex|--gluniform|--glarray|--gltexture|--glblend]' >&2; exit 2 ;;
esac
variant=${1:-}
if [[ "$variant" == --glstate ]]; then
 set -- --rectclear
fi
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
if [[ "${1:-}" == --twp ]]; then
 sed -i '1i#define TWP_PROBE 1' "$probe/draw.c"
 cp "$root/tests/ps4/vulkan/twp.h" "$probe/twp.h"
 python3 "$root/scripts/prepare-twp-probe.py" "$probe" > "$reports/twp-prepare.log" 2>&1
else
 python3 "$root/scripts/prepare-vulkan-shaders.py" "$probe" "$@"
fi
if [[ "$variant" == --glstate ]]; then
 sed -i '1i#define GLSTATE_PROBE 1' "$probe/draw.c"
 cp "$root/compat/ps4gl/ps4gl.c" "$root/compat/ps4gl/ps4gl.h" "$probe/"
fi
if [[ "$variant" == --glblend || "$variant" == --gldraw || "$variant" == --glvbo || "$variant" == --glindex || "$variant" == --gluniform || "$variant" == --glarray || "$variant" == --gltexture ]]; then
 sed -i '1i#define GLDRAW_PROBE 1' "$probe/draw.c"
 cp "$root/compat/ps4gl/ps4gl.c" "$root/compat/ps4gl/ps4gl.h" "$root/compat/ps4gl/ps4gl-vulkan.c" "$root/compat/ps4gl/ps4gl-reflect.h" "$root/compat/ps4gl/ps4gl-texture-vulkan.h" "$probe/"
fi
if [[ "$variant" == --glvbo || "$variant" == --glindex || "$variant" == --gluniform || "$variant" == --glarray || "$variant" == --gltexture ]]; then
 sed -i '1i#define GLVBO_PROBE 1' "$probe/draw.c"
 cp "$root/tests/ps4/vulkan/glvbo.h" "$probe/"
fi
if [[ "$variant" == --glindex || "$variant" == --gluniform || "$variant" == --glarray || "$variant" == --gltexture ]]; then
 sed -i '1i#define GLINDEX_PROBE 1' "$probe/draw.c"
 cp "$root/tests/ps4/vulkan/glindex.h" "$probe/glindex.h"
fi
if [[ "$variant" == --gluniform || "$variant" == --glarray || "$variant" == --gltexture ]]; then
 sed -i '1i#define GLUNIFORM_PROBE 1' "$probe/draw.c"
 cp "$root/tests/ps4/vulkan/gluniform.h" "$probe/"
fi
if [[ "$variant" == --glarray || "$variant" == --gltexture ]]; then
 sed -i '1i#define GLARRAY_PROBE 1' "$probe/draw.c"
 cp "$root/tests/ps4/vulkan/glarray.h" "$probe/"
fi
if [[ "$variant" == --gltexture ]]; then
 sed -i '1i#define GLTEXTURE_PROBE 1' "$probe/draw.c"
 cp "$root/tests/ps4/vulkan/gltexture.h" "$probe/"
fi
if [[ "$variant" == --glblend ]]; then
 sed -i '1i#define GLBLEND_PROBE 1' "$probe/draw.c"
 cp "$root/tests/ps4/vulkan/glblend.h" "$probe/"
fi
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
if [[ "${1:-}" == --twp ]]; then
 package=ScummVM-PS4-Vulkan-TWP.pkg
fi
if [[ "$variant" == --glstate ]]; then
 package=ScummVM-PS4-GL-State.pkg
fi
if [[ "$variant" == --gldraw ]]; then
 package=ScummVM-PS4-GL-Draw.pkg
fi
if [[ "$variant" == --glvbo ]]; then
 package=ScummVM-PS4-GL-Vertex.pkg
fi
if [[ "$variant" == --glindex || "$variant" == --gluniform || "$variant" == --glarray || "$variant" == --gltexture ]]; then
 package=ScummVM-PS4-GL-Index.pkg
fi
if [[ "$variant" == --gluniform || "$variant" == --glarray || "$variant" == --gltexture ]]; then
 package=ScummVM-PS4-GL-Uniform.pkg
fi
if [[ "$variant" == --glarray || "$variant" == --gltexture ]]; then
 package=ScummVM-PS4-GL-Array.pkg
fi
if [[ "$variant" == --gltexture ]]; then
 package=ScummVM-PS4-GL-Texture.pkg
fi
if [[ "$variant" == --glblend ]]; then
 package=ScummVM-PS4-GL-Blend.pkg
fi
cp "$driver/$content.pkg" "$root/dist/$package"
cd "$root/dist"
sha256sum "$package" > "$package.sha256"
cat "$package.sha256"
