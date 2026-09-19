#!/usr/bin/env bash
set -euo pipefail
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
dest="$project_dir/.tools/shadps4"
mkdir -p "$dest"
archive="$dest/release.zip"
if [[ ! -f "$archive" ]]; then
    curl -fL --retry 3 \
        https://github.com/shadps4-emu/shadPS4/releases/download/v.0.18.0/shadps4-linux-sdl-0.18.0.zip \
        -o "$archive.part"
    mv "$archive.part" "$archive"
fi
# Digest published by GitHub's release API for this exact asset.
printf '%s  %s\n' 3cf0f669c089411775a2434a41ca9c391fb7d24c1a1521ee45dd1d36f390c4a4 "$archive" | sha256sum -c -
unzip -o "$archive" -d "$dest"
chmod +x "$dest/Shadps4-sdl.AppImage"
