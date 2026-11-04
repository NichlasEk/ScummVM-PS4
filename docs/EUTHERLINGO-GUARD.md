# EutherLingo Guard: first bounded implementation

Status: 2026-11-04. This is diagnostic infrastructure and VM boundary hardening,
not a claim that Jönssonligan: Jakten på Mjölner is repaired. No original game
files were changed. No new game-specific exception or general rollback was added.

## Investigation

The runtime under test is the prepared ScummVM 2026.4.0 Director engine, including
this repository's existing `director7.patch`. The upstream tree under `.tools`
is shared with the historical private checkout. This work was prepared separately
under `build/eutherlingo-source`; the shared source/build/profile was not edited.

| Area | Existing implementation and implications |
| --- | --- |
| Interpreter | `lingo.cpp::execute` dispatches threaded instructions; `lingo-code.cpp::LC::call`, `pushContext`, and `popContext` manage arguments, handlers, return values and saved caller state. |
| Per-window state | `LingoState` owns operand/call stacks, locals, current script/context and `me`. Window calls, `tell`, freeze and play queues can transfer or suspend this state. A shallow snapshot cannot restore the engine's external effects. |
| Script errors | `lingoError` marks `_abort`; `execute` unwinds using `popContext(true)`. `scummvmAssertError` has a separate expected-error path. Strict Lingo mode is already fatal. Returning from `lingoError` is not a C++ exception or an immediate nonlocal jump. |
| Objects | `Datum`, `Symbol` and script contexts use reference counting. Disposed ScriptContext property access already stops with `error`; some disposed method lookups return an undefined symbol. Reference counts are not proof that arbitrary native pointers are valid. |
| Arguments | Handlers deliberately accept extra arguments and pad missing named arguments with VOID. Builtin arity/type handling is separate. These semantics must remain intact. |
| Resources | Cast/member accessors and executeScript handle several missing-resource cases; some are legal probes. Existing Lingo errors now include history, while missing-script requests only add a ring event. Warnings that bypass these paths are not all intercepted. |
| Stack and PC | `pop`/`peek` previously relied on assertions. Named dispatch indexed arguments before validating available operands. The execute loop read the first instruction before a PC bounds check. No general configurable call-depth ceiling was found in these paths. |
| Compatibility | `lingo-patcher.cpp` matches game ID, platform, extra/version description, movie, script type, cast/member ID and (for line patches) original text. `ps4-jonsson.h` separately gates this port's existing experimental Jönsson behavior by PS4, game ID and opt-in. Use the existing patch tables for a demonstrated script correction, not a new title check in the generic interpreter. |

Local game data was found. The Windows detector file is 2,432,602 bytes; its
last-5000-byte MD5 is `f9299bd39817cbfadad4affa1187777d`, matching the existing
Swedish `jonssonligan1` Director 7.02 entry. This identifies the available release;
it does not establish a new crash reproduction.

Existing evidence in `PS4-JONSSONLIGAN.md`, `PS4-JONSSON-030.md`,
`PS4-JONSSON-031.md` and local historical logs establishes several port-side
problems: coordinate-list comparison causing unbounded search growth, incorrect
helper-window sound ownership, video sizing and redundant presentation. The
historical font-encoding log contains a `std::bad_alloc` termination. These are
not evidence of faults in the original game's intended logic. The reported
physical PS4 CE-34878-0 has no recovered native stack, and its cause remains
unproven. No new original-game defect was established in this iteration.

## Implemented boundary and diagnostic policy

* A heap-allocated, fixed 128-entry history is enabled by `eutherlingo_guard=true`.
  Each entry copies a handler name (up to 63 bytes), event, PC, operand count,
  call depth and opaque state/context/object addresses. It retains no Datum,
  Symbol or object ownership and never dereferences stored historical addresses.
