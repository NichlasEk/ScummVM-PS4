#!/usr/bin/env bash
# Isolated integration build; never packages over the stable ScummVM application.
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
export OO_PS4_TOOLCHAIN=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
sdk=$OO_PS4_TOOLCHAIN
export SCUMMVM_SOURCE_DIR="$root/.tools/scummvm-vulkan-src"
export SCUMMVM_BUILD_DIR="$root/build/scummvm-vulkan"
overlay="$SCUMMVM_SOURCE_DIR/.vulkan-overlay.patch"
if [[ -f "$overlay" ]]; then
 git -C "$SCUMMVM_SOURCE_DIR" apply --reverse --check "$overlay"
 git -C "$SCUMMVM_SOURCE_DIR" apply --reverse "$overlay"
 rm "$overlay"
fi
"$root/scripts/prepare-scummvm.sh"
# Only dedicated source copy: preserve ordinary PS4 settings and saves.
python3 - "$SCUMMVM_SOURCE_DIR" "$root/compat/ps4gl/scummvm-context.inc" <<'PY'
from pathlib import Path
import sys, difflib, subprocess
root=Path(sys.argv[1])
patch=[]
for name in ('posix-main.cpp','posix.cpp'):
 p=root/'backends/platform/sdl/posix'/name
 original=p.read_text()
 s=original.replace('/data/scummvm/', '/data/scummvm-vulkan/').replace('"/data/scummvm"','"/data/scummvm-vulkan"')
 relative=str(p.relative_to(root))
 patch.extend(difflib.unified_diff(original.splitlines(True),s.splitlines(True),fromfile='a/'+relative,tofile='b/'+relative))
relative='backends/graphics/openglsdl/openglsdl-graphics.cpp'
p=root/relative
original=p.read_text()
s=original
changes={
 'SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);':'SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);',
 'DEFAULT_GL_MAJOR = 1,':'DEFAULT_GL_MAJOR = 2,',
 'DEFAULT_GL_MINOR = 4,':'DEFAULT_GL_MINOR = 0,',
 'if (!createOrUpdateWindow(width, height, flags)) {':
 '// Experimental native display is fixed-size and color-only.\n\twidth = 1280; height = 720; flags = SDL_WINDOW_OPENGL; _vsync = true;\n\tif (!createOrUpdateWindow(width, height, flags)) {'
}
for old,new in changes.items():
 assert s.count(old)==1, old
 s=s.replace(old,new)
patch.extend(difflib.unified_diff(original.splitlines(True),s.splitlines(True),fromfile='a/'+relative,tofile='b/'+relative))
relative='backends/platform/sdl/sdl-window.cpp'
p=root/relative
original=p.read_text()
s=original
for old,new in (
 ('mode.w, mode.h, 0);', 'mode.w, mode.h, flags & SDL_WINDOW_OPENGL);'),
 ('\t_lastFlags = 0;\n\treturn _window != nullptr;', '\t_lastFlags = flags & SDL_WINDOW_OPENGL;\n\treturn _window != nullptr;')
):
 assert s.count(old)==1, old
 s=s.replace(old,new)
patch.extend(difflib.unified_diff(original.splitlines(True),s.splitlines(True),fromfile='a/'+relative,tofile='b/'+relative))
relative='backends/platform/sdl/sdl.cpp'
p=root/relative
original=p.read_text()
s=original.replace('if (f == kFeatureShadersForGame) return _supportsShaders;',
 'if (f == kFeatureShadersForGame) return OpenGLContext.type == OpenGL::kContextGL && OpenGLContext.enginesShadersSupported;')
