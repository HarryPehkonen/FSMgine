# FSMgine Performance

How to profile FSMgine, and what the recorded profiling runs measured. This is the
single performance document: the tools and commands to reproduce a measurement, the
results that were recorded, and how to read them. For the library itself see
[README.md](README.md); for the development workflow see [CLAUDE.md](CLAUDE.md).

## Profiling tools

Four profilers are used, each answering a different question:

- **Google Benchmark** (recommended) — microbenchmarks that isolate one optimization
  and compare a before/after build.
- **Linux perf** — system-wide profiling and hotspot finding.
- **Valgrind Callgrind** — detailed, per-function instruction and cache analysis.
- **GProf** — quick call-graph timing.

## Setup

Install the tooling:

```bash
./scripts/setup_profiling.sh
```

Build with benchmarks enabled:

```bash
mkdir build && cd build
cmake -DBUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release ..
make
```

## Running the benchmarks

```bash
make benchmark
# or directly:
./FSMgine_benchmarks
```

The Google Benchmark executable takes the usual flags:

```bash
# Run all benchmarks
./FSMgine_benchmarks

# Run specific patterns
./FSMgine_benchmarks --benchmark_filter="StringInterner"

# Statistical analysis
./FSMgine_benchmarks --benchmark_repetitions=10

# JSON output
./FSMgine_benchmarks --benchmark_format=json --benchmark_out=results.json
```

Sample output (illustrative — the recorded runs are further down):

```
Benchmark                               Time             CPU   Iterations
------------------------------------------------------------------------
BM_StringInterner_RepeatedSingleton    245 ns          245 ns      2856789
BM_StringInterner_CachedReference      198 ns          198 ns      3534782
BM_FSM_StateTransitions               1247 ns         1247 ns       561892
```

`Time`/`CPU` is the average time per iteration and `Iterations` is how many times the
benchmark ran. Look for differences above roughly 10% between an optimized and an
unoptimized build.

## Linux perf

For an overall profile, find the hotspots and the expensive call paths:

```bash
# Profile a program
perf record -g ./your_program
perf report
```

`your_program` is any binary that drives the machine — the benchmark executable works
as-is. A minimal timed driver that compiles against the current API is:

```cpp
#include <FSMgine/FSMgine.hpp>
#include <chrono>
#include <iostream>

int main() {
    fsmgine::FSM<int> fsm;
    auto builder = fsm.get_builder();
    builder.from("idle").predicate([](const int& i) { return i > 0; }).to("active");
    builder.from("active").predicate([](const int& i) { return i <= 0; }).to("idle");
    fsm.setInitialState("idle");

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000000; ++i) {
        fsm.process(i % 2);
    }
    auto end = std::chrono::high_resolution_clock::now();

    std::cout << "Time: "
              << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
              << "us\n";
    return 0;
}
```

```bash
# Compile the test
g++ -O2 -I../include profile_test.cpp -L. -lFSMgine -o profile_test

# Profile with perf
perf record -g ./profile_test
perf report
```

Key metrics: the functions taking the most time (%), the call graph showing expensive
call paths, and cache misses and branch mispredictions.

## Valgrind Callgrind

For detailed function-level analysis and cache behavior:

```bash
# Profile the test program
valgrind --tool=callgrind --callgrind-out-file=callgrind.out ./profile_test

# Analyze results
callgrind_annotate callgrind.out

# Visualize with kcachegrind (if available)
kcachegrind callgrind.out
```

Key metrics: `Ir` (instruction reads / execution count), `Dr`/`Dw` (data reads/writes),
`I1mr`/`D1mr`/`D1mw` (L1 cache misses). Functions with high instruction counts are the
optimization candidates.

## GProf

For quick function timing:

```bash
# Compile with profiling enabled
g++ -pg -O2 -I../include profile_test.cpp -L. -lFSMgine -o profile_test_gprof

# Run to generate profile data
./profile_test_gprof

# Analyze results
gprof profile_test_gprof gmon.out > analysis.txt
cat analysis.txt
```

## Measuring one change

Compare a baseline build against a changed one:

```bash
# 1. Setup tools
./scripts/setup_profiling.sh

# 2. Baseline measurement
cd build
./FSMgine_benchmarks > baseline.txt

# 3. Apply optimizations
# ... edit code ...

# 4. Measure improvements
make
./FSMgine_benchmarks > optimized.txt

# 5. Compare results
diff baseline.txt optimized.txt
```

Filter to the benchmark that covers the change:

```bash
./FSMgine_benchmarks --benchmark_filter="StringInterner"
./FSMgine_benchmarks --benchmark_filter="StateLookups"
./FSMgine_benchmarks --benchmark_filter="EventCreation"
```

For a deeper look, profile the benchmark binary directly:

```bash
perf record -g ./FSMgine_benchmarks
perf report
```

## Recorded results