* Events cover dispatch, handler entry/return, execute/resume entry, freeze,
  abort/unwind, missing scripts and faults. This is call/transition history,
  not a full opcode trace. PCs on call entries are caller positions; the final
  fault entry records the current VM position. Addresses are process-local
  correlation tokens and may be reused, not permanent object IDs.
* Lingo errors export the ring through the existing ScummVM logger. At most
  three nonfatal history reports are emitted per Lingo instance; fatal reports
  bypass that budget. Existing engine warnings are not suppressed. No normal
  execution log stream or per-event disk write is added.
* With diagnostics off, no history is allocated, no handler strings are copied
  and no ring report is formatted. The boundary checks remain active. No
  end-to-end performance claim has been measured.
* Operand pop/peek, available call arguments, builtin target presence, empty
  call frames, return stack balance, and instruction dispatch positions are
  checked before their relevant unsafe accesses/mutations. PC is rechecked
  after the debugger hook. Invalid invariants use ScummVM's non-returning
  `error()` path, which may show its debugger before terminating.
* Null OBJECT property access in `getObjectProp`/`setObjectProp` is stopped;
  existing disposed-property fatal errors receive history. This is not a
  universal native pointer validator or a use-after-free detector.
* `eutherlingo_max_call_depth=N` optionally stops before allocating the next
  handler frame at depth N. Zero (default) leaves the existing behavior. It
  limits Lingo handler frames, not every C++ recursive path or ancestor cycle.
* Existing recoverable `lingoError` behavior is preserved and tested. There is
  no new arbitrary-error recovery, checkpoint, exception swallowing or claim
  that arbitrary engine state can be restored.

## Configuration and logs

Put these settings in the target's ScummVM INI section, then restart the game:

```ini
eutherlingo_guard=true
eutherlingo_max_call_depth=0
```

A finite limit is useful for controlled recursion probes; the regression uses
8. Do not choose a production ceiling for Mjölner without measuring real scripts.
Diagnostics can be switched off with `eutherlingo_guard=false`.

Reports use the normal ScummVM log (`--logfile` on host), or the console log
captured by `scripts/run-scummvm-shadps4.py`. Search for `EutherLingo Guard:` and
`ELG `. See `EMULATOR.md` for the isolated profile. Export the relevant log file;
there is no separate savegame-format report and no custom filesystem dependency.
Normal ScummVM diagnostics may contain local paths or game text; review them
before public posting.

## Changed files

The durable engine changes are in `experiments/scummvm-2026.4.0/director7.patch`,
following the repository's existing source-patch convention. Within that patch:

| Prepared source | Reason |
| --- | --- |
| `engines/director/lingo/eutherlingo-guard.h` | Bounded value-only ring. |
| `engines/director/lingo/lingo.h` | Guard state and boundary/report method declarations. |
| `engines/director/lingo/lingo.cpp` | Configuration, reports, PC validation and lifecycle/error events. |
| `engines/director/lingo/lingo-code.cpp` | Argument/stack/frame checks and call/return history. |
| `engines/director/lingo/lingo-object.cpp` | Diagnostics before existing disposed-property fatal errors. |
| `engines/director/lingo/lingo-the.cpp` | Null object checks at the two generic property entry points. |

`tests/director7/check-eutherlingo-guard.py` exercises the actual ring header and
extracted boundary methods under ASan/UBSan. Its fatal-error stub throws only
inside the test harness to inspect unchanged pre-fatal state; production does
not throw/catch to recover.

`tests/director7/check-eutherlingo-runtime.py` and `eutherlingo/*.lingo` test the
real host interpreter. All new scripts are explicitly synthetic regressions,
not invented reproductions attributed to Jönssonligan.

The preceding emulator setup adds `scripts/run-scummvm-shadps4.py`, routes
`capture-test.py` through it and documents it in `EMULATOR.md`. Its RC1 stage
check is separate evidence and does not contain the Guard changes.

## Validation and reproducibility

Prepare without touching shared `.tools` sources:

