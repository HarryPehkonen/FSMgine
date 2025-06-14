#include <benchmark/benchmark.h>
#include "FSMgine/FSM.hpp"
#include "FSMgine/FSMBuilder.hpp"
#include "FSMgine/StringInterner.hpp"
#include <variant>

using namespace fsmgine;

// Event type for testing
struct TestEvent {
    int value = 0;
    std::string data = "test";
};

// Benchmark FSM state transitions
static void BM_FSM_StateTransitions(benchmark::State& state) {
    FSM<TestEvent> fsm;
    
    // Setup FSM with multiple states and transitions
    auto builder = fsm.get_builder();
    
    // Set up enter actions
    builder.onEnter("idle", [](const TestEvent&) { /* do nothing */ });
    builder.onEnter("processing", [](const TestEvent&) { /* do nothing */ });
    builder.onEnter("completed", [](const TestEvent&) { /* do nothing */ });
    builder.onEnter("error", [](const TestEvent&) { /* do nothing */ });
    
    // Set up transitions
    builder.from("idle")
        .predicate([](const TestEvent& e) { return e.value > 0; })
        .to("processing");
    
    builder.from("processing")
        .predicate([](const TestEvent& e) { return e.value > 10; })
        .to("completed");
    
    builder.from("processing")
        .predicate([](const TestEvent& e) { return e.value < 0; })
        .to("error");
    
    fsm.setInitialState("idle");
    
    TestEvent event;
    for (auto _ : state) {
        fsm.setCurrentState("idle");
        
        // Simulate typical FSM usage
        event.value = 5;
        fsm.process(event);  // idle -> processing
        
        event.value = 15;
        fsm.process(event);  // processing -> completed
        
        benchmark::DoNotOptimize(fsm.getCurrentState());
    }
}
BENCHMARK(BM_FSM_StateTransitions);

// Benchmark repeated state lookups (current approach)
static void BM_FSM_RepeatedStateLookups(benchmark::State& state) {
    // Simulate the current setInitialState implementation
    std::unordered_map<std::string_view, int> states;
    states["idle"] = 1;
    states["processing"] = 2;
    states["completed"] = 3;
    
    std::string_view test_state = "idle";
    
    for (auto _ : state) {
        // Current approach - multiple lookups
        auto& interner = StringInterner::instance();
        auto interned_state = interner.intern(test_state);
        
        // First lookup for validation
        if (states.find(interned_state) == states.end()) {
            continue; // Would throw in real code
        }
        
        // Second lookup for actual use (simulated)
        auto it = states.find(interned_state);
        benchmark::DoNotOptimize(it->second);
    }
}
BENCHMARK(BM_FSM_RepeatedStateLookups);

// Benchmark optimized state lookups
static void BM_FSM_OptimizedStateLookups(benchmark::State& state) {
    // Simulate optimized approach
    std::unordered_map<std::string_view, int> states;
    states["idle"] = 1;
    states["processing"] = 2;
    states["completed"] = 3;
    
    std::string_view test_state = "idle";
    
    for (auto _ : state) {
        // Optimized approach - single lookup
        auto& interner = StringInterner::instance();
        auto interned_state = interner.intern(test_state);
        
        // Single lookup, reuse iterator
        auto it = states.find(interned_state);
        if (it == states.end()) {
            continue; // Would throw in real code
        }
        
        benchmark::DoNotOptimize(it->second);
    }
}
BENCHMARK(BM_FSM_OptimizedStateLookups);

// Benchmark event object creation
static void BM_FSM_EventCreation_Current(benchmark::State& state) {
    for (auto _ : state) {
        // Current approach - create new event each time
        TestEvent dummy_event{};
        benchmark::DoNotOptimize(dummy_event);
    }
}
BENCHMARK(BM_FSM_EventCreation_Current);

// Benchmark static event reuse
static void BM_FSM_EventCreation_Static(benchmark::State& state) {
    static const TestEvent dummy_event{};
    
    for (auto _ : state) {
        // Optimized approach - static event
        benchmark::DoNotOptimize(dummy_event);
    }
}
BENCHMARK(BM_FSM_EventCreation_Static);

// Comprehensive FSM benchmark with realistic workload
static void BM_FSM_RealisticWorkload(benchmark::State& state) {
    FSM<TestEvent> fsm;
    
    // Create a more complex FSM
    auto builder = fsm.get_builder();
    
    // Build a workflow-like FSM with transitions
    builder.from("idle")
        .predicate([](const TestEvent& e) { return e.value == 1; })
        .to("validating");
    
    builder.from("idle")
        .predicate([](const TestEvent& e) { return e.value < 0; })
        .to("error");
    
    builder.from("validating")
        .predicate([](const TestEvent& e) { return e.value == 2; })
        .to("processing");
    
    builder.from("validating")
        .predicate([](const TestEvent& e) { return e.value == 0; })
        .to("rejected");
    
    builder.from("processing")
        .predicate([](const TestEvent& e) { return e.value == 3; })
        .to("completed");
    
    builder.from("processing")
        .predicate([](const TestEvent& e) { return e.value == -1; })
        .to("retrying");
    
    builder.from("retrying")
        .predicate([](const TestEvent& e) { return e.value == 2; })
        .to("processing");
    
    builder.from("retrying")
        .predicate([](const TestEvent& e) { return e.value == -2; })
        .to("failed");
    
    fsm.setInitialState("idle");
    
    // Simulate realistic event sequence
    std::vector<int> event_sequence = {1, 2, 3}; // idle->validating->processing->completed
    
    for (auto _ : state) {
        fsm.setCurrentState("idle");
        
        for (int val : event_sequence) {
            TestEvent event{val, "test_data"};
            fsm.process(event);
        }
        
        benchmark::DoNotOptimize(fsm.getCurrentState());
    }
}
BENCHMARK(BM_FSM_RealisticWorkload); 