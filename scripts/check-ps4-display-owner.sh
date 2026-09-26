#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
mkdir -p build artifacts/vulkan
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
 -Icompat/ps4gl -I.tools/opengnm-stack/Vulkan-Headers/include \
 tests/ps4/check-ps4gl-display.c compat/ps4gl/ps4gl-display.c -o build/check-ps4gl-display
build/check-ps4gl-display | tee artifacts/vulkan/display-owner-host.log
