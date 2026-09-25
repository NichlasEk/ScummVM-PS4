# Open GPU stack: first rendering milestone

2026-09-25. The selected direction is OpenGNM -> Vulkan -> an OpenGL
translation layer -> ScummVM. The standard ScummVM 1.17 package is unchanged.
This is an independent experimental package, not enabled game support.

## Proven locally

- OpenGNM builds for Linux and OpenOrbis. Its host CTest suite passes.
- The open `opengnm-psbc` host compiler builds. Its eight upstream shader-stage
  structural tests pass (these are binary format/CRC checks, not GPU execution).
- A PS4-targeted shader compiler archive and Vulkan driver archives build.
  Building a shared library is not proof that all runtime imports resolve.
- GFX7 vertex and fragment shaders compiled by our patched compiler render an
  orange-to-yellow triangle through GNM in shadPS4. The probe completes 120
  submission/fence/flip iterations. It imports libkernel, libSceGnmDriver and
  libSceVideoOut, **neither Piglet nor Shacc**.
- 149 interior pixels match the expected interpolated color, including sRGB
  conversion, and five exterior pixels match the CPU-prefilled gray background.
  The same oracle rejects the earlier broken-compiler black triangle.

The background is deliberately CPU-prefilled: it isolates drawing from the
separate uniform-buffer/clear-shader path, which rendered black in the earlier
test. No texture sampling, GPU clear, full Vulkan rendering, OpenGL context,
Thimbleweed execution, or physical console result is claimed yet.

## Reproduce

Dependencies on the Linux build host: clang/clang++, LLD, llvm-ar, CMake, Make,
Python with Mako/PyYAML (and Pillow for image checks), glslc, OpenOrbis SDK.
The runner additionally uses shadPS4 SDL, Xvfb, libX11 and libXtst.

```sh
scripts/build-opengnm-probe.sh
xvfb-run -a -s '-screen 0 1280x720x24' python3 scripts/run-opengnm-probe.py
```

The emulator runner uses an isolated `.tools/xdg-opengnm` profile. The validated
profile has 1280x720 output and the default F12 framebuffer-capture binding.
It captures shadPS4's game framebuffer, not an X11 desktop screenshot, then
runs `tests/ps4/check-opengnm-triangle.py`. A missing or incorrect image fails
the check. The runner terminates the emulator after capture; wrapper process
exit is not used as a substitute for guest log and pixel validation.

Artifact: `dist/ScummVM-PS4-OpenGNM-Probe.pkg`, title ID **SCVM00006**.
Its SHA-256 sidecar accompanies it. The original upstream FGNM00000 identity
was used only during initial debugging. Physical PS4 execution is still needed;
this probe targets base GFX7, not a validated Pro/NEO mode.

Validated package SHA-256:
`38826e4c8928052bf476165c70772a510dcc818dc3fdc8f06df4a75b7d47bc48`.

Evidence under `artifacts/opengnm/`: `triangle-gpu.png`, `pixel-test.log`,
`triangle-result.txt`, `triangle-emulator.log`, `triangle-imports.txt`,
`gnm-host-tests.log`, `psbc-verify.log`, and individual build logs.
The guest report is `/data/scummvm-opengnm-triangle.log`.

## Pinned source and local fixes

The build script pins the umbrella repo to
`2de9fc54ca33b39baa0ace59c6b0059e5de94a63`, including OpenGNM
`c51243f333a1a964894903da080a508f7af4d91f`, shader compiler
`a92a1228ea3a64e4be9f0e61c2a65a5aa7ffed92`, and examples
`ffd055563e5b849ebef1b276ba29e730199670f8`.
Vulkan is deliberately advanced to `f4d940b723771b3fe637e8ac0704bb5d6ab19622`:
the umbrella's older revision failed compilation with conflicting stub types.
Vulkan/SPIR-V headers are also pinned in the script.

- `prepare-opengnm-psbc.py` supplies the missing host configuration and Mesa
  code-generation steps (format tables, built-in types, shader stats, Vulkan
  casts and AMD packet definitions). The host hash implementation is portable
  Blake3; ARM NEON sources cannot be built on this x86-64 host.
- `psbc-build.patch` fixes compiler-info pointer types, includes the generated
  format table, and restores RADV's output-linking step for standalone legacy
  GFX7/GFX8 vertex shaders. Without that pass, ACO dropped all color varyings
  while the output container still advertised semantics: positions rendered
  but the triangle was black. Pixel checks, not CRC checks, caught this.
- `gnm-init.patch` zero-initializes the command buffer, including predication
  flags that otherwise had indeterminate values.
- `triangle-probe.patch` initializes graphics/instance/scissor state, corrects
  submit pointer types, logs explicit stages, polls a volatile GPU fence with
  a timeout, and runs a bounded number of frames. The EOP uses INT_SEL=0 with
  DATA_SEL=2: the probe polls the memory label and needs no interrupt. shadPS4's
  SignalFence implementation aborted on upstream's INT_SEL=3. This adjustment
  is local to the probe, not a change to the library's general EOP semantics.

Patches apply only to the dedicated `.tools/opengnm-stack` checkout. The SDK
is not modified. Sources/licenses remain available in the pinned checkouts:
[OpenGNM stack](https://github.com/PS4-OpenGNM/opengnm-stack),
[compiler](https://github.com/PS4-OpenGNM/opengnm-psbc),
[Vulkan driver](https://github.com/PS4-OpenGNM/vulkan-ps4),
[examples](https://github.com/PS4-OpenGNM/freegnm-examples).

## Next gates

1. Verify descriptor/user-SGPR layout with a uniform color, then a known RGBA
   texture and sampler. Require pixel checks for both before adding effects.
2. Run the native probe on physical PS4; retain the emulator checks as a
   separate regression gate.
3. Link a real Vulkan probe against the target archives, resolve runtime
   dependencies, test drawing/presentation and query advertised features.
   `llvm-nm -u libvulkan_ps4.so` currently includes unprovided Blake3 SIMD
   implementations and Mesa helpers; the `.so` build permits unresolved
   references. Those must be resolved with actual implementations/portable
   build selection before claiming a loadable Vulkan runtime.
4. Compare those features with the chosen OpenGL translator's requirements;
   integrate only after real Vulkan tests pass. Then audit ScummVM launcher,
   TWP shaders/effects, input, audio, transitions and save/load.

There is no claim yet of a complete OpenGL replacement or Vulkan conformance.
