# Open GPU stack: triangle and uniform-buffer milestones

2026-09-25. The selected direction is OpenGNM -> Vulkan -> an OpenGL
translation layer -> ScummVM. The standard ScummVM 1.17 package is unchanged.
This is an independent experimental package, not enabled game support.

## Proven locally

- OpenGNM builds for Linux and OpenOrbis. Its host CTest suite passes.
- The open `opengnm-psbc` host compiler builds. The PS4 compiler regression suite
  checks vertex, fragment and two-UBO binary structures/CRCs, plus six rejected
  resource layouts. The earlier eight-stage upstream suite accepted a compute
  SSBO shader using null descriptors; that case now correctly fails compilation.
  Structural checks alone are not GPU execution.
- A PS4-targeted shader compiler archive and Vulkan driver archives build.
  Building a shared library is not proof that all runtime imports resolve.
- GFX7 vertex and fragment shaders compiled by our patched compiler render an
  orange-to-yellow triangle through GNM in shadPS4. The probe completes 120
  submission/fence/flip iterations. It imports libkernel, libSceGnmDriver and
  libSceVideoOut, **neither Piglet nor Shacc**.
- 149 interior pixels match the expected interpolated color, including sRGB
  conversion, and five exterior pixels match the CPU-prefilled gray background.
  The same oracle rejects the earlier broken-compiler black triangle.

- A separate GFX7 test reads two uniform buffers, including a nonzero member
  offset, using full 64-bit descriptor-table pointers. All 149 interior samples
  match `inputColor * tint + bias`, with sRGB conversion. It also completes 120
  submission/fence/flip iterations.

The background remains deliberately CPU-prefilled to isolate drawing.
No texture sampling, GPU clear, full Vulkan rendering, OpenGL context,
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

Current baseline package SHA-256:
`6d654861f1091f18ff394c4a48ea7c0ae16330226d2f8648cfa5fbf304829728`.
The original triangle checkpoint at `8717b12` had SHA-256
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

## Uniform-buffer checkpoint

```sh
scripts/build-opengnm-uniforms.sh
xvfb-run -a -s '-screen 0 1280x720x24' python3 scripts/run-opengnm-probe.py --uniforms
```

The script also builds/checks the baseline stack. Package:
`dist/ScummVM-PS4-OpenGNM-Uniforms.pkg`, title ID `SCVM00007`, SHA-256
`2c05b94aa279b4482fc61a4854143d1f2cf3bfc8bcf6fac7776f0f541fd7a2df`.
Evidence is in `artifacts/opengnm/uniforms/` (image, target/emulator logs,
pixel result, shaders, package log and imports). Host compiler fixtures are in
`artifacts/opengnm/compiler-tests/`.

Three problems were independently resolved:

1. Upstream standalone lowering returned zero descriptors when no Vulkan layout
   was provided. The buffer path now loads the actual `GnmBuffer` descriptor.
2. RADV normally takes a 32-bit descriptor pointer plus a fixed high word.
   OpenGNM supplies a 64-bit pointer. Both SGPR declaration/accounting and the
   descriptor load now use two words; no address high word is hardcoded.
   The test table was at `0x2007d0230`, beyond the 32-bit address range.
3. The fragment shader's descriptor pointer begins at user register 2 in this
   compilation. The test reads `PTR_INDIRECTRESOURCETABLE` metadata instead of
   assuming register 0. The earlier clear helper still assumes register 0 and
   is not enabled or claimed fixed by this milestone.

The explicit standalone contract is set 0, bindings 0–15, each binding at
`binding * 16` in a table of `GnmBuffer` descriptors. Only ordinary UBO access
in PS4 vertex/fragment stages is enabled. Descriptor arrays/nonuniform access,
other descriptor sets, textures, storage buffers and push constants are rejected
with `PSBC_RESULT_UNSUPPORTED_RESOURCE`. The tests exercise those rejections;
resource-free stages remain structurally compilable, not GPU-validated.
This is a deliberately bounded ABI, **not Vulkan descriptor-layout support**.
Before integrating the Vulkan driver, its layout information must become an
explicit compiler input; do not feed arbitrary Vulkan descriptor tables into
this buffer-only path. The 64-bit ABI and the validation apply to the dedicated
patched compiler, including its target archive; the stable ScummVM is unaffected.

## Next gates

1. Add explicit mixed-resource layout information for buffers/images/samplers,
   then sample a known RGBA texture and check its pixels. Do not remove the
   unsupported-resource guards until those resource paths work.
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
