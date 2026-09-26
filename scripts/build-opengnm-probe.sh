#!/usr/bin/env bash
# Independent GPU milestone; does not change the ScummVM package or SDK.
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
stack="$root/.tools/opengnm-stack"
export OO_PS4_TOOLCHAIN=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
jobs=${JOBS:-8}
mkdir -p "$root/artifacts/opengnm" "$root/dist"
pin() {
 local url=$1 directory=$2 revision=$3
 if [[ ! -d "$directory/.git" && ! -f "$directory/.git" ]]; then
  git clone --no-checkout "$url" "$directory"
  git -C "$directory" checkout --detach "$revision"
 fi
 [[ $(git -C "$directory" rev-parse HEAD) == "$revision" ]] || {
  echo "Unexpected revision: $directory" >&2; exit 1;
 }
}
pin https://github.com/PS4-OpenGNM/opengnm-stack.git "$stack" 2de9fc54ca33b39baa0ace59c6b0059e5de94a63
git -C "$stack" submodule update --init --depth 1 opengnm opengnm-psbc freegnm-examples
pin https://github.com/PS4-OpenGNM/vulkan-ps4.git "$stack/vulkan-ps4" f4d940b723771b3fe637e8ac0704bb5d6ab19622
pin https://github.com/KhronosGroup/SPIRV-Headers.git "$stack/SPIRV-Headers" cb42dec3830d3ac67fa449ecdc0c0f73d5e74498
pin https://github.com/KhronosGroup/Vulkan-Headers.git "$stack/Vulkan-Headers" 3c65a01745e4a1134d32b9c2c456472212dba16d
patch_once() {
 local directory=$1 patch=$2
 if git -C "$directory" apply --reverse --check "$patch" 2>/dev/null; then return; fi
 git -C "$directory" apply --check "$patch"
 git -C "$directory" apply "$patch"
}
patch_once "$stack/opengnm-psbc" "$root/patches/opengnm/psbc-build.patch"
patch_once "$stack/vulkan-ps4" "$root/patches/opengnm/vulkan-runtime.patch"
patch_once "$stack/opengnm" "$root/patches/opengnm/gnm-init.patch"
patch_once "$stack/freegnm-examples" "$root/patches/opengnm/triangle-probe.patch"
# Keep schema validation enabled without changing the system Python installation.
codegen_python="$root/.tools/psbc-codegen/bin/python3"
if [[ ! -x "$codegen_python" ]]; then python3 -m venv --system-site-packages "$root/.tools/psbc-codegen"; fi
if ! "$codegen_python" -c 'import lxml.etree, rnc2rng, mako, yaml' 2>/dev/null; then
 "$codegen_python" -m pip install --disable-pip-version-check -r "$root/scripts/psbc-codegen-requirements.txt"
fi
"$codegen_python" "$root/scripts/prepare-opengnm-psbc.py" "$stack/opengnm-psbc"
make -C "$stack/opengnm-psbc" -j"$jobs" > "$root/artifacts/opengnm/psbc-host-build.log" 2>&1
python3 "$root/tests/ps4/check-opengnm-compiler.py" > "$root/artifacts/opengnm/psbc-verify.log" 2>&1
cmake -S "$stack/opengnm" -B "$root/build/opengnm-host" \
 -DOPENGNM_PLATFORM=generic -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_STANDARD_LIBRARIES=-lm \
 > "$root/artifacts/opengnm/gnm-host-build.log" 2>&1
cmake --build "$root/build/opengnm-host" -j"$jobs" >> "$root/artifacts/opengnm/gnm-host-build.log" 2>&1
ctest --test-dir "$root/build/opengnm-host" --output-on-failure > "$root/artifacts/opengnm/gnm-host-tests.log" 2>&1
cc -std=c11 -I"$stack/opengnm/include" "$root/tests/ps4/check-opengnm-texture-layout.c" \
 "$root/build/opengnm-host/libopengnm.a" -lm -o "$root/build/opengnm-host/check-texture-layout"
"$root/build/opengnm-host/check-texture-layout" >> "$root/artifacts/opengnm/gnm-host-tests.log"
cp "$stack/opengnm/config.orbis.mak" "$stack/opengnm/config.mak"
make -C "$stack/opengnm" CC=clang AR=llvm-ar > "$root/artifacts/opengnm/gnm-orbis-build.log" 2>&1
make -C "$stack/opengnm-psbc" -f Makefile.orbis -j"$jobs" > "$root/artifacts/opengnm/psbc-orbis-build.log" 2>&1
make -C "$stack/vulkan-ps4" -f Makefile.orbis -j"$jobs" > "$root/artifacts/opengnm/vulkan-orbis-build.log" 2>&1
# Upstream defaults to PS5! Explicit -4 selects PS4 base GFX7.
make -C "$stack/freegnm-examples/triangle" -B \
 assets/misc/tri.vert.sb assets/misc/tri.frag.sb assets/misc/clear.frag.sb \
 PSBC="$stack/opengnm-psbc/opengnm-psbc -4" \
 > "$root/artifacts/opengnm/triangle-shaders.log" 2>&1
content=IV0000-SCVM00006_00-SCUMMVMGNMPROBE1
sfo="$stack/freegnm-examples/triangle/sce_sys/param.sfo"
if [[ -f "$sfo" ]]; then mv "$sfo" "$sfo.previous"; fi
make -C "$stack/freegnm-examples/triangle" USE_OPENGNM=1 \
 TITLE_ID=SCVM00006 CONTENT_ID="$content" TITLE='ScummVM OpenGNM Probe' \
 PSBC="$stack/opengnm-psbc/opengnm-psbc -4" \
 RUNTIME_MODULE_DIR="$OO_PS4_TOOLCHAIN/samples/SDL2/sce_module" \
 > "$root/artifacts/opengnm/triangle-package.log" 2>&1
elf="$stack/freegnm-examples/triangle/tri"
readelf -d "$elf" > "$root/artifacts/opengnm/triangle-imports.txt"
if rg 'NEEDED.*(Piglet|Shacc)' "$root/artifacts/opengnm/triangle-imports.txt"; then
 echo 'Unexpected proprietary graphics dependency' >&2; exit 1
fi
cp "$stack/freegnm-examples/triangle/$content.pkg" "$root/dist/ScummVM-PS4-OpenGNM-Probe.pkg"
cd "$root/dist"
sha256sum ScummVM-PS4-OpenGNM-Probe.pkg > ScummVM-PS4-OpenGNM-Probe.pkg.sha256
cat ScummVM-PS4-OpenGNM-Probe.pkg.sha256
echo 'Build only. Run target and inspect pixels separately; physical PS4 unverified.'
