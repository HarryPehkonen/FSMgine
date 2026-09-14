# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

FSMgine is a C++ finite state machine library with two distinct approaches:
1. **Code Generation Tool**: Processes DSL comments in C++ files to generate transition code
2. **Runtime Library**: Provides fluent builder API for dynamic FSM construction

The project is in transition - old code generation components are being replaced with a new runtime library design.

## Build System

FSMgine uses CMake with C++17 standard:

```bash
# Build the project
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make

# Install
make install
```

## Architecture

### Core Components (New Design)
- **StringInterner**: Singleton for string optimization and memory efficiency
- **FSM**: Main state machine container with thread-safe operations
- **FSMBuilder/TransitionBuilder**: Fluent interface for FSM construction
- **Transition**: Represents state transitions with predicates and actions

### Threading Model
- Compile-time threading support via `FSMGINE_MULTI_THREADED` preprocessor flag
- Uses `std::shared_mutex` for concurrent read operations (`step()`, `getCurrentState()`)
- Write operations (`to()`, `onEnter()`, `onExit()`) acquire exclusive locks
- Single-threaded builds compile out all locking overhead

### Builder Pattern Usage
```cpp
FSM fsm;
fsm.get_builder()
    .from("START")
    .predicate([]() { return condition; })
    .action([]() { /* do something */ })
    .to("END");
```

## Key Design Decisions

- **Move-only semantics**: FSM objects cannot be copied, only moved
- **String interning**: All state names are interned for memory efficiency
- **Flexible definition order**: States can be referenced before being fully defined
- **Live editing support**: FSM structure can be modified during runtime in multi-threaded scenarios
- **No event loop management**: Library integrates into existing application architectures

## Development Commands

Since the project structure is in transition, check for the existence of build files before running commands:

```bash
# Check if CMakeLists.txt exists before building
ls CMakeLists.txt

# For code generation mode (if old structure exists)
./fsmgine < input.cpp > output.cpp
```

## C++ Standards (MANDATORY)

All C++ work in this repo MUST follow `CODING_STANDARDS.md` — modern C++17 in
the spirit of the C++ Core Guidelines (Type/Bounds/Lifetime); exceptions
allowed. This is an FSM library: respect the threading model
(`FSMGINE_MULTI_THREADED`, shared-mutex read/write discipline) and the
move-only semantics of FSM objects. Gates before any commit:

1. `cmake -B build && cmake --build build` — zero warnings (own targets
   compile with -Wall -Wextra -Wpedantic -Werror).
2. `ctest --test-dir build` — all tests pass (TDD: failing test first).
3. No raw owning pointers, no `new`/`delete`, no C casts.
4. Sanitizer pass where feasible (ASan+UBSan recipe in CODING_STANDARDS.md).

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