# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

FSMgine is a C++17 finite state machine library (public domain, Unlicense) with a
fluent builder API for dynamic FSM construction. It is header-heavy: the public API
lives in `include/FSMgine/` (`FSM.hpp`, `FSMBuilder.hpp`, `Transition.hpp`,
`StringInterner.hpp`, and the umbrella `FSMgine.hpp`), with a single translation unit,
`src/StringInterner.cpp`. Everything lives in namespace `fsmgine`; the umbrella header
also defines `namespace fsm = fsmgine;` as a convenience alias. `FSMgine/version.hpp` is
generated at configure time from `include/FSMgine/version.hpp.in` rather than checked in;
see Releasing below.

A second, additive back end lives in `include/FSMgine/compiled/Machine.hpp`, namespace
`fsmgine::compiled` — enum states, a transition table of plain data, no interned
strings or `std::function` on the hot path. The umbrella `FSMgine.hpp` deliberately does
not include it, so the interpreted back end's users pay nothing for it. See "Current
API" below and README's "Compiled Back End" section.

CMake builds two library variants from the same sources, selected by the
`FSMGINE_MULTI_THREADED` compile definition: **FSMgine** (single-threaded, no
synchronization overhead) and **FSMgineMT** (mutex-protected FSM operations).

There is no code-generation tool or DSL-comment processor in this repo — `src/`
contains only `StringInterner.cpp`. A `./fsmgine` binary or "code generation mode"
was an earlier design direction that was never built; don't assume it exists.

## Build and Test

```bash
cmake -B build && cmake --build build -j
ctest --test-dir build
```

`BUILD_EXAMPLES` and `BUILD_BENCHMARKS` are both `OFF` by default; pass
`-DBUILD_EXAMPLES=ON` / `-DBUILD_BENCHMARKS=ON` to opt in. `BUILD_TESTING` is `ON` by
default and uses GTest when it is installed.

## The Local Gate

`scripts/gate.sh` is the single definition of every check in this repo — CI and local
hooks both call it, so there is one place to read or change the rules.

```bash
./scripts/gate.sh            # run the default stage set
./scripts/gate.sh <stage>     # run one stage
./kit-ci --list      # show what the stages are
```

Stages: `tree selftest format version docexamples dbs build lint tests coverage release
asan fuzz pristine`. The `selftest` stage runs `tools/ci_selftest.sh`, which verifies the
gate's own changed-file scope on a throwaway clone — a regression guard for a bug where a
commit that deletes a source file used to fail the format stage with "No such file or
directory" because the scope was listing the deleted path; the script asserts both that
the fix works and that the pre-fix line still fails, so the guard cannot rot. The `version`
stage fails if the version declared in `CMakeLists.txt`
is ever behind the newest `v*` tag, if HEAD is tagged with something other than the
declared version, or if `tools/release.sh status` does not run; it skips with a note
in a clone that has no tags.

Two git hooks are checked in but not armed automatically — run `git config
core.hooksPath .githooks` once per clone. **pre-commit** then runs `--changed dbs
format build lint tests` (seconds, scoped to touched files); **pre-push** runs the
full, now-fourteen-stage list plus `--require-clean` (~2-3 minutes).

Lint and coverage are baseline-gated, not zero-tolerance, so pre-existing findings
don't block unrelated work:
- Accepted clang-tidy findings live in `.ci/tidy-baseline.txt`, keyed by
  `path:check` (no line numbers — they drift on every reformat). A key that no
  longer fires is reported by the lint stage; `--write-tidy-baseline lint` prunes
  it. A deliberate deviation tied to one line is instead suppressed at the site
  with `// NOLINTNEXTLINE(check)` and the reason on the line(s) above it.
- Coverage is gated per file on missed-line counts in `.ci/coverage-baseline.txt`;
  `--write-coverage-baseline coverage` re-records it.

Every C++ example in `README.md`, `CLAUDE.md` and the public headers is compiled by the
`docexamples` stage (`tools/check_doc_examples.py`) — if you add or edit one, the
gate will try to compile it, so keep it self-contained and correct.

## Releasing

The version has exactly one home: the `project(FSMgine VERSION x.y.z)` line in
`CMakeLists.txt`. Don't write a version anywhere else — `FSMgine/version.hpp` is
generated from it at configure time (see Project Overview). `tools/release.sh` is the
release process, used in this order:

