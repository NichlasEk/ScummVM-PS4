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
