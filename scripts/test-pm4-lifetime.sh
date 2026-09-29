#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
stack="$root/.tools/opengnm-stack"
mkdir -p "$root/build/tests" "$root/artifacts"
clang -std=c11 -O1 -g -DVK_USE_PLATFORM_PS4 -ffunction-sections -fdata-sections \
 -fsanitize=address,undefined -Wl,--gc-sections \
 -I"$stack/vulkan-ps4/include" -I"$stack/Vulkan-Headers/include" \
 -I"$stack/opengnm/include" -I"$stack/opengnm-psbc/libpsbc" \
 "$root/tests/ps4/pm4-lifetime.c" "$stack/vulkan-ps4/src/vk_ps4_command.c" \
 -o "$root/build/tests/pm4-lifetime"
"$root/build/tests/pm4-lifetime" | tee "$root/artifacts/pm4-lifetime-test.txt"
