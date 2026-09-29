## Diagnostic candidate: ScummVM Vulkan Ops 1.23

Physical Serial 1.22 completed the first 283-DWORD command and signal, but the
846-DWORD command stopped at EOP waiting. No completion/timeout was visible in
the photographed tail. This weakens, but does not eliminate, the storage reuse
hypothesis; neither shader compilation nor submit return proves GPU completion.

1.23 adds the package marker `scummvm-operation-submit.txt`. Recording captures
operation boundaries (maximum 256 per command buffer; overflow fails recording).
At QueueSubmit it executes successively longer **prefixes** of the original PM4
buffer, always starting with the original hardware initialization. Each prefix
ends at an API operation boundary, is packet-validated, and receives a private
GPU-written completion label with the existing five-second timeout. All PM4 and
inline GPU addresses stay in the original direct allocation. Normal packages
without the marker retain the existing submit path.

An initial disjoint-range experiment completed in shadPS4 but produced a black
image. It was rejected. Prefix replay restores preceding GPU state and the
final candidate displays the launcher. Replaying operations and inserting EOP
flushes deliberately changes timing/cache behavior. This is a startup diagnostic,
not a general Vulkan submission strategy (queries, atomics and other cumulative
side effects must not be assumed replay-safe). Success alone does not identify
one defect; failure identifies the first tested prefix that did not complete.

The 846-DWORD stream was mapped in the emulator:

| Last operation in prefix | Exclusive end (DWORDs) |
| --- | ---: |
| INIT | 256 |
| PipelineBarrier | 283 |
| PipelineBarrier | 310 |
| BeginRenderPass | 405 |
| ClearAttachments | 636 |
| BindPipeline | 765 |
| DrawIndexed | 789 |
| EndRenderPass | 792 |
| PipelineBarrier | 819 |
| PipelineBarrier | 846 |

Install `ScummVM-PS4-Vulkan-Experimental.pkg` and confirm **ScummVM Vulkan Ops
1.23** (SCVM00010). Test the launcher. On failure photograph SCUMMVM GPU in
Diagnose: the last `OP ... PREFIX BEGIN` and `OP ... GPU COMPLETE` narrow the
failing prefix. SCUMMVM START remains useful too. Physical 1.23 is untested.

Local validation: all 221 extracted package payloads audited; fresh isolated
shadPS4 profile reached two successful presents, all startup checkpoints and a
visually inspected launcher screenshot. Evidence: `artifacts/scummvm-vulkan/ops-123-*`.
SHA-256: `ceda23618684e51525b44a65f498b0722004024c1502ff953b6df4f6b2637aa3`.

Full rebuild recipe:
```sh
SCUMMVM_CONTROLLER_ONLY=1 SCUMMVM_SERIAL_SUBMIT=1 SCUMMVM_OPERATION_SUBMIT=1 \
SCUMMVM_APP_VERSION=1.23 SCUMMVM_TITLE='ScummVM Vulkan Ops 1.23' \
scripts/build-scummvm-vulkan.sh
```
This candidate rebuilt the native ICD and relinked the existing full engine,
then used the normal package script. No game assets were added.

## Diagnostic candidate: ScummVM Vulkan Serial 1.22

The user confirmed Probe 0.8's moving stripe and green final image on physical
PS4. Its logs completed all 361 presentations. ScummVM 1.21 still stopped after
the 283- and 846-DWORD submissions. This does not prove either job completed.

1.22 tests the resource-lifetime/scheduling hypothesis. The package includes
`scummvm-serial-submit.txt`; without this marker the normal scheduling path is
unchanged. In the diagnostic path each native submission inside QueueSubmit
(including semaphore wait/signal packets) is followed by a private EOP packet
in a fresh direct-memory allocation. The CPU waits for the GPU to write its
label before returning/reusing storage. Timeout is five seconds; submit errors
or timeout return DEVICE_LOST, with uncertain completion storage retained.
No CPU-generated success label is used. QueueSubmit logs command-buffer and
PM4 addresses, chunk sizes, sequence numbers and GPU completion durations.

This is deliberately serialized and may be slower. It can deadlock/time out
workloads requiring later cross-queue signals; it is a focused diagnostic for
this single-graphics-queue startup, not a production synchronization policy.
It also serializes the shared epilogue packet's reuse, so success would narrow
the cause to scheduling/lifetime effects, not identify one specific buffer.
A hardware fault may still crash the system before a timeout can be logged.

Install `dist/ScummVM-PS4-Vulkan-Experimental.pkg` and verify the PS4 title
**ScummVM Vulkan Serial 1.22** (SCVM00010). First test the launcher. If it crashes,
open Diagnose and photograph SCUMMVM START and SCUMMVM GPU. Look for
`SERIAL DIAG 1.22 enabled=1`, then `COMMAND GPU COMPLETE` or `TIMEOUT`.
The old Diagnose 0.10 reads these same paths. No FTP or mouse is needed.

