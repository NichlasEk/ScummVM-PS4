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


### Additional gameplay probe, afternoon 2026-11-04

No new executable or game-file edits were used. In the 0.34 Shad candidate,
playing the full introduction and waiting for Harry's walk to the radio did not
make item pickup visibly complete. This does not establish that the radio itself
is a collectible or that its script is defective. No later scene was reached.

Existing `events` logging at level 3 isolated the Save interaction: a click in
`setUp.DIR` at local (127,87) resolved to sprite 0. Moving nine screenshot pixels
down produced local (127,93), sprite 5, and opened the Save page. This is evidence
of a narrow/offset hit target; its rendering or collision root cause is not yet
established. The previous inability to open Save was not proof of missing FileIO.

Saving with the default name wrote a 17,642-byte game-state file to the private
profile's SaveFileManager directory, SHA256
`32588099932e88551f5364dbeb621a8ef8d95c90dbdb9840e40f8287727333ba`.
After clearing `setUp.DIR`, the emulator exited with signal 5 and reported a guest
null read at PC `0x122b358`. With the executable mapped at `0x400000`, the exact
candidate ELF resolves this to `Window::getMousePos()+0x28`, dereferencing its
MacWindow pointer. A likely lifetime bug is that `Window::~Window()` destroys
that widget without clearing the engine's non-owning `_cursorWindow`; the port's
`DirectorEngine::draw()` uses that pointer for cursor updates. This is a targeted
hypothesis, not a verified fix, and must not be treated with generic null recovery.

