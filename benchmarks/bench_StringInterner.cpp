#include <benchmark/benchmark.h>
#include "FSMgine/StringInterner.hpp"
#include <vector>
#include <string>

using namespace fsmgine;

// Benchmark current approach - repeated singleton calls
static void BM_StringInterner_RepeatedSingleton(benchmark::State& state) {
    StringInterner::instance().clear();
    
    std::vector<std::string> states = {
        "idle", "processing", "completed", "error", "retry",
        "waiting", "active", "suspended", "terminated", "initialized"
    };
    
    for (auto _ : state) {
        for (const auto& s : states) {
            // Current approach - singleton call each time
            auto interned = StringInterner::instance().intern(s);
            benchmark::DoNotOptimize(interned);
        }
    }
}
BENCHMARK(BM_StringInterner_RepeatedSingleton);

// Benchmark optimized approach - cached reference
static void BM_StringInterner_CachedReference(benchmark::State& state) {
    StringInterner::instance().clear();
    
    std::vector<std::string> states = {
        "idle", "processing", "completed", "error", "retry",
        "waiting", "active", "suspended", "terminated", "initialized"
    };
    
    for (auto _ : state) {
        // Optimized approach - cache the reference
        auto& interner = StringInterner::instance();
        for (const auto& s : states) {
            auto interned = interner.intern(s);
            benchmark::DoNotOptimize(interned);
        }
    }
}
BENCHMARK(BM_StringInterner_CachedReference);

// Benchmark string construction in exceptions
static void BM_ExceptionStringConstruction(benchmark::State& state) {
    std::string_view test_state = "nonexistent_state";
    
    for (auto _ : state) {
        try {
            // Current approach - string concatenation
            std::string msg = "Cannot set initial state to undefined state: " + std::string(test_state);
            benchmark::DoNotOptimize(msg);
        } catch (...) {
            // Won't throw, just measuring string construction
        }
    }
}
BENCHMARK(BM_ExceptionStringConstruction);

// Benchmark optimized exception construction
static void BM_ExceptionOptimizedConstruction(benchmark::State& state) {
    std::string_view test_state = "nonexistent_state";
    
    for (auto _ : state) {
        try {
            // Optimized approach - avoid string concatenation
            std::string msg;
            msg.reserve(50 + test_state.size());
            msg.append("Cannot set initial state to undefined state: ");
            msg.append(test_state);
            benchmark::DoNotOptimize(msg);
        } catch (...) {
            // Won't throw, just measuring string construction
        }
    }
}
BENCHMARK(BM_ExceptionOptimizedConstruction); 