```sh
SCUMMVM_SOURCE_DIR="$PWD/build/eutherlingo-source" bash experiments/scummvm-2026.4.0/prepare.sh vulkan
python3 tests/director7/check-eutherlingo-guard.py build/eutherlingo-source
mkdir -p build/eutherlingo-host
cd build/eutherlingo-host
../eutherlingo-source/configure --disable-all-engines --enable-engine=director --disable-detection-full --disable-fluidsynth --enable-text-console --disable-readline
make -j8
cd ../..
python3 tests/director7/check-eutherlingo-runtime.py build/eutherlingo-source build/eutherlingo-host/scummvm artifacts/eutherlingo-guard/runtime --baseline build/eutherlingo-baseline-host/scummvm
```

The baseline executable is built from the same prepared port before Guard,
with the same host configuration. Preserve/build that revision separately for
comparison; it is not generated by the runtime test runner. The text-console
option allows automated fatal-path tests to dismiss the debugger via stdin.
Tests enforce a 60-second subprocess timeout and retain timeout output.

Actual results:

* Host Director executable built and linked successfully.
* Ring wrap/order/name ownership and operand/argument/PC boundary tests passed
  with AddressSanitizer and UndefinedBehaviorSanitizer. Negative/oversized
  argument counts, empty/out-of-range stacks and absent/empty/out-of-range
  instruction buffers were covered; rejected operations preserved test state.
* Real VM tests passed: normal recursive calls at the configured depth boundary,
  missing/extra arguments, undefined handlers, bad builtin types, invalid object
  types, missing field resources, existing expected-error handling, and uncaught
  error unwinding followed by a new root handler with zero stack/depth.
* Disposed-object property access reported history and stopped; configured
  recursion overflow reported depth 8 and stopped before the next frame.
* All 34 upstream Lingo files ran in the pre-Guard baseline and with Guard
  diagnostics off/on. All three runs have the same six assertion failures:
  four in `chunks.lingo` (lines 37–40), two in `reference.lingo` (20 and 22).
  They are not a green upstream suite and are not claimed fixed here.
* Existing input-service and channel-order regression checks passed.
* The four modified C++ translation units compiled against the actual OpenOrbis
  PS4 configuration/toolchain, including the new headers. Only the pre-existing
  anonymous-union extension warning was observed. This is object compilation,
  not a newly linked PS4 executable, package validation or console testing.
* Clean patch replay matched all 157 patched files byte for byte. A second
  preparation of the replay tree also passed (idempotence).

Local build/test evidence is under `artifacts/eutherlingo-guard/` and
`artifacts/emulator/eutherlingo-*.log`. The runtime `summary.json` retains the
baseline failure list. No commercial data or logs are added to Git.

## Limits and next step

The checks do not validate every bytecode operand, indirect instruction target,
resource parser, ancestor cycle or native pointer. There is no operand-stack
allocation ceiling, blanket timeout, automatic game-state recovery or new
version-specific game correction. Other real Director games and prolonged
Mjölner gameplay have not been exercised with this candidate.

Next, link/package a separately named PS4 diagnostic candidate, retain RC1 as
rollback, and reproduce a specific Mjölner action (scene change, object use,
character switching or save/load) with history enabled. Capture script history
and, for native faults, the native stack. Compare the identified script and
resource against the exact detected release before adding a narrowly matched
entry to ScummVM's existing compatibility mechanism. Investigate the six baseline
Lingo assertions separately; they may help distinguish shared engine limitations
from Mjölner-specific behavior.


## Shad follow-up: window identity (2026-11-04)

A bounded gameplay run in the project's shadPS4 fork reached the Stockholm attic,
selected and moved all three characters, varied movement-click timing, and used
the inventory. Activating the lower-right inventory/menu area produced an actual
script failure: `getObjectProp` could not read `name` on `window "block.dir"`.
The stack was `9:miawPos` at PC 27, called from `36:mouseUp` at PC 45.
The same failure was then reproduced in the untouched RC1 executable, using a
separate emulator profile. Guard therefore exposes an existing missing engine
property; this evidence does not attribute the failure to the original game.

