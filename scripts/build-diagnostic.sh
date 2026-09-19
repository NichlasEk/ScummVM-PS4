#!/usr/bin/env sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
toolchain=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}

if [ ! -d "$toolchain" ]; then
	echo "OpenOrbis toolchain not found: $toolchain" >&2
	exit 1
fi

export OO_PS4_TOOLCHAIN="$toolchain"
exec make -C "$project_dir" diagnostic

