#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
mkdir -p build artifacts/vulkan
flags=(-std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -Icompat/ps4gl -I.tools/opengnm-stack/Vulkan-Headers/include)
cc "${flags[@]}" tests/ps4/check-ps4gl-frame.c compat/ps4gl/ps4gl-frame.c -o build/check-ps4gl-frame
build/check-ps4gl-frame | tee artifacts/vulkan/frame-owner-host.log
cc "${flags[@]}" tests/ps4/check-ps4gl.c compat/ps4gl/ps4gl.c -o build/check-ps4gl
build/check-ps4gl | tee artifacts/vulkan/frame-state-host.log

cc "${flags[@]}" tests/ps4/check-ps4gl-texture-cache.c -o build/check-ps4gl-texture-cache
build/check-ps4gl-texture-cache | tee artifacts/vulkan/texture-cache-host.log
