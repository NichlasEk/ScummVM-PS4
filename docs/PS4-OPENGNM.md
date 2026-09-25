# Open GPU stack: triangle, uniform-buffer and texture milestones

2026-09-25. The selected direction is OpenGNM -> Vulkan -> an OpenGL
translation layer -> ScummVM. The standard ScummVM 1.17 package is unchanged.
This is an independent experimental package, not enabled game support.

## Proven locally

- OpenGNM builds for Linux and OpenOrbis. Its host CTest suite passes.
- The open `opengnm-psbc` host compiler builds. The PS4 compiler regression suite
  checks vertex, fragment, two-UBO and texture binary structures/CRCs, plus
  rejected resource layouts. The earlier eight-stage upstream suite accepted a compute
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
No GPU clear, full Vulkan rendering, OpenGL context,
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
`c905f0ec94dbc4a5f2cd99ba5745a64dd8b19918c1f2d1c337e7b7a8fedaef92`.
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
`1e64358a81ef3ec17d72aafbf890d1c13050071b77cf1b1d04a3c0454046f95e`.
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
`binding * 16` in a table of `GnmBuffer` descriptors. Ordinary UBO access is supported in PS4 vertex/fragment stages; the later
texture checkpoint adds combined sampler2D access in fragment stages. Descriptor arrays/nonuniform access,
other descriptor sets, storage buffers and push constants are rejected
with `PSBC_RESULT_UNSUPPORTED_RESOURCE`. The tests exercise those rejections;
resource-free stages remain structurally compilable, not GPU-validated.
The texture checkpoint below extends this ABI with a separate texture bank.
This is a deliberately bounded ABI, **not Vulkan descriptor-layout support**.
Before integrating the Vulkan driver, its layout information must become an
explicit compiler input; do not feed arbitrary Vulkan descriptor tables into
this standalone path. The 64-bit ABI and the validation apply to the dedicated
patched compiler, including its target archive; the stable ScummVM is unaffected.

## Texture/sampler checkpoint

```sh
scripts/build-opengnm-textures.sh
xvfb-run -a -s '-screen 0 1280x720x24' python3 scripts/run-opengnm-probe.py --textures
```

Independent package `dist/ScummVM-PS4-OpenGNM-Textures.pkg`, title `SCVM00008`.
SHA-256: `92a79e1f3b9d7fdd9baa567bffa23903c80c7b09489460593b9bf540b1edaf33`.
Evidence is in `artifacts/opengnm/textures/`.

A generated 2x2 RGBA8 UNORM image is sampled with nearest filtering above the
middle of the framebuffer and bilinear filtering below. Both samplers clamp to
edge; UVs deliberately cross the [0,1] bounds. Two UBOs supply tint and bias.
Sampled alpha multiplies RGB before output, making all four sampled channels
observable even through an opaque presentation capture. The CPU oracle computes
sampling, UBO arithmetic, alpha modulation and sRGB conversion independently.
It checks 208 nearest + 369 bilinear pixels (including 333 outside-UV samples),
plus five CPU-prefilled background pixels. This does not test framebuffer alpha
preservation or alpha blending.

The standalone ABI retains UBOs at `binding * 16`. Combined image/sampler slots
are in a separate bank at `256 + binding * 64`: a 32-byte `GnmTexture` followed
by a 16-byte `GnmSampler`. Full 64-bit table pointers and shader metadata select
the SGPRs. Public constants are in the patched `psbc_compile.h`. The GPU test
uses bindings 3 and 7, exercising actual slot offsets and differing samplers.
Texture support is bounded to non-array floating sampler2D in set 0 (bindings
0–15), fragment stages, ordinary sampling or explicit LOD. Cube/array/integer/
shadow textures, queries, storage images and other unsupported resources retain
explicit compilation failures. The GPU test exercises explicit LOD 0 and one
mip; implicit sampling has a host compile check, not its own pixel test.

OpenGNM texture fixes captured by host regression:

- `numfragments` is a count; shifting it again produced a two-sample allocation
  for a single-sample texture. This changed the computed row pitch and lost the
  second texel row in the GPU test.
- `lastarray` is inclusive and must be `numslices - 1`.
- Byte-size calculation initializes its outputs and counts array layers once,
  separately from 3D volume depth. Repeated calls and a three-layer descriptor
  are checked on the host; texture-array sampling is still not enabled.

The probe uses `DISPLAY_LINEAR_ALIGNED`, with the row pitch and allocation size
returned by OpenGNM. shadPS4 does not handle `ArrayLinearGeneral` in its
[image-size path](https://github.com/shadps4-emu/shadPS4/blob/main/src/video_core/texture_cache/image_info.cpp).
No emulator source or proprietary graphics module was patched or introduced.

## Next gates

1. The bounded Vulkan set-0 resource bridge now validates per-stage layout
   bindings and passes texture/UBO pixel tests; see [PS4-VULKAN.md](PS4-VULKAN.md).
   Extend upload/transition support before broadening the descriptor ABI.
2. Run the native probe on physical PS4; retain the emulator checks as a
   separate regression gate.
3. The first native Vulkan draw/presentation/lifecycle gate now passes in
   shadPS4, with real portable compiler dependencies. See
   [PS4-VULKAN.md](PS4-VULKAN.md) for reproduction and the still-unvalidated
   optimal-image uploads and overlapping submission paths.
4. Compare those features with the chosen OpenGL translator's requirements;
   integrate only after real Vulkan tests pass. Then audit ScummVM launcher,
   TWP shaders/effects, input, audio, transitions and save/load.

There is no claim yet of a complete OpenGL replacement or Vulkan conformance.
