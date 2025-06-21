#include <gtest/gtest.h>
#include <thread>
#include "FSMgine/FSM.hpp"
#include "FSMgine/FSMBuilder.hpp"
#include "FSMgine/StringInterner.hpp"

using namespace fsmgine;

// Use an event-less FSM (std::monostate) for these tests to ensure process() works.
using TestFSM = FSM<>; // Default template argument is std::monostate

class FSMTest : public ::testing::Test {
protected:
    void SetUp() override {
        StringInterner::instance().clear();
        action_call_count = 0;
        on_enter_called = false;
        on_exit_called = false;
    }

    int action_call_count = 0;
    bool on_enter_called = false;
    bool on_exit_called = false;
};

TEST_F(FSMTest, DefaultConstructor) {
    TestFSM fsm;

    // Should throw when trying to get current state without setting initial state
    EXPECT_THROW(fsm.getCurrentState(), FSMNotInitializedError);
    EXPECT_THROW(fsm.process(), FSMNotInitializedError);
}

TEST_F(FSMTest, SimpleTransition) {
    TestFSM fsm;

    // Build simple transition
    fsm.get_builder()
        .from("START")
        .to("END");

    fsm.setInitialState("START");
    EXPECT_EQ(fsm.getCurrentState(), "START");

    // Process should transition to END
    bool transitioned = fsm.process();
    EXPECT_TRUE(transitioned);
    EXPECT_EQ(fsm.getCurrentState(), "END");

    // Process again should not transition (no outgoing transitions from END)
    transitioned = fsm.process();
    EXPECT_FALSE(transitioned);
    EXPECT_EQ(fsm.getCurrentState(), "END");
}

TEST_F(FSMTest, TransitionWithPredicate) {
    TestFSM fsm;
    bool condition = false;

    fsm.get_builder()
        .from("WAITING")
        .predicate([&condition](const auto&) { return condition; })
        .to("READY");

    fsm.setInitialState("WAITING");
    EXPECT_EQ(fsm.getCurrentState(), "WAITING");

    // Process should not transition (predicate is false)
    bool transitioned = fsm.process();
    EXPECT_FALSE(transitioned);
    EXPECT_EQ(fsm.getCurrentState(), "WAITING");

    // Set condition to true
    condition = true;
    transitioned = fsm.process();
    EXPECT_TRUE(transitioned);
    EXPECT_EQ(fsm.getCurrentState(), "READY");
}

TEST_F(FSMTest, TransitionWithAction) {
    TestFSM fsm;

    fsm.get_builder()
        .from("START")
        .action([this](const auto&) { action_call_count++; })
        .to("END");

    fsm.setInitialState("START");
    EXPECT_EQ(action_call_count, 0);

    bool transitioned = fsm.process();
    EXPECT_TRUE(transitioned);
    EXPECT_EQ(action_call_count, 1);
    EXPECT_EQ(fsm.getCurrentState(), "END");
}

TEST_F(FSMTest, MultiplePredicatesAndActions) {
    TestFSM fsm;
    bool cond1 = true;
    bool cond2 = true;

    fsm.get_builder()
        .from("START")
        .predicate([&cond1](const auto&) { return cond1; })
        .predicate([&cond2](const auto&) { return cond2; })
        .action([this](const auto&) { action_call_count++; })
        .action([this](const auto&) { action_call_count++; })
        .to("END");

    fsm.setInitialState("START");

    // Both predicates true - should transition and execute both actions
    bool transitioned = fsm.process();
    EXPECT_TRUE(transitioned);
    EXPECT_EQ(action_call_count, 2);
    EXPECT_EQ(fsm.getCurrentState(), "END");
}

TEST_F(FSMTest, MultipleTransitionsFirstValidWins) {
    TestFSM fsm;
    bool cond1 = false;
    bool cond2 = true;

    auto builder = fsm.get_builder();

    // First transition (will fail)
    builder.from("START")
        .predicate([&cond1](const auto&) { return cond1; })
        .action([this](const auto&) { action_call_count = 10; })
        .to("BRANCH1");

    // Second transition (will succeed)
    builder.from("START")
        .predicate([&cond2](const auto&) { return cond2; })
        .action([this](const auto&) { action_call_count = 20; })
        .to("BRANCH2");

    fsm.setInitialState("START");

    bool transitioned = fsm.process();
    EXPECT_TRUE(transitioned);
    EXPECT_EQ(action_call_count, 20); // Second transition's action
    EXPECT_EQ(fsm.getCurrentState(), "BRANCH2");
}

TEST_F(FSMTest, OnEnterActions) {
    TestFSM fsm;

    fsm.get_builder()
        .onEnter("START", [this](const auto&) { on_enter_called = true; })
        .from("START")
        .to("TARGET");

    EXPECT_FALSE(on_enter_called);
    fsm.setInitialState("START");
    EXPECT_TRUE(on_enter_called); // Called for START
    on_enter_called = false; // reset

    fsm.process();
    EXPECT_FALSE(on_enter_called); // Not called for TARGET since no action was registered
}

TEST_F(FSMTest, OnExitActions) {
    TestFSM fsm;

    fsm.get_builder()
        .onExit("START", [this](const auto&) { on_exit_called = true; })
        .from("START")
        .to("END");

    fsm.setInitialState("START");
    EXPECT_FALSE(on_exit_called);

    fsm.process();
    EXPECT_TRUE(on_exit_called); // Called when exiting START
}