Rebuild with the full existing recipe:
```
SCUMMVM_CONTROLLER_ONLY=1 SCUMMVM_SERIAL_SUBMIT=1 SCUMMVM_APP_VERSION=1.22 \
SCUMMVM_TITLE='ScummVM Vulkan Serial 1.22' scripts/build-scummvm-vulkan.sh
```
This candidate reused the already configured full engine build: native ICD was
rebuilt, the previous engine ELF was moved aside to force relinking, then the
normal package script built a new eboot and package. All previous 1.21 fixes
remain. Application success on physical PS4 is still unverified.

SHA-256: `199f1863bd359eb9dccbe335c76071d48a0d32468f376286400e614cc0d686f0`.

Local validation: package validation and extraction passed; all 220 payload
files matched the manifest/staging tree, including the diagnostic marker.
The extracted package reached the visible launcher and two successful presents
in shadPS4. Driver logs confirm `SERIAL DIAG 1.22 enabled=1` and actual GPU label
completion for the 283- and 846-DWORD jobs, without a diagnostic timeout.
Evidence: `artifacts/scummvm-vulkan/serial-122-*`. Physical PS4 testing remains
pending; emulator completion does not establish the physical crash is fixed.

> Remaining diagnostic issue: repeated DMA load-op clears in the presentation
> probe left trails in shadPS4 despite the null-memory fix. The new probe uses a
> full-screen shader background to isolate presentation. This issue is not yet
> attributed to driver vs emulator and is not claimed solved by ScummVM 1.21.

## Previous candidate: ScummVM Vulkan Viewport 1.21

Built with the existing ScummVM Vulkan stack plus the viewport/scissor fix
verified by physical UT99 Probe 0.7. Pipeline creation now owns copies of static
viewport/scissor arrays, and binding emits those states. Dynamic states retain
the explicit setter path. The direct PM4 allocation fix from 1.20 remains.
Swapchain load-op clears also use the VideoOut-owned backing address; previously
the clear silently returned because those images have no VkDeviceMemory.
This does not establish that the physical ScummVM crash is fixed.

Build:
```
SCUMMVM_CONTROLLER_ONLY=1 SCUMMVM_APP_VERSION=1.21 \
SCUMMVM_TITLE='ScummVM Vulkan Viewport 1.21' scripts/build-scummvm-vulkan.sh
```

Package: `dist/ScummVM-PS4-Vulkan-Experimental.pkg`, SCVM00010.
Install and verify title **ScummVM Vulkan Viewport 1.21**. First test the launcher,
then Thimbleweed Park with existing external USB game data. The same bootstrap
and driver log paths work with Diagnose 0.10 or 0.11. Physical testing remains
pending for this application build. Package hashes below for 1.20 are historical.

Local 1.21 validation: all 219 packaged files audited against staging; extracted
PKG runs with two successful presents and a visually inspected launcher in a
fresh emulator profile. Controller-only marker and direct PM4 log confirmed.
Evidence: `artifacts/scummvm-vulkan/viewport-121-audit.log`,
`viewport-121-test.txt`, `viewport-121-launcher.png`.
SHA-256: `bea829fa90faf4699ed61cb42d2c01c3d8360ef124725137198468738027c38f`.


# Direct GPU command memory: hardware finding and candidates

## Physical evidence (2026-09-28)

The user's physical PS4 passed **Vulkan Probe 0.5**: raw EOP, init+EOP,
separate init/EOP submissions, Vulkan empty submission, DMA fill, memory barrier,
and all 64 readback words. Probe 0.4 had failed the Vulkan empty submission even
with SFENCE and SubmitDone. The distinguishing successful probe change copied
PM4 command bytes from ordinary heap storage into directly allocated GPU memory.
This is strong evidence for command-buffer memory accessibility as a cause of
that startup failure. It is not proof of full application rendering or gameplay.

## General fix

All three local native ICD variants now record PM4 directly into WC Garlic direct
memory with CPU/GPU read-write permissions and 64 KiB alignment. Buffers retain
a stable address through recording, submission and command-pool reuse. This
preserves commands that refer to data inside their own recording buffer; the
probe's limited relocation shortcut is removed from the UT99 driver.

Each command buffer owns its direct-memory handle. Pool destruction, cache trim,
free-list overflow and error cleanup release it through the direct-memory API.
Partial allocation failures remove tracked command buffers before freeing them.
Host-only builds retain their ordinary allocator. SFENCE publication is retained;
SubmitDone is retained/added at the existing completion boundaries. The projects'
other renderer features and queue behavior are not made byte-identical.

`scripts/test-pm4-lifetime.sh` tests the actual PS4 allocation code with tracked
host stubs and ASan/UBSan: allocation, stable reuse, destruction of active buffers,
trimming cached buffers, and rollback after a partial allocation failure. This
checks CPU ownership and lifetime, not real GPU execution.

## Candidates

