#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <iomanip>
#include "FSMgine/FSM.hpp"
#include "FSMgine/FSMBuilder.hpp"
#include "FSMgine/StringInterner.hpp"

using namespace fsmgine;
using namespace std::chrono;

// Simple timer class
class Timer {
    high_resolution_clock::time_point start_time;
public:
    void start() { start_time = high_resolution_clock::now(); }
    double elapsed_ms() {
        auto end_time = high_resolution_clock::now();
        return duration_cast<nanoseconds>(end_time - start_time).count() / 1000000.0;
    }
};

// Benchmark function template
template<typename Func>
double benchmark(const std::string& name, Func func, int iterations = 100000) {
    std::cout << "Running " << name << " (" << iterations << " iterations)... ";
    std::cout.flush();
    
    Timer timer;
    timer.start();
    
    for (int i = 0; i < iterations; ++i) {
        func();
    }
    
    double elapsed = timer.elapsed_ms();
    double per_op = elapsed * 1000.0 / iterations; // nanoseconds per operation
    
    std::cout << std::fixed << std::setprecision(2) 
              << elapsed << "ms total, " 
              << per_op << "ns per operation\n";
    
    return per_op;
}

// Test event type
struct TestEvent {
    int value = 0;
    std::string data = "test";
};

int main() {
    std::cout << "FSMgine Simple Performance Benchmark\n";
    std::cout << "=====================================\n\n";
    
    // Test 1: StringInterner - Repeated Singleton Calls
    std::vector<std::string> test_states = {
        "idle", "processing", "completed", "error", "retry",
        "waiting", "active", "suspended", "terminated", "initialized"
    };
    
    auto time_singleton = benchmark("StringInterner Repeated Singleton", [&]() {
        StringInterner::instance().clear();
        for (const auto& state : test_states) {
            auto interned = StringInterner::instance().intern(state);
            (void)interned; // Prevent optimization
        }
    }, 10000);
    
    // Test 2: StringInterner - Cached Reference
    auto time_cached = benchmark("StringInterner Cached Reference", [&]() {
        StringInterner::instance().clear();
        auto& interner = StringInterner::instance();
        for (const auto& state : test_states) {
            auto interned = interner.intern(state);
            (void)interned; // Prevent optimization
        }
    }, 10000);
    
    std::cout << "StringInterner optimization: " << std::setprecision(1) 
              << ((time_singleton - time_cached) / time_singleton * 100) << "% improvement\n\n";
    
    // Test 3: FSM State Transitions
    FSM<TestEvent> fsm;
    auto builder = fsm.get_builder();
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
    auto time_transitions = benchmark("FSM State Transitions", [&]() {
        fsm.setCurrentState("idle");
        event.value = 5;
        fsm.process(event);  // idle -> processing
        event.value = 15;
        fsm.process(event);  // processing -> completed
    }, 50000);
    
    // Test 4: Exception String Construction - Current Method
    std::string_view test_state = "nonexistent_state";
    auto time_exception_old = benchmark("Exception String Construction (Current)", [&]() {
        std::string msg = "Cannot set initial state to undefined state: " + std::string(test_state);
        (void)msg;
    }, 100000);
    
    // Test 5: Exception String Construction - Optimized Method
    auto time_exception_new = benchmark("Exception String Construction (Optimized)", [&]() {
        std::string msg;
        msg.reserve(50 + test_state.size());
        msg.append("Cannot set initial state to undefined state: ");
        msg.append(test_state);
        (void)msg;
    }, 100000);
    
    std::cout << "Exception string optimization: " << std::setprecision(1)
              << ((time_exception_old - time_exception_new) / time_exception_old * 100) << "% improvement\n\n";
    
    // Test 6: Event Object Creation
    auto time_event_creation = benchmark("Event Object Creation", [&]() {
        TestEvent dummy_event{};
        (void)dummy_event;
    }, 1000000);
    
    static const TestEvent static_event{};
    auto time_static_event = benchmark("Static Event Reference", [&]() {
        const TestEvent& dummy_event = static_event;
        (void)dummy_event;
    }, 1000000);
    
    std::cout << "Static event optimization: " << std::setprecision(1)
              << ((time_event_creation - time_static_event) / time_event_creation * 100) << "% improvement\n\n";
    
    // Summary
    std::cout << "Performance Summary:\n";
    std::cout << "===================\n";
    std::cout << "FSM transitions: " << std::setprecision(0) 
              << (1000000000.0 / time_transitions) << " operations/second\n";
    std::cout << "StringInterner calls: " << std::setprecision(0)
              << (1000000000.0 / time_cached) << " operations/second\n";
    
    std::cout << "\nRecommended optimizations based on results:\n";
    if ((time_singleton - time_cached) / time_singleton > 0.1) {
        std::cout << "✓ Cache StringInterner reference (significant improvement)\n";
    }
    if ((time_exception_old - time_exception_new) / time_exception_old > 0.05) {
        std::cout << "✓ Optimize exception string construction\n";
    }
    if ((time_event_creation - time_static_event) / time_event_creation > 0.05) {
        std::cout << "✓ Use static dummy events\n";
    }
    
    return 0;
} 