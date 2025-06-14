#!/bin/bash

# Simple profiling runner for FSMgine
# Usage: ./scripts/profile.sh [benchmark|perf|callgrind|all]

cd "$(dirname "$0")/.."

TOOL=${1:-benchmark}
BUILD_DIR=${BUILD_DIR:-build}

# Ensure build directory exists and is configured
if [ ! -d "$BUILD_DIR" ]; then
    echo "Creating build directory..."
    mkdir -p "$BUILD_DIR"
fi

cd "$BUILD_DIR"

if [ ! -f "Makefile" ]; then
    echo "Configuring build..."
    cmake -DBUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release ..
fi

# Build the project
echo "Building FSMgine..."
make -j$(nproc)

case "$TOOL" in
    "benchmark"|"bench")
        echo "Running Google Benchmark..."
        if [ -f "./benchmarks/FSMgine_benchmarks" ]; then
            ./benchmarks/FSMgine_benchmarks
        elif [ -f "./benchmarks/FSMgine_simple_benchmark" ]; then
            echo "Google Benchmark not available, running simple benchmark..."
            ./benchmarks/FSMgine_simple_benchmark
        else
            echo "No benchmark executable found. Make sure benchmarks are built."
            echo "Run: cmake -DBUILD_BENCHMARKS=ON .."
            exit 1
        fi
        ;;
    
    "perf")
        echo "Running perf profiling..."
        if ! command -v perf &> /dev/null; then
            echo "perf not found. Install with: sudo apt-get install linux-tools-generic"
            exit 1
        fi
        if [ -f "./benchmarks/FSMgine_benchmarks" ]; then
            echo "Recording performance data..."
            perf record -g --call-graph=dwarf ./benchmarks/FSMgine_benchmarks --benchmark_min_time=0.1
            echo "Generating report..."
            perf report
        elif [ -f "./benchmarks/FSMgine_simple_benchmark" ]; then
            echo "Recording performance data with simple benchmark..."
            perf record -g --call-graph=dwarf ./benchmarks/FSMgine_simple_benchmark
            echo "Generating report..."
            perf report
        else
            echo "No benchmark executable found."
            exit 1
        fi
        ;;
    
    "callgrind")
        echo "Running Valgrind Callgrind..."
        if ! command -v valgrind &> /dev/null; then
            echo "valgrind not found. Install with: sudo apt-get install valgrind"
            exit 1
        fi
        if [ -f "./benchmarks/FSMgine_benchmarks" ]; then
            echo "Profiling with callgrind (this may take a while)..."
            valgrind --tool=callgrind --callgrind-out-file=callgrind.out ./benchmarks/FSMgine_benchmarks --benchmark_min_time=0.01
            echo "Generating report..."
            callgrind_annotate callgrind.out | head -50
            echo ""
            echo "Full report saved to callgrind.out"
            echo "View with: kcachegrind callgrind.out (if available)"
        elif [ -f "./benchmarks/FSMgine_simple_benchmark" ]; then
            echo "Profiling simple benchmark with callgrind (this may take a while)..."
            valgrind --tool=callgrind --callgrind-out-file=callgrind.out ./benchmarks/FSMgine_simple_benchmark
            echo "Generating report..."
            callgrind_annotate callgrind.out | head -50
            echo ""
            echo "Full report saved to callgrind.out"
            echo "View with: kcachegrind callgrind.out (if available)"
        else
            echo "No benchmark executable found."
            exit 1
        fi
        ;;
    
    "all")
        echo "Running all profiling tools..."
        ./scripts/profile.sh benchmark
        echo -e "\n=== PERF PROFILING ===\n"
        ./scripts/profile.sh perf
        echo -e "\n=== CALLGRIND PROFILING ===\n"
        ./scripts/profile.sh callgrind
        ;;
    
    *)
        echo "Usage: $0 [benchmark|perf|callgrind|all]"
        echo ""
        echo "Available tools:"
        echo "  benchmark  - Run Google Benchmark microbenchmarks (default)"
        echo "  perf       - Profile with Linux perf"
        echo "  callgrind  - Profile with Valgrind Callgrind"
        echo "  all        - Run all profiling tools"
        echo ""
        echo "Examples:"
        echo "  $0 benchmark    # Quick benchmark"
        echo "  $0 perf        # System profiling"
        echo "  $0 all         # Complete analysis"
        exit 1
        ;;
esac

echo ""
echo "Profiling complete. Results available in $BUILD_DIR/" 