The fix adds window `name` to the existing entity-field table and Window's
has/get/set methods, using the existing object name and lookup implementation.
It does not alias `name` to `title` or `fileName`. Macromedia's
[Director MX Lingo Dictionary, page 415](https://www.manualslib.com/manual/389805/Macromedia-Director-Mx-Lingo-Dictionary.html?page=415)
documents readable/writable window identity, separate from the title bar.
The real D7 script confirms its use there. Registration is conservatively gated
at Director 7; this is not a claim that D7 introduced the property. Earlier
versions need independent verification before widening availability.
No game-specific exception, swallowed error, or game-file edit is needed.

`window-name.lingo` uses classic property syntax because the test target compiles
at D4 before changing its runtime version. Its pre-fix run failed on the same
missing property. The corrected VM passes with Guard off and on: initial name,
rename, case-insensitive lookup, unchanged title/fileName, missing-property error,
and continued valid access. A fresh D4 target still rejects the property.
Runtime version downgrades are not a valid availability test: existing entity
cleanup does not clear the field map. That unrelated behavior was not changed.
The 34 upstream Lingo files retain exactly the six baseline assertion failures.

Local evidence (ignored, not published with game data):

* Guard 0.33: `artifacts/eutherlingo-guard/shad/guard/20261104-101054.log`.
* RC1 reproduction: `artifacts/eutherlingo-guard/shad/rc1/20261104-102753.log`.
* Host regression results: `artifacts/eutherlingo-guard/window-name-final2/`.
* Replay: `artifacts/eutherlingo-guard/window-name-replay.json` (157 files).
* PS4 candidate: `dist/eutherlingo-guard-034/ScummVM-2026.4.0-Vulkan-Test.pkg`.
  SHA256 `ad2660511fd9a01e61c1767d44f6ee9c36a36346e7b33eb0121614b0802c011a`.
* Extracted executable SHA256
  `2a451c447a8e7f81ad4d03b96605195a5e9e7f06cdb28116d372c865ec91749b`.
* Package hashes/signatures passed validation; all 241 GP4 payload files matched
  extraction. RC1 and the failing 0.33 candidate remain preserved.

Input journals and screenshots accompany each local emulator run. Mouse motion
uses relative XTest input and its resulting position is checked visually;
activation uses the emulator's Cross mapping (`n`). This is emulator evidence,
not physical PS4 acceptance, full-game completion, or a reproducibility claim
for every intermittent click-dependent problem.

The fixed 0.34 emulator run is
`artifacts/eutherlingo-guard/shad/fixed/20261104-103528.log`.
It visibly opened the notebook, turned to its empty page, closed it, and resumed
walking. The game menu also opened and its Back button returned to the attic.
The initial isolated scenario is: New Game, skip introduction, open the bag,
activate the notebook, turn its lower-right page corner, click outside to close,
then reopen the bag and activate Menu. At 1280x720, the bag/menu is near
(1060,660), notebook near (998,660), and page corner near (810,540).
Wait for each visible state before the next input; these are screenshot
coordinates, not raw XTest coordinates or a timing-independent replay.

The fixed session was stopped after approximately ten minutes with zero
`window.name` errors, zero uncaught Lingo errors and zero Guard reports.
This does not mean a warning-free run: unsupported D7 film-loop warnings and
`setTheSprite` with a VOID sprite ID remain. Clicking Save did not yield a
verified save dialog/file; the sole file in the configured saves directory is
an existing screen-preference record, not gameplay state. No successful load or
scene transition beyond the attic is claimed. The next bounded investigation
should trace input/handler delivery for item actions and the save menu, then
verify the FileIO path if the save handler is reached. Do not infer that FileIO
is the cause from its startup warning alone. Owned emulator sessions were stopped;
logs, screenshots, input journals and profiles remain local for resumption.