anchor='void OSystem_SDL::detectOpenGLFeaturesSupport() {\n'
assert s.count(anchor)==1
s=s.replace(anchor,anchor+'\t// This backend owns one fixed window; query capabilities from its live context.\n\t_oglType = OpenGL::kContextGL; _supportsFrameBuffer = false; _supportsShaders = false; return;\n')
patch.extend(difflib.unified_diff(original.splitlines(True),s.splitlines(True),fromfile='a/'+relative,tofile='b/'+relative))
relative='backends/graphics3d/openglsdl/openglsdl-graphics3d.cpp'
p=root/relative
original=p.read_text()
s=original.replace('SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);','SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);').replace('SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);','SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);')
# GL 0.0 subset supports completed offscreen captures, not the presented swapchain.
s=s.replace('if (g_engine) {', 'if (g_engine && glReadPixels && OpenGLContext.majorVersion > 0) {')
anchor='\t\tif (_window->createOrUpdateWindow(effectiveWidth, effectiveHeight, sdlflags)) {'
assert s.count(anchor)==1
s=s.replace(anchor,'\t\teffectiveWidth = 1280; effectiveHeight = 720; sdlflags = SDL_WINDOW_OPENGL; _vsync = true;\n'+anchor)
patch.extend(difflib.unified_diff(original.splitlines(True),s.splitlines(True),fromfile='a/'+relative,tofile='b/'+relative))
relative='engines/grim/grim.cpp'
p=root/relative
original=p.read_text()
anchor='\t\tinitGraphics3d(screenW, screenH);'
assert original.count(anchor)==1
s=original.replace(anchor,anchor+'\n\t\t// Fixed native target can differ from the engine request.\n\t\tscreenW = g_system->getWidth(); screenH = g_system->getHeight();')
patch.extend(difflib.unified_diff(original.splitlines(True),s.splitlines(True),fromfile='a/'+relative,tofile='b/'+relative))
relative='engines/twp/metaengine.cpp'
p=root/relative
original=p.read_text()
s=original
# Keep the backend analog virtual mouse active; TWP's digital stick mappings
# consume even sub-threshold axes, including shadPS4 mouse-to-stick movement.
for key,axis in (('LEFT','X-'),('RIGHT','X+'),('UP','Y-'),('DOWN','Y+')):
 old='"'+key+'|JOY_LEFT_STICK_'+axis+'"'
 assert s.count(old)==1
 s=s.replace(old,'"'+key+'"')
patch.extend(difflib.unified_diff(original.splitlines(True),s.splitlines(True),fromfile='a/'+relative,tofile='b/'+relative))
relative='gui/message.cpp'
p=root/relative
original=p.read_text()
s=original.replace('#include "gui/widget.h"', '#include "gui/widget.h"\n#include "common/debug.h"')
s=s.replace('\t_message = message;', '\tdebug(1, "PS4GL dialog: %s", message.encode().c_str());\n\t_message = message;')
patch.extend(difflib.unified_diff(original.splitlines(True),s.splitlines(True),fromfile='a/'+relative,tofile='b/'+relative))
relative='graphics/opengl/context.cpp'
p=root/relative
original=p.read_text()
anchor='\ttype = contextType;\n'
assert original.count(anchor)==1
s=original.replace(anchor,anchor+Path(sys.argv[2]).read_text())
patch.extend(difflib.unified_diff(original.splitlines(True),s.splitlines(True),fromfile='a/'+relative,tofile='b/'+relative))
import runpy
patch.extend(runpy.run_path(str(Path(sys.argv[2]).parents[2]/'scripts/scummvm-vulkan-runtime.py'))['runtime_patches'](root))
overlay=root/'.vulkan-overlay.patch'
overlay.write_text(''.join(patch))
subprocess.run(['git','-C',str(root),'apply','--check',str(overlay)],check=True)
subprocess.run(['git','-C',str(root),'apply',str(overlay)],check=True)
PY
"$root/scripts/build-ps4-sdlgl.sh"
"$root/scripts/build-ps4-glslang.sh"
native="$root/.tools/opengnm-stack"
# Do not link a stale ICD after editing the durable native runtime patch.
mkdir -p "$root/artifacts/scummvm-vulkan"
git -C "$native/vulkan-ps4" apply --reverse --check "$root/patches/opengnm/vulkan-runtime.patch"
make -C "$native/vulkan-ps4" -f Makefile.orbis -j"${BUILD_JOBS:-12}" > "$root/artifacts/scummvm-vulkan/native-build.log" 2>&1
for archive in "$native/vulkan-ps4/libvulkan_ps4.a" "$native/opengnm/libopengnm.a" "$native/opengnm-psbc/libpsbc.orbis.a"; do
 [[ -f "$archive" ]] || { echo "Build native Vulkan dependencies first: scripts/build-vulkan-probe.sh --sdlgl" >&2; exit 1; }