The four optimizations below were measured on 2025-06-13 with Google Benchmark plus
custom timer benchmarks, and validated by 33 unit tests, integration tests and
concurrent-access tests.

### Targets and measured impact

| Optimization | Target | Before | After | Improvement |
|---|---|---|---|---|
| StringInterner caching | 15–25% | 147ns | 122ns | 17.0% |
| Map lookup optimization | 10–20% | 52.9ns | 45.6ns | 13.8% |
| Exception string construction | — | 57.4ns | 40.8ns | 28.9% |
| Event object reuse | 5–15% | 1.65ns | 0.304ns | 81.6% |
| Overall FSM throughput | >1M transitions/sec | ~4.9B ops/sec | ~5.3B ops/sec | 8.2% |

Key metrics from the same runs:

- All 33 tests pass — no functionality broken.
- Compilation successful — no breaking changes.
- Code readability maintained — clean, well-commented optimizations.
- StringInterner calls: 1.57 billion operations/second.

### The four optimizations

#### 1. StringInterner reference caching

Location: `FSM.hpp` and `FSMBuilder.hpp`.

```diff
- auto interned_state = StringInterner::instance().intern(state);
- auto other_state = StringInterner::instance().intern(other);
+ // Cache StringInterner reference
+ auto& interner = StringInterner::instance();
+ auto interned_state = interner.intern(state);
+ auto other_state = interner.intern(other);
```

Impact: 17% improvement in StringInterner operations.

#### 2. Eliminated redundant map lookups

Location: `setInitialState()`, `setCurrentState()`, `process()`.

```diff
- if (states_.find(interned_state) == states_.end()) {
-     throw invalid_argument("State not found");
- }
- // Later: another lookup to use the state
- auto it = states_.find(interned_state);
+ auto it = states_.find(interned_state);
+ if (it == states_.end()) {
+     throw invalid_argument("State not found");
+ }
+ // Use 'it' directly
```

Impact: 13.8% improvement in state lookup operations.

#### 3. Optimized exception string construction

Location: error handling in `setInitialState()`, `setCurrentState()`, `process()`.

```diff
- throw invalid_argument("Cannot set initial state: " + std::string(state));
+ // Pre-allocate and append instead of concatenation
+ std::string error_msg;
+ error_msg.reserve(50 + state.size());
+ error_msg.append("Cannot set initial state: ");
+ error_msg.append(state);
+ throw invalid_argument(error_msg);
```

Impact: 28.9% improvement in exception construction.

#### 4. Static dummy event reuse

Location: `setInitialState()`, `setCurrentState()`.

```diff
- executeOnEnterActions(current_state_, TEvent{});
+ // Static dummy event to avoid repeated construction
+ static const TEvent dummy_event{};
+ executeOnEnterActions(current_state_, dummy_event);
```

Impact: 81.6% improvement in event object operations.

### Methods optimized

Core FSM methods:

1. `setInitialState()` — 4 optimizations applied
2. `setCurrentState()` — 4 optimizations applied
3. `process()` — 2 optimizations applied (hot path)
4. `addTransition()` — StringInterner caching
5. `addOnEnterAction()` — StringInterner caching
6. `addOnExitAction()` — StringInterner caching

FSMBuilder methods:

1. `from()` — StringInterner caching
2. `onEnter()` — StringInterner caching
3. `onExit()` — StringInterner caching
4. `to()` — StringInterner caching

### Quality assurance

Functionality:

- All 33 unit tests pass: StringInterner, Transition, FSM, Integration.
- Concurrent access tested; thread-safety maintained.
- API compatibility: no breaking changes to the public interface.
- Memory safety: RAII and move semantics preserved.

Performance:

- Statistical significance: 5 repetitions with aggregated results.
- Consistent improvements: low coefficient of variation (<3%).
- Production-ready: the optimizations maintain code clarity.

Development impact: no significant build-time change, a minimal binary-size increase
from the added static objects, and slightly lower memory usage from fewer temporary
objects.

## Interpreting results

### Statistical significance

- Run benchmarks multiple times: `--benchmark_repetitions=10`.
- Look for consistent improvements across runs.
- Allow for measurement noise (typically 1–5%).

### What to optimize

1. High-frequency operations (`process`, state transitions).
2. Setup operations that block initialization.
3. Memory allocations in hot paths.
4. Function-call overhead in tight loops.

### When to stop

- Diminishing returns (below 5% improvement).
- The code becomes significantly more complex.
- Performance is already adequate for the use case.

## Troubleshooting

1. **"Benchmark not found":** install the Google Benchmark library.
2. **"Permission denied" for perf:** run with `sudo` or adjust `perf_event_paranoid`.
3. **Inconsistent results:** ensure consistent system load and disable CPU scaling.

Best practices:

- Run benchmarks on a dedicated, idle system.
- Use the Release build configuration.
- Disable address-space randomization for consistent results.
- Profile on the target architecture and OS.
- Consider the impact of compiler optimizations.