TEST_F(FSMTest, SelfTransitionNoStateChangeActions) {
    TestFSM fsm;

    fsm.get_builder()
        .onEnter("LOOP", [this](const auto&) { on_enter_called = true; })
        .onExit("LOOP", [this](const auto&) { on_exit_called = true; })
        .from("LOOP")
        .action([this](const auto&) { action_call_count++; })
        .to("LOOP");

    fsm.setInitialState("LOOP");
    // Initial state setting should call onEnter
    EXPECT_TRUE(on_enter_called);
    on_enter_called = false; // Reset for test

    // Self-transition should execute action but not onExit/onEnter
    fsm.process();
    EXPECT_EQ(action_call_count, 1);
    EXPECT_FALSE(on_exit_called);
    EXPECT_FALSE(on_enter_called);
}

TEST_F(FSMTest, SetCurrentStateExecutesActions) {
    TestFSM fsm;

    fsm.get_builder()
        .onEnter("STATE1", [this](const auto&) { action_call_count = 1; })
        .onExit("STATE1", [this](const auto&) { action_call_count = 2; })
        .onEnter("STATE2", [this](const auto&) { action_call_count = 3; })
        .from("STATE1")
        .to("STATE2");

    fsm.setInitialState("STATE1");
    EXPECT_EQ(action_call_count, 1); // onEnter STATE1

    fsm.setCurrentState("STATE2");
    EXPECT_EQ(action_call_count, 3); // onExit STATE1 (=2), then onEnter STATE2 (=3)
}

TEST_F(FSMTest, ErrorHandling) {
    TestFSM fsm;

    // Cannot set initial state to undefined state
    EXPECT_THROW(fsm.setInitialState("UNDEFINED"), FSMInvalidStateError);

    // Cannot set current state to undefined state
    EXPECT_THROW(fsm.setCurrentState("UNDEFINED"), FSMInvalidStateError);

    // After defining a state, should be able to set it
    fsm.get_builder().from("DEFINED").to("ANOTHER");
    EXPECT_NO_THROW(fsm.setInitialState("DEFINED"));
}

TEST_F(FSMTest, FluentBuilderInterface) {
    TestFSM fsm;

    // Test fluent interface chaining
    fsm.get_builder()
        .onEnter("START", [this](const auto&) { action_call_count++; })
        .onExit("START", [this](const auto&) { action_call_count++; })
        .from("START")
        .predicate([](const auto&) { return true; })
        .action([this](const auto&) { action_call_count++; })
        .to("END");

    fsm.setInitialState("START");
    EXPECT_EQ(action_call_count, 1); // onEnter START

    fsm.process();
    EXPECT_EQ(action_call_count, 3); // onExit START + transition action
}

TEST_F(FSMTest, MoveSemantics) {
    TestFSM fsm1;
    fsm1.get_builder().from("A").to("B");
    fsm1.setInitialState("A");

    // Move constructor
    TestFSM fsm2 = std::move(fsm1);
    EXPECT_EQ(fsm2.getCurrentState(), "A");

    // Move assignment
    TestFSM fsm3;
    fsm3 = std::move(fsm2);
    EXPECT_EQ(fsm3.getCurrentState(), "A");

    // Original FSM should be in valid but unspecified state
    // We don't test the moved-from state as it's implementation-defined
}

TEST_F(FSMTest, ConcurrentStateAccess) {
    TestFSM fsm;
    std::atomic<int> exceptions_caught{0};
    std::atomic<int> invalid_states{0};
    std::atomic<int> total_operations{0};
    const int NUM_THREADS = 8;
    const int ITERATIONS = 1000;

    // Build a simple state machine
    fsm.get_builder()
        .from("A")
        .to("B");

    fsm.get_builder()
        .from("B")
        .to("A");

    fsm.setInitialState("A");

    std::vector<std::thread> threads;

    // Create threads that both read and write concurrently
    // This tests for real race conditions like exceptions or corrupted state
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&fsm, &exceptions_caught, &invalid_states, &total_operations, ITERATIONS]() {
            for (int j = 0; j < ITERATIONS; ++j) {
                try {
                    // Mix of read and write operations to create contention
                    if (j % 3 == 0) {
                        // Process the FSM (write operation)
                        fsm.process();
                        total_operations++;
                    } else {
                        // Read the current state (read operation)
                        auto state = fsm.getCurrentState();
                        total_operations++;

                        // Check if we got a valid state
                        if (state != "A" && state != "B") {
                            invalid_states++;
                        }
                    }
                } catch (const std::exception& e) {
                    exceptions_caught++;
                } catch (...) {
                    exceptions_caught++;
                }
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "Concurrent access test results:" << std::endl;
    std::cout << "Total operations: " << total_operations.load() << std::endl;
    std::cout << "Exceptions caught: " << exceptions_caught.load() << std::endl;
    std::cout << "Invalid states: " << invalid_states.load() << std::endl;

    // With proper multithreading support, we should have:
    // - No exceptions (race conditions cause runtime errors)
    // - No invalid states (race conditions can corrupt state)
    // - Successful operations (the FSM should work correctly)
    EXPECT_EQ(exceptions_caught.load(), 0);
    EXPECT_EQ(invalid_states.load(), 0);
    EXPECT_GT(total_operations.load(), 0);
}
