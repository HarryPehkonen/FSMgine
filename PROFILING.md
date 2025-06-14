# FSMgine Profiling Guide

This guide explains how to profile FSMgine performance and measure optimization impacts.

## Quick Start

1. **Setup profiling tools:**
   ```bash
   ./scripts/setup_profiling.sh
   ```

2. **Build with benchmarks:**
   ```bash
   mkdir build && cd build
   cmake -DBUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release ..
   make
   ```

3. **Run benchmarks:**
   ```bash
   make benchmark
   # or directly:
   ./FSMgine_benchmarks
   ```

## Available Profiling Tools

### 1. Google Benchmark (Recommended)

**Best for:** Measuring specific optimizations, comparing before/after

**Usage:**
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

### 2. Linux Perf

**Best for:** System-wide profiling, finding hotspots

```bash
# Profile a program
perf record -g ./your_program
perf report
```

### 3. Valgrind Callgrind

**Best for:** Detailed function analysis

```bash
valgrind --tool=callgrind ./your_program
callgrind_annotate callgrind.out
```

## Target Optimizations

The benchmarks test these specific optimizations:

1. **StringInterner Caching** - Cache singleton reference
2. **Map Lookup Reduction** - Eliminate redundant lookups  
3. **Event Object Reuse** - Static dummy events
4. **Exception String Construction** - Optimize error messages

## Performance Targets

- StringInterner caching: 15-25% improvement
- Map lookup optimization: 10-20% improvement  
- Event reuse: 5-15% improvement
- Overall throughput: >1M transitions/second

## Example Workflow

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

## Profiling Tools Overview

### 1. Google Benchmark (Recommended for Microbenchmarks)

**Best for:** Measuring specific optimizations, comparing before/after performance

**Usage:**
```bash
# Run all benchmarks
./FSMgine_benchmarks

# Run specific benchmark pattern
./FSMgine_benchmarks --benchmark_filter="StringInterner"

# Run with different repetitions for statistical significance
./FSMgine_benchmarks --benchmark_repetitions=10

# Output to JSON for analysis
./FSMgine_benchmarks --benchmark_format=json --benchmark_out=results.json
```

**Sample Output:**
```
Benchmark                               Time             CPU   Iterations
------------------------------------------------------------------------
BM_StringInterner_RepeatedSingleton    245 ns          245 ns      2856789
BM_StringInterner_CachedReference      198 ns          198 ns      3534782
BM_FSM_StateTransitions               1247 ns         1247 ns       561892
```

**Interpreting Results:**
- **Time/CPU:** Average time per iteration
- **Iterations:** Number of times the benchmark ran
- Look for significant differences (>10%) between optimized and unoptimized versions

### 2. Linux Perf (Best for Overall Profiling)

**Best for:** Understanding overall program performance, finding hotspots

**Usage:**
```bash
# Create a test program first
cat > profile_test.cpp << 'EOF'
#include "FSMgine/FSM.hpp"
#include <chrono>
#include <iostream>

int main() {
    FSM<int> fsm;
    auto builder = fsm.get_builder();
    
    // Setup FSM
    builder.state("idle").transition_to("active").when([](int i) { return i > 0; });
    builder.state("active").transition_to("idle").when([](int i) { return i <= 0; });
    
    fsm.setInitialState("idle");
    
    // Performance test
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000000; ++i) {
        fsm.process(i % 2);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    std::cout << "Time: " << std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() << "μs\n";
    return 0;
}
EOF

# Compile the test
g++ -O2 -I../include profile_test.cpp -L. -lFSMgine -o profile_test

# Profile with perf
perf record -g ./profile_test
perf report
```

**Key Metrics to Look For:**
- Functions taking the most time (%)
- Call graph showing expensive call paths
- Cache misses, branch mispredictions

### 3. Valgrind Callgrind (Detailed Call Analysis)

**Best for:** Detailed function-level analysis, cache behavior

**Usage:**
```bash
# Profile the test program
valgrind --tool=callgrind --callgrind-out-file=callgrind.out ./profile_test

# Analyze results
callgrind_annotate callgrind.out

# Visualize with kcachegrind (if available)
kcachegrind callgrind.out
```

**Key Metrics:**
- **Ir:** Instruction reads (execution count)
- **Dr/Dw:** Data reads/writes
- **I1mr/D1mr/D1mw:** L1 cache misses
- Functions with high instruction counts are optimization candidates

### 4. GProf (Simple Call Graph Profiling)

**Best for:** Quick function timing analysis

**Usage:**
```bash
# Compile with profiling enabled
g++ -pg -O2 -I../include profile_test.cpp -L. -lFSMgine -o profile_test_gprof

# Run to generate profile data
./profile_test_gprof

# Analyze results
gprof profile_test_gprof gmon.out > analysis.txt
cat analysis.txt
```