| Application | Visible version | USB filename |
|---|---|---|
| ScummVM | ScummVM Vulkan Direct PM4 1.20 | ScummVM-PS4-Vulkan-Experimental.pkg |
| UT99 | UT99 Orbis Direct PM4 0.9 | ut99-orbis-hardware.pkg |
| Doom 3 | Doom 3 PS4 - Direct PM4 0.02 | Doom3-PS4-Menu-Test-0.02.pkg |
| Basic probe using general allocator | PS4 Vulkan Probe 0.6 | ut99-vulkan-probe.pkg |
| Log viewer | PS4 Vulkan Diagnose 0.11 | ut99-orbis-diagnostic.pkg |

Start the probe, then ScummVM's launcher, UT99 and Doom 3. On a crash open the
log viewer again to reload its snapshots; photograph the relevant engine/start
and GPU pages. The viewer now includes `/data/doom3-client/dudelog.txt` and
`/data/client-vulkan.log` (pages 7/8 and 8/8). Log files may be from older runs.
Do not relaunch a failing app before photographing its logs, as it may overwrite them.

Only Probe 0.5 has been physically verified at this checkpoint. The general
allocator and the three new application packages still need physical testing.
Probe 0.6 has no draw/shader tests. Doom 3 remains a finite 60-frame menu test
with a five-second capture hold, not a completed gameplay port.

## Rebuild ScummVM

```sh
SCUMMVM_CONTROLLER_ONLY=1 SCUMMVM_APP_VERSION=1.20 \
 SCUMMVM_TITLE='ScummVM Vulkan Direct PM4 1.20' scripts/build-scummvm-vulkan.sh
scripts/test-pm4-lifetime.sh
```
The script checks the native patch and rebuilds the ICD before linking.
Title SCVM00010 remains separate from ordinary ScummVM. No mouse is required.
The extracted package was audited against all 219 manifest files and reached the
visible launcher with successful presents in shadPS4. Logs confirm direct PM4
allocation. Evidence: `artifacts/scummvm-vulkan/direct-120-*`.

## Built package SHA-256

- `ScummVM-PS4-Vulkan-Experimental.pkg`: `825c42af9f9b52d9987447c5efc3e893ab9d79b85b74b254096c77615baba9e1`

## 2026-09-29: Clear 1.24 hardware candidate

The user reproduced ScummVM's first-frame stall with Probe 0.9: a cold
`vkCmdClearAttachments` before any application draw did not complete its GPU
fence. Probe 0.10 replaced that clear with an ordinary compiled fullscreen
pipeline. The user confirmed the complete physical test, from initial magenta
through presentation to final magenta. This isolates a failing clear path; it
does not identify a single faulty register or shader instruction.

ScummVM 1.24 now uses that pipeline approach inside the native color clear
implementation, including partial rectangles. It restores the application
pipeline, viewport/scissor, stencil state, and vertex/fragment push constants.
The color shader uses specialized literals instead of the old inline color
descriptor. Variants are cached until device destruction, bounded at 128 unique
color/format combinations; exceeding the bound reports allocation failure.
This is a diagnostic candidate, with first-use shader compilation overhead.
Depth/stencil clear and existing linear load-op DMA paths are unchanged.

Serial GPU completion diagnostics remain enabled; operation-prefix replay is
disabled. Title: `ScummVM Vulkan Clear 1.24`, title ID `SCVM00010`.

Validation:
- Native driver and ScummVM rebuilt successfully.
- Extracted package: all 220 manifest files audited.
- Extracted package in shadPS4: visible launcher and two successful presents;
  `META CLEAR 1.24` logged.
- Native rect-clear regression: 1,230 pixel checks and lifecycle cleanup pass.
- The older GL framebuffer regression reached/presented frame 0 but then hit
  an emulator access violation during cleanup; it is NOT a passing regression.
  Its obsolete exact snapshot-count assertion was replaced by the intended
  check that an undefined texture causes no texture upload, with line diagnostics.
- SDL regression build is blocked by missing legacy HPL source fixture.
- ScummVM 1.24 physical-console validation remains pending.

Evidence: `artifacts/scummvm-vulkan/clear-124-*` and
`artifacts/vulkan/rectclear/`. Regenerate embedded shaders with
`scripts/generate-meta-clear.py`; sources/header live in the durable native patch.

Rebuild/package settings: `SCUMMVM_CONTROLLER_ONLY=1`,
`SCUMMVM_SERIAL_SUBMIT=1`, `SCUMMVM_OPERATION_SUBMIT=0`,
`SCUMMVM_APP_VERSION=1.24`, `SCUMMVM_TITLE='ScummVM Vulkan Clear 1.24'`
with `scripts/build-scummvm-vulkan.sh`.

Package SHA-256: `2bb17d613016841290ca36a7655f002e44e8d0bd7b10bde593eb842bbf14cbbc`.
USB copy was read back and matched this SHA-256; the prior package was retained.
