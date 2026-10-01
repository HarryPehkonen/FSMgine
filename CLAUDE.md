# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

FSMgine is a C++17 finite state machine library (public domain, Unlicense) with a
fluent builder API for dynamic FSM construction. It is header-heavy: the public API
lives in `include/FSMgine/` (`FSM.hpp`, `FSMBuilder.hpp`, `Transition.hpp`,
`StringInterner.hpp`, and the umbrella `FSMgine.hpp`), with a single translation unit,
`src/StringInterner.cpp`. Everything lives in namespace `fsmgine`; the umbrella header
also defines `namespace fsm = fsmgine;` as a convenience alias.

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

`tools/ci.sh` is the single definition of every check in this repo — CI and local
hooks both call it, so there is one place to read or change the rules.

```bash
./tools/ci.sh            # run the default stage set
./tools/ci.sh <stage>     # run one stage
./tools/ci.sh --list      # show what the stages are
```

Stages: `tree format docexamples dbs build lint tests coverage release asan fuzz
pristine`.

Two git hooks are checked in but not armed automatically — run `git config
core.hooksPath .githooks` once per clone. **pre-commit** then runs `--changed dbs
format build lint tests` (seconds, scoped to touched files); **pre-push** runs the
full stage list plus `--require-clean` (~2-3 minutes).

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
singleton whose storage arena is append-only by design — `clear()` forgets the
lookup index but keeps the storage so views handed out earlier stay valid. A
long-running target that keeps interning fresh names therefore grows resident
memory without limit: measured at ~250 MiB/min, it ended a run after 12 minutes on
libFuzzer's own 2 GB guard. So every input must finish with
`StringInterner::reset()`, which releases the arena and **invalidates every view** —
and everything holding a view (the machine, the view and name pools) must be
destroyed **first**. See `fuzz/retention_check.cpp`.

Do **not** "fix" growth by shrinking the name vocabulary: it costs coverage (it
took the FSMgine corpus from 1124 entries to ~400). `reset()` is the fix.

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
- `fsmgine::StringInterner::instance()`, `.intern(...)`, `.clear()` (keeps the arena —
  earlier views stay valid), `.reset()` (releases the arena — **every earlier view
  becomes dangling**; destroy everything holding a view first), `.arena_size()`.

**`to()` is terminal and returns `void`.** There is no `when()` and no `build()` —
an earlier design sketched them, but they never landed in the implementation. Each
transition is its own chain, starting from a fresh `get_builder()`:

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

## Known Gaps

- `tests/simple_test_runner.cpp` is dead code: GTest is available in this
  environment, so `CMakeLists.txt` always takes the GTest branch and no target ever
  compiles this file.
- `benchmarks/bench_FSM.cpp` and `bench_StringInterner.cpp` need Google Benchmark,
  which is not installed here; only `simple_timer_benchmark.cpp` builds without it.
- `REQUIREMENTS.md` is a historical design document carrying its own banner saying
  so — its API sketches (`step()`, `when()`, `build()`, zero-argument predicates)
  predate the current implementation and do not compile against it. It is excluded
  by name from the `docexamples` gate.