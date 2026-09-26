#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
src="$root/.tools/glslang"
[[ $(git -C "$src" rev-parse HEAD) == fa9c3deb49e035a8abcabe366f26aac010f6cbfb ]]
work="$root/build/glslang-host"
mkdir -p "$root/artifacts/glslang"
cmake -S "$src" -B "$work" -G Ninja -DCMAKE_BUILD_TYPE=Release \
 -DBUILD_EXTERNAL=OFF -DENABLE_GLSLANG_BINARIES=OFF -DENABLE_HLSL=OFF -DENABLE_OPT=OFF \
 -DENABLE_SPVREMAPPER=OFF -DENABLE_PCH=OFF -DGLSLANG_TESTS=OFF -DGLSLANG_ENABLE_INSTALL=OFF \
 > artifacts/glslang/host-build.log 2>&1
cmake --build "$work" -j"${JOBS:-8}" >> artifacts/glslang/host-build.log 2>&1
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Icompat/ps4gl -I"$src" \
 -c compat/ps4gl/ps4gl-glsl.c -o "$work/wrapper.o"
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Icompat/ps4gl \
 -c tests/ps4/check-ps4gl-glsl.c -o "$work/check.o"
c++ -fsanitize=address,undefined "$work/check.o" "$work/wrapper.o" -Wl,--start-group \
 "$work/glslang/libglslang.a" "$work/glslang/libMachineIndependent.a" "$work/glslang/libGenericCodeGen.a" \
 "$work/glslang/OSDependent/Unix/libOSDependent.a" "$work/SPIRV/libSPIRV.a" \
 "$work/glslang/libglslang-default-resource-limits.a" -Wl,--end-group -lpthread -o "$work/check-glsl"
"$work/check-glsl" | tee artifacts/glslang/host-test.log
spirv-val "$work/test.spv"