```bash
tools/release.sh status                       # what version this repo declares, what is
                                               # unreleased
tools/release.sh prepare [--apply] [--set X.Y.Z]
                                               # propose the next version from commits since
                                               # the last tag; --apply writes it to
                                               # CMakeLists.txt
tools/release.sh notes [--open]               # draft the release notes, compatibility
                                               # table first
tools/release.sh publish --yes                # tag and publish on GitHub
```

A release's notes lead with a compatibility table; filling it in is a human job, and
`publish` deliberately blocks while it still contains TODO. `prepare`'s default
convention: while this library has no consumers, a breaking (`!`) commit is a MINOR bump.
`--set X.Y.Z` overrides that recommendation to express a deliberate MAJOR — the honest
version when a public method is removed or renamed, as with the StringInterner changes
below. The declared version is currently 2.0.0.

## Coding Standards

Follow `CODING_STANDARDS.md`: modern C++17 in the spirit of the C++ Core Guidelines
(exceptions are allowed), builds clean under `-Wall -Wextra -Wpedantic -Werror`, no
raw owning pointers, no `new`/`delete`, no C-style casts. Respect the move-only
semantics of `FSM` (copy is deleted, both moves are defined) and the threading model:
`FSMGINE_MULTI_THREADED` gates all locking, so a single-threaded build compiles it
out entirely.

## Fuzzing

Two libFuzzer targets live behind `-DFSMGINE_BUILD_FUZZING=ON` (clang only):
`fuzz_fsmgine`, a stateful driver for the engine + interner, and `retention_check`,
a regression test that drives the target with many inputs and asserts resident
memory stays bounded.

```bash
cmake -B build-fuzz -DFSMGINE_BUILD_FUZZING=ON -DFSMGINE_BUILD_MULTITHREADED=OFF \
      -DBUILD_TESTING=OFF -DCMAKE_CXX_COMPILER=clang++ ..
cmake --build build-fuzz --target fuzz_fsmgine retention_check
./build-fuzz/fuzz_fsmgine <corpus-dir>     # binaries land in the build root
./build-fuzz/retention_check
```

**Rule: reset the interner between inputs.** `StringInterner` is a process-global
singleton whose storage arena is append-only by design — there is no per-name
release, so a long-running target that keeps interning fresh names grows resident
memory without limit: measured at ~250 MiB/min, it ended a run after 12 minutes on
libFuzzer's own 2 GB guard. So every input must finish with
`StringInterner::resetArena()`, which releases the arena and **invalidates every
view** — and everything holding a view (the machine, the view and name pools) must
be destroyed **first**. See `fuzz/retention_check.cpp`.

Do **not** "fix" growth by shrinking the name vocabulary: it costs coverage (it
took the FSMgine corpus from 1124 entries to ~400). `resetArena()` is the fix.

## Current API

- `fsmgine::FSM<TEvent>` (default `TEvent = std::monostate`; alias
  `fsmgine::EventlessFSM = FSM<>`) — move-only, copy deleted.
  `FSMBuilder<TEvent> get_builder()`; `setInitialState`/`setCurrentState(std::string_view)`;
  `std::string_view getCurrentState() const`; `bool process(const TEvent&)` and the
  event-less overload `bool process()` (static_assert-guarded to `FSM<>`).
- `fsmgine::FSMBuilder<TEvent>`: `TransitionBuilder<TEvent> from(const std::string&)`,
  `FSMBuilder& onEnter(const std::string&, Action)`, `FSMBuilder& onExit(const
  std::string&, Action)`.
- `fsmgine::TransitionBuilder<TEvent>`: `TransitionBuilder& predicate(Predicate)`,
  `TransitionBuilder& action(Action)`, `void to(const std::string&)`.
- `fsmgine::StringInterner::instance()`, `.intern(const std::string&)`,
  `.intern(std::string_view)`, `.resetArena()` (the only release mechanism —
  releases the arena and **every earlier view becomes dangling**; destroy
  everything holding a view first, machines first since they hold interned
  state-name views), `.arenaSize()`. That is the entire public surface — no
  `clear()`: it was test-only, it never actually released memory (it only
  forgot the lookup index, leaving the arena allocated), and it was removed
  outright in 2.0.0. `reset()`/`arena_size()` were renamed to
  `resetArena()`/`arenaSize()` in the same release.

