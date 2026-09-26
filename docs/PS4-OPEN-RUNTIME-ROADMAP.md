# Roadmap: source-built PS4 runtime and broader GL support

Status: proposal, 2026-09-26. No runtime modules have been removed or replaced
as part of writing this document. Work below must use separate package staging
and test profiles, preserving the current playable build and saves.

## Objective and boundary

Build every component distributed in our application package from available
open-source code, with pinned revisions, license notices and reproducible build
instructions. This does not replace PS4 firmware, kernel services or game data.
The Vulkan/OpenGNM graphics route already avoids Piglet and Shacc.

See [external module setup](../external/ps4-runtime/README.md) for current paths.
`scripts/package-scummvm.sh` currently copies three prebuilt modules from the
SDK. Their presence in the package alone does not prove they are used.

## 1. Establish what is actually required

Record hashes and provenance of the local modules, application imports and
transitive dependencies. Inspect startup constructors and explicit module loads
as well as ELF `NEEDED` entries. Capture module load logs in shadPS4; emulator
provided implementations can conceal dependencies required on hardware.

Create fresh isolated packages omitting exactly one module at a time. Do not
reuse a staging tree containing old files. Test boot, launcher, game startup,
audio, scene transitions, save/load and shutdown. Repeat on physical PS4 before
declaring a module unnecessary. Preserve baseline and variant evidence.

## 2. Candidate replacements

| File | First investigation | If it is genuinely needed |
| --- | --- | --- |
| `libc.prx` | Determine which imports, if any, require it despite our static OpenOrbis musl runtime. | Use source-built musl functionality where possible; otherwise implement only the required exported ABI in a source-built module. |
| `libSceFios2.prx` | Determine whether application, SDL, startup or other modules invoke FIOS at all. | Implement the observed file-I/O contract over kernel/POSIX operations, including async completion, cancellation and error semantics where used. |
| `right.sprx` | Identify its actual role and whether packaging tools, loader or runtime require it. Its filename and SDK sample directory are insufficient evidence. | If it is ordinary callable code, implement the required interface from documented behavior. If it is a platform-format or signing requirement, investigate the toolchain/package format first; a dummy module is not a demonstrated replacement. |

Prefer removing an unused dependency. Do not build speculative full Sony API
implementations. Do not claim that musl is automatically a drop-in replacement
for a Sony PRX: exports, calling convention, TLS, allocation ownership and
threading must agree across module boundaries.

The current allocator investigation is directly relevant: imported kernel
threads and the static libc need consistent locking. Resolve and stress-test
that integration before declaring the libc route complete. Test concurrent
malloc/realloc/free, alignment, exhaustion and shutdown without suppressing
real failures.

## 3. Delivery gates

For each removed/replaced module:

1. Commit its dependency audit and minimal required API list.
2. Pin any source dependency and document its build and license.
3. Add focused behavioral tests for the implemented contract.
4. Build a fresh package and inspect its exact contents and imports.
5. Pass the isolated emulator flow and physical-console flow above.
6. Update application, diagnostic and probe packaging scripts together.

Only then remove the corresponding external placeholder. Finish with a clean
checkout build that cannot silently borrow binaries from SDK sample folders.
Publish a package manifest distinguishing source-built application components
from imports supplied by the console firmware.

## Graphics status and next games

The current bridge is a limited OpenGL API implemented through Vulkan/OpenGNM,
not a complete or conformant OpenGL implementation. It deliberately reports
`0.0 PS4GL experimental subset`. TWP has reached gameplay in shadPS4; the saved
`artifacts/scummvm-vulkan/twp-run5-summary.json` reports zero warnings in that
sampled run. This is not full-game coverage or physical PS4 validation.

Depth testing/attachments are currently unsupported in `compat/ps4gl/ps4gl.c`.
Memory/threading stabilization is also still in progress. Do not treat the
current development worktree as a finished release solely because a previous
package played successfully.

Recommended test order (engineering judgment, not compatibility promises):

| Game | Purpose | Local renderer source |
| --- | --- | --- |
| Thimbleweed Park | Keep the existing shader, texture, UI and save/load regression baseline. | `engines/twp/` |
| Myst III: Exile | Already available locally; exercise the shader renderer with panoramic scenes, transitions and video. Earlier software-renderer success does not validate this path. | `engines/myst3/gfx_opengl_shaders.cpp` |
| The Longest Journey | Exercise 3D actors, depth and scene composition; expect missing functionality to surface. | `engines/stark/gfx/opengl*.cpp` |
| Grim Fandango (original game data) | Exercise another 3D engine, geometry, depth and shader state. | `engines/grim/gfx_opengl_shaders.cpp` |

Renderer sources above are under `.tools/scummvm-vulkan-src/`. Before running
each game, inventory its GL calls and shader requirements against our loader.
Implement missing features with focused native pixel tests, then use the real
game to verify integration. Select `opengl_shaders` explicitly where supported
and verify the selected renderer in logs: software fallback is not a GL pass.
The legacy fixed-function OpenGL route may need a separate compatibility layer.

ScummVM documents the distinction between OpenGL, OpenGL with shaders and
software rendering in its [graphics settings documentation](https://docs.scummvm.org/en/latest/settings/graphics.html).
Upstream support for Myst III, The Longest Journey and Grim Fandango is recorded
in the [2.5.0 release notes](https://downloads.scummvm.org/frs/scummvm/2.5.0/ReleaseNotes.html).
Upstream game support does not establish support in this experimental PS4 port.
