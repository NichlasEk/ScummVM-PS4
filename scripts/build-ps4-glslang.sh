#!/usr/bin/env bash
# Pinned in-process GLSL -> SPIR-V compiler; no host libraries in the PS4 archive.
set -euo pipefail
root=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
src="$root/.tools/glslang"
revision=fa9c3deb49e035a8abcabe366f26aac010f6cbfb
if [[ ! -d "$src/.git" ]]; then
 git clone --no-checkout https://github.com/KhronosGroup/glslang.git "$src"
 git -C "$src" checkout --detach "$revision"
fi
[[ $(git -C "$src" rev-parse HEAD) == "$revision" ]]
patch="$root/patches/opengnm/glslang-orbis.patch"
if ! git -C "$src" apply --reverse --check "$patch" 2>/dev/null; then
 git -C "$src" apply --check "$patch"
 git -C "$src" apply "$patch"
fi
sdk=${OO_PS4_TOOLCHAIN:-/opt/openorbis/OpenOrbis/PS4Toolchain}
build="$root/build/glslang-orbis"
mkdir -p "$build" "$root/artifacts/glslang"
cat > "$build/toolchain.cmake" <<CMAKE
set(CMAKE_SYSTEM_NAME FreeBSD)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_AR llvm-ar)
set(CMAKE_RANLIB llvm-ranlib)
set(CMAKE_C_COMPILER_TARGET x86_64-pc-freebsd12-elf)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-freebsd12-elf)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_C_FLAGS "-fPIC -D_GNU_SOURCE -D__ORBIS__ -DORBIS -D__PS4__ -isysroot $sdk -I$sdk/include -I$sdk/include/orbis" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS "-fPIC -D_GNU_SOURCE -D__ORBIS__ -DORBIS -D__PS4__ -isysroot $sdk -I$sdk/include -I$sdk/include/orbis -nostdinc++ -I$sdk/include/c++/v1" CACHE STRING "" FORCE)
CMAKE
cmake -S "$src" -B "$build" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$build/toolchain.cmake" \
 -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DBUILD_EXTERNAL=OFF \
 -DENABLE_GLSLANG_BINARIES=OFF -DENABLE_HLSL=OFF -DENABLE_OPT=OFF \
 -DENABLE_SPVREMAPPER=OFF -DENABLE_PCH=OFF -DENABLE_RTTI=OFF -DENABLE_EXCEPTIONS=OFF \
 -DGLSLANG_TESTS=OFF -DGLSLANG_ENABLE_INSTALL=OFF \
 > "$root/artifacts/glslang/configure.log" 2>&1
cmake --build "$build" -j"${JOBS:-8}" > "$root/artifacts/glslang/build.log" 2>&1
# Merge archives to make target linking independent of CMake target ordering.
{
 echo 'CREATE libglslang-ps4.a'
 for archive in glslang/libglslang.a glslang/libMachineIndependent.a glslang/libGenericCodeGen.a \
 glslang/OSDependent/Unix/libOSDependent.a SPIRV/libSPIRV.a glslang/libglslang-default-resource-limits.a; do
  echo "ADDLIB $build/$archive"
 done
 echo SAVE
 echo END
} > "$build/merge.mri"
(cd "$build" && llvm-ar -M < merge.mri)
