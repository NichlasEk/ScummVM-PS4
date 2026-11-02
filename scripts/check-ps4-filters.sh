#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
work=build/glslang-host
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Icompat/ps4gl -c tests/ps4/check-filter-compiler.c -o "$work/check-filters.o"
c++ -fsanitize=address,undefined "$work/check-filters.o" "$work/wrapper.o" -Wl,--start-group \
 "$work/glslang/libglslang.a" "$work/glslang/libMachineIndependent.a" "$work/glslang/libGenericCodeGen.a" \
 "$work/glslang/OSDependent/Unix/libOSDependent.a" "$work/SPIRV/libSPIRV.a" \
 "$work/glslang/libglslang-default-resource-limits.a" -Wl,--end-group -lpthread -o "$work/check-filters"
"$work/check-filters" assets/ps4-shaders/PS4/*.glsl