A separate fresh launch retained the identical save file but Open Game displayed
no saved games. The existing `getNthFileNameInFolder` compatibility path matches
both Jonsson titles yet filters `save_*.sav.txt` (the second title's naming).
The first title's default file is `jonssonligan1-Jönsson1.txt`, so that filter
excludes it. Correct title-specific save enumeration and safe cursor-reference
invalidation are the next two bounded fixes; save/load is still NOT accepted.

Local evidence is under `artifacts/eutherlingo-guard/shad/followup/`, including
`result.json`, a preserved save copy, and read-only compilation of the setup
movie. Session logs: `fixed/20261104-142727.log` (full intro/item attempts),
`fixed/20261104-143828.log` (event trace/save/crash), and
`fixed/20261104-144530.log` (fresh-launch Open Game). Owned sessions were stopped
and the profile's normal logging configuration restored. No additional source
fix or physical-console validation is claimed by this probe.


### Save/window repair candidate 0.36 (2026-11-05)

The window destructor now invalidates the non-owning cursor reference before
tearing down its movie, script state or widget. It preserves a cursor pointing
to another window. This addresses the observed post-save null-widget access at
its ownership boundary, without swallowing faults or attempting VM rollback.

Mjolner save enumeration now accepts its freeform `.txt` filenames; the second
Jonsson title retains its numbered-slot pattern. A live 0.35 test showed the
previously invisible save, then reported that it could not find it. Inspection
of the original setup handler identified a `baFileExists` check preceding load.
Candidate 0.36 maps that check to FileIO's target-prefixed SaveFileManager name,
only for Mjolner's `Sparade spel` directory and the opt-in PS4 experiment. Other
asset paths, titles and non-PS4 behavior retain their existing implementation.
Original game data is unchanged.

`tests/director7/check-window-save-lifecycle.py` extracts the production destructor,
save-list branch, name mapping and file-existence method into ASan/UBSan fixtures.
It tests teardown ordering, unrelated windows, Unicode save names, target
isolation, directory separators, missing saves, list bounds, opt-out behavior,
second-title compatibility and stack balance. The old destructor and old listing
failed their respective regression assertions; the repaired implementation passes.
Host and PS4 builds pass. The real-VM regression suite passes its focused cases
and runs 34 upstream scripts with Guard disabled/enabled; the six pre-existing
baseline assertion failures remain unchanged. Fresh patch replay matches all
158 modified files. PKG 0.36 validates and all 241 extracted payload files match
staging. Evidence: `artifacts/eutherlingo-guard/window-save-fix/`.


Live shadPS4 0.36 verification (`repaired/20261105-205442.log`) lists the preserved
`Jönsson1` save, passes the existence check and executes FileIO open/read. Restore
then stops at `getCord` PC 8: `getAt` receives INT rather than a list and does not
return a value. Guard captures the 128-entry trace and the VM enters its fatal
error path. No fabricated return value or recovery was added. Save/load remains
unaccepted; investigate the restored coordinate data and serialization next.
The destructor regression is sanitizer-verified, but a complete post-save gameplay
cycle has not yet been revalidated with this candidate.

The owned headless run ended. Our fork `/home/nichlas/shadPS4/build-validation/shadps4`
was launched on the actual desktop DISPLAY `:1`, windowed, with candidate 0.36
and a separate copied local profile under `shad/visible036/`. Its mapped window
reports `shadPS4 NichlasEk/v0.18.0 ... <0.36>`. This interactive session is left
running for New Game testing; it is not a physical-PS4 validation or proof that
later scenes work. Package SHA256:
`48fc43a9090ca3d15266124689e7a6b03ff950ec605a5acbbff7b7e81a0d4386`.

### Refrigerator crash: candidate 0.37 (2026-11-05)

The user's visible 0.36 session crashed after opening the attic refrigerator.
A second 0.36 run reproduced it with ordinary left clicks and `lingoexec`
tracing: `56:mouseUp` executes a dynamic `changeMember` call, then
`9:changeMember` searches the unsorted object-name list for `kylskap1`.
The existing `findPos` implementation coerces the string to an integer and
returns zero. `setAt(list, 0, ...)` then indexes `index - 1` without rejecting
zero, hitting the array assertion. The same handler uses `cb_delete 5` with
operand 24 to remove the final character from local slot 3. Our earlier D7
slot-index compatibility rule incorrectly treated this as slot 24 and skipped
the deletion, producing `kylskap12` rather than `kylskap2`.

The candidate makes three bounded corrections:

- In the opt-in PS4 Mjolner compatibility path, `findPos` searches string values
  in an unsorted linear list. Numeric, other-title and disabled behavior stay
  unchanged. This is game-specific compatibility, not a claim about all Director
  versions' `findPos` semantics.
- `cb_delete` requests byte-offset interpretation from `findVarV4`; other callers
  retain their established behavior. Actual game bytecode supplies 24 for local
  slot 3 and 8 for slot 1, while put-after supplies the plain slot index 1.
- General `setAt` rejects nonpositive indices via the existing Lingo error path,
  before arithmetic or mutation. Valid replacement and extension still work.

The sanitizer fixture `check-fridge-bytecode.py` tests the extracted lookup and
index-decoding code. Real-VM tests catch indices 0, -1 and INT_MIN, verify the
list stays unchanged after errors, then replace and extend it successfully.
Host/PS4 builds pass; 34 upstream scripts retain their six baseline assertion
failures with Guard off/on. Fresh replay matches 158 modified files; the 0.37
package validates and its 241 extracted payloads match staging.

Local evidence: `artifacts/eutherlingo-guard/fridge/`, including the original
user crash, the reproduced failing handler and read-only compilation of the
Stockholm movie. No original game data has been modified.

Actual game verification in our shadPS4 fork (`repaired/20261105-210717.log`):
Sickan walked to the refrigerator; left clicks opened it, closed it and opened
it again. Screenshots `open037.png`, `closed037.png` and `reopened037.png` record
those states. The process remained alive; the invalid-number/variable-index
warnings, array assertion and Guard fatal report did not recur in this test.
Existing unrelated VOID sprite warnings remain. This is a bounded emulator
regression result, not full-game or physical-console acceptance. The old save
restore failure has not been retested by this change.

Candidate PKG SHA256:
`33ee1299bc8b51870efb3ee1ac3181c7038e7f705758e0a87b1a6e815886d9e8`.

### Research-centre dynamite report: candidate 0.38 (2026-11-05)

The user progressed beyond the attic and reported a crash when placing dynamite
on the research-centre door. The preserved visible 0.37 log identifies
`Stockholm.DIR`, `49:mouseUp` -> `11:useTool` PC 4646. This is a default-response
branch: it enumerates `@\voice\default\`, filters names for the selected actor,
then randomly selects a response. The filtered list was empty (`getAt` index
6690 of 0); Guard reported the script error and the VM stopped. This evidence
does not establish that the dynamite placement itself was accepted by the puzzle.

The directory builtin used only `findAbsolutePath`. On failure an empty path
was walked as the game root, exposing unrelated root entries. There are six
root entries, consistent with the seven enumeration calls in the recorded
handler. Thirteen actual default voice files exist under `data/voice/Default`.
For opt-in PS4 Mjolner only, a failed non-root absolute folder lookup now uses
`findPath` with the current folder/search paths, matching the asset loader.
If that also fails it returns an empty filename rather than listing the root.
Explicit root paths, existing directories, relative paths and other titles
keep their previous behavior. No sound is fabricated and no script error is
suppressed. A genuinely missing response set remains a game-script limitation.

`check-folder-fallback.py` exercises the extracted production branch with
ASan/UBSan for fallback success/failure, existing/root/relative paths and gates.
This is a resolver fixture, not an end-to-end reproduction of the user's scene.
Host/PS4 builds and window/fridge regression tests pass. Real-VM tests run 34
upstream scripts with Guard off/on and retain six baseline assertion failures.
Fresh source replay matches 158 modified files. The candidate PKG validates;
its SHA256 is
`341dbcd975d5349ac00e1fb064d84c514092109948be90bac96928349bc1e6f8`.
Evidence is under `artifacts/eutherlingo-guard/dynamite/`.
The research-centre action requires a live retest; no physical-PS4 acceptance
or full-game stability is claimed. The earlier save-restore limitation remains.

### Save restore / FileIO mode 0: candidate 0.39 (2026-11-05)

The 0.38 load attempt reproduced the existing `getCord`/`getAt` INT failure.
The preserved 17,642-byte save contains the character coordinate lists at its
end; its SHA256 remains
`32588099932e88551f5364dbeb621a8ef8d95c90dbdb9840e40f8287727333ba`.
The original `readSaveFile` handler opens FileIO with mode 0 (read/write), calls
`readFile`, then closes. ScummVM mapped that mode to append, which only creates
an output buffer. `readFile` therefore returned empty text; `value` on empty
coordinate chunks returned integer zero, causing the later coordinate failure.
The same game uses mode 0 to write saves, so mapping it to read-only would break
saving and is not a valid repair.

For opt-in PS4 Mjolner, mode 0 now creates a read view of the existing writable
memory buffer and starts at offset zero. Reads, writes and seeks share a position;
the view fetches the buffer pointer on each read so growth cannot stale it.
EOF and invalid seeks are bounded. The view is destroyed before its backing
buffer during close or deletion. Modes 1/2, explicit append, other titles and
non-PS4 behavior keep their existing mode selection. Persistence continues to
use FileIO's existing SaveFileManager flush on close. No original game file or
save content is patched and no replacement coordinates are invented.

`check-fileio-readwrite.py SOURCE HOST_BUILD` compiles the actual production
header against ScummVM's host common libraries with ASan/UBSan. It tests empty
files, shared read/write position, overwriting, growth/reallocation, EOF,
relative/end seeks and extreme invalid offsets. Host/PS4 builds pass; real-VM
regressions retain six known baseline assertions across 34 upstream scripts.
Fresh patch replay matches 159 modified files. PKG 0.39 validates and extracted
payloads match staging. Evidence: `artifacts/eutherlingo-guard/load/`.

Live 0.39 verification (`repaired/20261105-213901.log`) loaded the preserved
`Jönsson1` save from the title menu and restored the attic and characters.
`restored-old-save039.png` records the scene. The saved file's SHA256 remained
unchanged after loading. This establishes the cause of the previous empty-input
restore failure; the code does not bypass `getCord` or invent VM state.

The save-overwrite path also uses `baDeleteFile` before mode-0 writing, but that
function was a stub. Candidate 0.40 implements it only for Mjolner's virtual
save directory via SaveFileManager, so replacing a longer file cannot retain
its old tail. Original game asset paths remain ineligible. The existing
save-lifecycle fixture now tests deletion, missing-file idempotence, title and
opt-in gates, target isolation and stack balance. Buddy API's original help
specifies success for deletion or an already absent file:
https://discmaster.textfiles.com/file/15766/pc-play-and-learn.iso/xtras/BUDAPI.HLP/BUDAPI.pdf

Live 0.40 save test (`repaired/20261105-214505.log`): loaded the old save,
walked Sickan to a different position, selected the existing `Jönsson1` slot
and saved over it. The menu closed and gameplay continued without the earlier
window teardown crash. The resulting 17,642-byte file has SHA256
`9a31fc335f39ca7257067b1cd0fa855b26a2755ebdb630b683bbfbaa4a3e7d4c`;
Sickan's serialized grid position changed from `[25, 27]` to `[21, 19]`.
The original user's profile and a backup retain the previous file.

A separate emulator process (`repaired/20261105-215048.log`) then loaded that
new file after restart. Sickan appeared at the saved new location; the file's
SHA256 remained unchanged. `reloaded-new-position040.png` and `result040.json`
record this round trip. Thus the formerly failing old save and an overwritten
save both load in our fork. This replaces the earlier blanket save-restore
limitation for this tested file/workflow; other saves, the research-centre
sequence and physical PS4 remain unverified. Candidate 0.40 package SHA256:
`ad5ecf2288a30ad5a34f36db8a2c9d7b566d26c25fc1e0791077c50f32ddbd80`.