## Measuring Specific Optimizations

### 1. StringInterner Singleton Caching

**Before optimization:**
```cpp
// In setInitialState()
auto interned_state = StringInterner::instance().intern(state);
// In process()
auto interned_state = StringInterner::instance().intern(other_state);
```

**After optimization:**
```cpp
// Cache the reference
auto& interner = StringInterner::instance();
auto interned_state = interner.intern(state);
auto other_interned = interner.intern(other_state);
```

**Measurement:**
```bash
./FSMgine_benchmarks --benchmark_filter="StringInterner"
```

### 2. Map Lookup Optimization

**Before optimization:**
```cpp
if (states_.find(interned_state) == states_.end()) {
    throw invalid_argument("State not found");
}
// Later: another lookup to use the state
auto it = states_.find(interned_state);
```

**After optimization:**
```cpp
auto it = states_.find(interned_state);
if (it == states_.end()) {
    throw invalid_argument("State not found");
}
// Use 'it' directly
```

**Measurement:**
```bash
./FSMgine_benchmarks --benchmark_filter="StateLookups"
```

### 3. Event Object Creation

**Measurement:**
```bash
./FSMgine_benchmarks --benchmark_filter="EventCreation"
```

## Comprehensive Performance Testing

### Create a Realistic Test Case

```cpp
// realistic_performance_test.cpp
#include "FSMgine/FSM.hpp"
#include <chrono>
#include <vector>
#include <random>

struct ComplexEvent {
    int type;
    std::string data;
    double value;
};

int main() {
    FSM<ComplexEvent> fsm;
    auto builder = fsm.get_builder();
    
    // Create a complex FSM with many states and transitions
    // ... (detailed setup)
    
    // Generate realistic workload
    std::vector<ComplexEvent> events;
    // ... (generate events)
    
    // Measure performance
    auto start = std::chrono::high_resolution_clock::now();
    for (const auto& event : events) {
        fsm.process(event);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    // Report results
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "Processed " << events.size() << " events in " << duration.count() << "μs\n";
    std::cout << "Throughput: " << (events.size() * 1000000.0 / duration.count()) << " events/second\n";
    
    return 0;
}
```

## Automation and CI Integration

### Continuous Benchmarking

Create a script for automated performance testing:

```bash
#!/bin/bash
# scripts/run_benchmarks.sh

echo "Running FSMgine performance benchmarks..."

# Build optimized version
cmake -DBUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release ..
make

# Run benchmarks and save results
./FSMgine_benchmarks --benchmark_format=json --benchmark_out=benchmark_results.json

# Compare with baseline (if exists)
if [ -f benchmark_baseline.json ]; then
    echo "Comparing with baseline..."
    # Use benchmark comparison tools or custom script
fi

echo "Benchmark results saved to benchmark_results.json"
```

## Performance Targets

Based on typical FSM usage patterns, target improvements:

- **StringInterner caching:** 15-25% improvement in setup methods
- **Map lookup optimization:** 10-20% improvement in state transitions
- **Event object reuse:** 5-15% improvement in frequent transitions
- **Overall FSM throughput:** Target >1M state transitions/second for simple FSMs

## Interpreting Results

### Statistical Significance
- Run benchmarks multiple times: `--benchmark_repetitions=10`
- Look for consistent improvements across runs
- Consider measurement noise (typically 1-5%)

### What to Optimize
1. **High-frequency operations** (process, state transitions)
2. **Setup operations** that block initialization
3. **Memory allocations** in hot paths
4. **Function call overhead** in tight loops

### When to Stop Optimizing
- Diminishing returns (<5% improvement)
- Code becomes significantly more complex
- Performance is already adequate for use case

## Troubleshooting

### Common Issues

1. **"Benchmark not found":** Install Google Benchmark library
2. **"Permission denied" for perf:** Run with sudo or adjust perf_event_paranoid
3. **Inconsistent results:** Ensure consistent system load, disable CPU scaling

### Best Practices

- Run benchmarks on dedicated/idle system
- Use Release build configuration
- Disable address space randomization for consistent results
- Profile on target architecture/OS
- Consider compiler optimizations impact

## Example Workflow

```bash
# 1. Setup
./scripts/setup_profiling.sh

# 2. Build and baseline
mkdir build && cd build
cmake -DBUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release ..
make
./FSMgine_benchmarks > baseline_results.txt

# 3. Apply optimization
# ... edit source code ...

# 4. Rebuild and test
make
./FSMgine_benchmarks > optimized_results.txt

# 5. Compare results
diff baseline_results.txt optimized_results.txt

# 6. Detailed profiling if needed
perf record -g ./FSMgine_benchmarks
perf report
```

This comprehensive approach will help you measure and validate the performance improvements from the suggested optimizations. 