done
mkdir -p "$SCUMMVM_BUILD_DIR/bridge" "$root/artifacts/scummvm-vulkan"
# PSBC bundles a C11 thread shim whose unprefixed symbols collide with the SDK
# libc in real ScummVM links. Namespace definitions AND internal references in
# a private archive copy, leaving the native probe's dependency untouched.
python3 - "$native/opengnm-psbc/libpsbc.orbis.a" "$SCUMMVM_BUILD_DIR/bridge" <<'PYBC'
import pathlib, subprocess, sys
source, folder = map(pathlib.Path, sys.argv[1:])
obj=folder/'psbc-threads.o'
obj.write_bytes(subprocess.check_output(['llvm-ar','p',str(source),'threads_posix.orbis.o']))
names=subprocess.check_output(['llvm-nm','--defined-only','--extern-only','--just-symbol-name',str(obj)],text=True).split()
assert names and 'call_once' in names and 'thrd_create' in names
assert all(n=='call_once' or n.startswith(('cnd_','mtx_','thrd_','tss_')) for n in names)
mapping=folder/'psbc-thread-symbols.txt'
mapping.write_text(''.join(f'{n} ps4gl_psbc_{n}\n' for n in names))
subprocess.run(['llvm-objcopy','--redefine-syms='+str(mapping),str(source),str(folder/'libpsbc-scummvm.a')],check=True)
PYBC
objects=()
for name in ps4gl ps4gl-vulkan ps4gl-glsl ps4gl-frame ps4gl-display ps4gl-main; do
 object="$SCUMMVM_BUILD_DIR/bridge/$name.o"
 clang --target=x86_64-pc-freebsd12-elf -fPIC -O2 -D__PS4__ -D__ORBIS__ \
  -isysroot "$sdk" -isystem "$sdk/include" -I"$root/compat/ps4gl" \
  -I"$native/Vulkan-Headers/include" -I"$native/vulkan-ps4/include" -I"$root/.tools/glslang" \
  -c "$root/compat/ps4gl/$name.c" -o "$object"
 [[ "$name" == ps4gl-main ]] || objects+=("$object")
done
export SCUMMVM_EXPERIMENTAL_VULKAN=1
# Default to the full engine set; use SCUMMVM_ENGINES=twp for a focused rebuild.
export SCUMMVM_ENGINES=${SCUMMVM_ENGINES:-}
export SCUMMVM_SDL_CONFIG=scummvm-vulkan-sdl2-config
bridge_ldflags="-Wl,--wrap=main -Wl,--wrap=_exit -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=free -Wl,-z,max-page-size=0x4000"
export SCUMMVM_VULKAN_LIBS="${objects[*]} -Wl,--whole-archive $native/vulkan-ps4/libvulkan_ps4.a $native/opengnm/libopengnm.a -Wl,--no-whole-archive $SCUMMVM_BUILD_DIR/bridge/libpsbc-scummvm.a $root/build/glslang-orbis/libglslang-ps4.a -lunwind -lSceGnmDriver -lSceSystemService"
"$root/scripts/configure-scummvm.sh" > "$root/artifacts/scummvm-vulkan/configure.log" 2>&1
for feature in USE_OPENGL USE_GLAD USE_OPENGL_SHADERS; do
 rg -q "^#define $feature" "$SCUMMVM_BUILD_DIR/config.h" || { echo "Missing $feature" >&2; exit 1; }
done
# Configure compiler probes must not wrap their tiny test mains.
printf '\nLDFLAGS += %s\nLIBS += %s\n' "$bridge_ldflags" "$SCUMMVM_BUILD_DIR/bridge/ps4gl-main.o" >> "$SCUMMVM_BUILD_DIR/config.mk"
make -C "$SCUMMVM_BUILD_DIR" -j"${BUILD_JOBS:-12}" > "$root/artifacts/scummvm-vulkan/build.log" 2>&1
echo "Experimental ScummVM ELF: $SCUMMVM_BUILD_DIR/scummvm"

export SCUMMVM_STAGE_DIR="$root/build/scummvm-vulkan-pkg"
export SCUMMVM_TITLE_ID=SCVM00010 SCUMMVM_CONTENT_ID=IV0000-SCVM00010_00-SCUMMVMPVULKAN01
export SCUMMVM_TITLE="${SCUMMVM_TITLE:-ScummVM Vulkan Experimental}" SCUMMVM_PACKAGE_NAME=ScummVM-PS4-Vulkan-Experimental.pkg
"$root/scripts/package-scummvm.sh" > "$root/artifacts/scummvm-vulkan/package.log" 2>&1
