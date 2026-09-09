# FSMgine — C++ Coding Standards (MANDATORY for all agents and humans)

This file is the binding standard for all C++ work in this repository. It
implements the *C++ Core Guidelines* (Type / Bounds / Lifetime profiles) in a
practical, enforceable form. It applies to EVERY commit, from ANY author —
human or AI agent. Exceptions ARE allowed for error handling (this is not
MISRA/JSF style; we do not ban exceptions).

## Tooling in this repo (already wired)

- Libraries `FSMgine` and `FSMgineMT` compile with `-Wall -Wextra -Wpedantic -Werror`.
- Test/example targets compile with the same strict flags (see CMakeLists).
- Tests: gtest (`FSMgine_tests`, ctest) when `GTest` is found, else the
  fallback runner `FSMgine_simple_tests`.
- Single-threaded (`FSMgine`) and multi-threaded (`FSMgineMT`, `FSMGINE_MULTI_THREADED`) builds exist.

## Hard rules

1. **C++17 minimum**, modern style. No C-style anything.
2. **No raw owning pointers** — `std::unique_ptr` / `std::shared_ptr` / RAII.
   A raw pointer/reference is a *non-owning view* only, never deleted.
3. **No `new`/`delete`** (no `malloc`/`free`) in new code.
4. **No `reinterpret_cast` or C casts.** `const_cast` only with a justifying
   comment. Prefer `static_cast`; `dynamic_cast` where polymorphic.
5. **No undefined behavior**: no signed-overflow reliance, no OOB (use
   `.at()`/checked paths where indexing), no uninitialized reads — initialize
   everything.
6. **`[[nodiscard]]`** on accessors, predicates and pure functions; callers
   that deliberately discard must cast to `(void)` with a comment.
7. **Exceptions for error handling** (never for control flow). RAII guarantees
   cleanup. Empty `catch` blocks are banned unless intentional — then comment
   AND `// NOLINT(bugprone-empty-catch)`.
8. **Threading** (this is an FSM library — respect the model): compile-time
   threading via `FSMGINE_MULTI_THREADED`; no data races; use the provided
   locking discipline (`std::shared_mutex`: reads `step()`/`getCurrentState()`,
   writes exclusive). No new global mutable state.
9. **No dangling**: never keep references/iterators across container mutation
   or object destruction (Lifetime profile).
10. **Move-only semantics preserved**: FSM objects cannot be copied — do not
    introduce copyability.
11. **Zero warnings**: every target compiles with
    `-Wall -Wextra -Wpedantic -Werror`. Never weaken flags to silence code;
    fix the code. Existing NOLINT comments must carry a reason.
12. **TDD**: write the failing test first (RED), watch it fail, implement,
    watch it pass (GREEN), then run the FULL suite (`ctest`) before commit.
13. **No `std::endl`** — use `'\n'`. Never pass `std::move` to a `const&`
    parameter. Match the repo's existing naming conventions.

## Definition of Done (every change)

- [ ] Builds clean: `cmake -B build && cmake --build build` — zero warnings
- [ ] Full suite passes: `ctest --test-dir build` (both MT and ST configs
      where relevant)
- [ ] New/changed behavior covered by a test that was RED first
- [ ] No raw owning pointers, no new UB patterns introduced
- [ ] Sanitizer pass where feasible:
      `cmake -B build-asan -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"`
      then run the tests against that build

## Agents: how to comply

1. Read this file at the start of every session (it is referenced from
   CLAUDE.md).
2. Follow the hard rules above; when in doubt, prefer the safer construct.
3. Before finishing: build, test, and report the actual command output.
4. Never claim a gate passed without running it.

Reference: https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines
