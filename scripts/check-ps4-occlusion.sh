#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build artifacts/vulkan
flags=(-std=c11 -Wall -Wextra -Wno-unused-parameter -fsanitize=address,undefined -Icompat/ps4gl -I.tools/opengnm-stack/Vulkan-Headers/include)
cc "${flags[@]}" tests/ps4/check-ps4gl-query.c -o build/check-ps4gl-query
build/check-ps4gl-query | tee artifacts/vulkan/query-state-host.log
cc "${flags[@]}" -ffunction-sections -fdata-sections -I.tools/opengnm-stack/vulkan-ps4/include -I.tools/opengnm-stack/opengnm/include tests/ps4/check-ps4-occlusion.c -Wl,--gc-sections -o build/check-ps4-occlusion
build/check-ps4-occlusion | tee artifacts/vulkan/query-driver-host.log
