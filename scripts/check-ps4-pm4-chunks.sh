#!/usr/bin/env bash
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
mkdir -p build artifacts/vulkan
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
 -I.tools/opengnm-stack/vulkan-ps4/include tests/ps4/check-ps4-pm4-chunks.c \
 -o build/check-ps4-pm4-chunks
build/check-ps4-pm4-chunks | tee artifacts/vulkan/pm4-chunks-host.log