**A `TransitionBuilder` builds exactly one transition, and `to()` spends it.** `to()`
is the commit point: it moves the accumulated predicates and actions into the machine,
is terminal, and returns `void` on purpose — a chain continued after it must not
compile, and a `static_assert` in `tests/test_FSM.cpp` fails the build if that
signature ever changes. Calling `to()` a second time, or `predicate()`/`action()` after
`to()`, throws `FSMBuildError` (a `std::logic_error` declared in `FSM.hpp`) rather than
being silently ignored — on a spent builder the accumulated state is already moved out,
and a transition with no predicate is deliberately unconditional
(`Transition::predicatesPass()` treats an empty list as always-true), so letting a
second `to()` through used to register a silent catch-all. There is no `when()` and no
`build()` — an earlier design sketched them, but they never landed in the
implementation. Each transition is its own chain, starting from a fresh
`get_builder()`:

```cpp
#include <FSMgine/FSMgine.hpp>
#include <string>

struct Event {
    std::string type;
};

int main() {
    fsmgine::FSM<Event> machine;
    machine.get_builder()
        .from("Idle")
        .predicate([](const Event& e) { return e.type == "start"; })
        .to("Working");
    machine.get_builder()
        .from("Working")
        .predicate([](const Event& e) { return e.type == "stop"; })
        .to("Idle");

    machine.setInitialState("Idle");
    machine.process(Event{"start"});  // transitions to "Working"
    return 0;
}
```

### Compiled Back End

- `fsmgine::compiled::Machine<State, Event>` (`include/FSMgine/compiled/Machine.hpp`,
  not included by the umbrella header) — `State` is a user `enum`; `Event` must have a
  public member named `kind` (its type, `decltype(Event::kind)`, is the machine's
  `EventKind` — a naming convention, not a template parameter, since C++17 cannot
  deduce a third parameter from the two-argument `Machine<State, Event>`).
  `Machine& from(State)`, `Machine& when(EventKind)`, `Machine& when(EventKind,
  Refinement<Event>)`, `Machine& action(Action<Event>)`, `Machine& to(State)` (commits
  the row being built, like `TransitionBuilder::to()`, but keeps returning `*this` so
  `from()` can start the next row on the same object), `Machine& withNames(NameFn)`,
  `setInitialState`/`setCurrentState(State)`, `State getCurrentState() const`,
  `std::string_view currentStateName() const`, `bool process(const Event&)`. Semantics
  match `fsmgine::FSM` exactly: first matching transition wins, an action (a plain
  `void(*)(const Event&)` function pointer — it cannot capture) runs before the state
  change, and a non-match leaves the state unchanged.
- `fsmgine::compiled::eq/lt/le/gt/ge(int Event::*, int)` build a `Refinement<Event>` —
  v1 supports exactly **one** refined field per machine (a second `when(kind,
  refinement)` naming a different member throws `CompiledMachineError`) and exactly
  **one** action per transition (a second `.action()` call before `.to()` also throws).
- `fsmgine::compiled::CompiledMachineError` — a `std::logic_error`, thrown for the two
  limits above plus calling `process()`/`getCurrentState()` before
  `setInitialState()`/`setCurrentState()`, or firing a refined row with no refined
  field on record.

**Interpreted vs. compiled, in one line:** reach for `fsmgine::FSM` when the machine is
defined at run time (config, plugins, user input) and guards/actions need to close over
context; reach for `fsmgine::compiled::Machine` when the machine is fixed at compile
time and you want the table-scan shape without strings or `std::function` on the hot
path. `benchmarks/bench_comparison.cpp` / README's bench table measure the difference.

## Known Gaps

- `benchmarks/bench_FSM.cpp` and `bench_StringInterner.cpp` need Google Benchmark,
  which is not installed here; `simple_timer_benchmark.cpp` and
  `benchmarks/bench_comparison.cpp` (targets `FSMgine_comparison` and `comparison`,
  pinned to the single-threaded `FSMgine` library) build without it.
  `bench_comparison.cpp` times FSMgine against a hand-rolled `switch`, a
  hand-rolled runtime table, and `fsmgine::compiled::Machine` on the same fixed event
  script; see README's "When not to use FSMgine" section for what it measures and its
  stated limits.
  `tools/update_bench_table.sh` rebuilds `FSMgine_comparison` in Release and
  rewrites only the text between the `<!-- BENCH-TABLE:BEGIN -->` /
  `<!-- BENCH-TABLE:END -->` markers in `README.md` — that table is generated, not
  hand-edited.
- `REQUIREMENTS.md` is a historical design document carrying its own banner saying
  so — its API sketches (`step()`, `when()`, `build()`, zero-argument predicates)
  predate the current implementation and do not compile against it. It is excluded
  by name from the `docexamples` gate.