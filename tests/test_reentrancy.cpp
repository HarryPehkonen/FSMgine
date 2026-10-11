/// @file test_reentrancy.cpp
/// @brief Tests for the reentrancy guard on FSM operations
///
/// These tests pin the contract: user-supplied actions (transition actions,
/// on-enter actions, on-exit actions) must NOT call back into the machine
/// while process(), setInitialState(), or setCurrentState() is executing.
/// Doing so throws FSMReentrancyError (a std::logic_error).

#include "FSMgine/FSM.hpp"
#include "FSMgine/FSMBuilder.hpp"
#include "FSMgine/StringInterner.hpp"
#include <gtest/gtest.h>

#ifdef FSMGINE_MULTI_THREADED
#include <atomic>
#include <chrono>
#include <thread>
#endif

using namespace fsmgine;

using TestFSM = FSM<>;

class ReentrancyTest : public ::testing::Test {
protected:
    void SetUp() override { StringInterner::instance().resetArena(); }
};

// --- (a) An action that calls setCurrentState() mid-process must throw ---

TEST_F(ReentrancyTest, ActionCallingCurrentStateMidProcessThrows) {
    TestFSM fsm;
    fsm.get_builder().from("A").to("B");
    fsm.get_builder()
        .from("B")
        .action([&fsm](const auto&) {
            // This re-enters the machine while process() is running
            fsm.setCurrentState("C");
        })
        .to("C");
    fsm.get_builder().from("C").to("D");

    fsm.setInitialState("A");
    fsm.process(); // A -> B

    // The next process() should throw because the action calls setCurrentState()
    EXPECT_THROW(fsm.process(), std::logic_error);
}

// --- (b) An action that calls process() recursively must throw ---

TEST_F(ReentrancyTest, ActionCallingProcessRecursivelyThrows) {
    TestFSM fsm;
    fsm.get_builder()
        .from("A")
        .action([&fsm](const auto&) {
            // Recursive call into process()
            fsm.process();
        })
        .to("B");

    fsm.setInitialState("A");

    // process() should throw because the action tries to call process() recursively
    EXPECT_THROW(fsm.process(), std::logic_error);
}

// --- (c) An action that mutates the machine through get_builder() must throw ---

TEST_F(ReentrancyTest, ActionCallingGetBuilderThrows) {
    TestFSM fsm;
    fsm.get_builder()
        .from("A")
        .action([&fsm](const auto&) {
            // Try to mutate the machine while an action is running
            fsm.get_builder().from("B").to("C");
        })
        .to("B");

    fsm.setInitialState("A");

    // process() should throw because the action calls get_builder()
    EXPECT_THROW(fsm.process(), std::logic_error);
}

// --- Additional: onEnter action reentrancy ---

TEST_F(ReentrancyTest, OnEnterActionCallingProcessThrows) {
    TestFSM fsm;
    fsm.get_builder().onEnter("B", [&fsm](const auto&) { fsm.process(); }).from("A").to("B");

    fsm.setInitialState("A");

    // The transition A->B triggers onEnter("B"), which calls process()
    EXPECT_THROW(fsm.process(), std::logic_error);
}

// --- Additional: onExit action reentrancy ---

TEST_F(ReentrancyTest, OnExitActionCallingProcessThrows) {
    TestFSM fsm;
    fsm.get_builder().onExit("A", [&fsm](const auto&) { fsm.process(); }).from("A").to("B");

    fsm.setInitialState("A");

    // The transition A->B triggers onExit("A"), which calls process()
    EXPECT_THROW(fsm.process(), std::logic_error);
}

// --- After the exception, the machine should be usable again (guard releases) ---

TEST_F(ReentrancyTest, MachineUsableAfterReentrancyException) {
    TestFSM fsm;
    bool throw_once = true;
    fsm.get_builder()
        .from("A")
        .action([&fsm, &throw_once](const auto&) {
            if (throw_once) {
                throw_once = false;
                fsm.process(); // reentrant — should throw
            }
        })
        .to("B");
    fsm.get_builder().from("B").to("C");

    fsm.setInitialState("A");

    // First process() throws due to reentrancy
    EXPECT_THROW(fsm.process(), std::logic_error);

    // Second process() should succeed because the flag is cleared
    EXPECT_TRUE(fsm.process());
    EXPECT_EQ(fsm.getCurrentState(), "B");
}

// --- (g) An action that calls setInitialState() must throw ---

TEST_F(ReentrancyTest, ActionCallingSetInitialStateThrows) {
    TestFSM fsm;
    fsm.get_builder()
        .from("A")
        .action([&fsm](const auto&) {
            fsm.setInitialState("A"); // reentrant — should throw
        })
        .to("B");

    fsm.setInitialState("A");
    EXPECT_THROW(fsm.process(), std::logic_error);
}

// --- (h) An action that calls addOnEnterAction via builder must throw ---

TEST_F(ReentrancyTest, ActionCallingOnEnterBuilderThrows) {
    TestFSM fsm;
    fsm.get_builder()
        .from("A")
        .action([&fsm](const auto&) {
            // get_builder() is fine; onEnter() calls addOnEnterAction which guards
            fsm.get_builder().onEnter("A", [](const auto&) {}).from("A").to("B");
        })
        .to("B");

    fsm.setInitialState("A");
    EXPECT_THROW(fsm.process(), std::logic_error);
}

// --- (i) An action that calls addOnExitAction via builder must throw ---

TEST_F(ReentrancyTest, ActionCallingOnExitBuilderThrows) {
    TestFSM fsm;
    fsm.get_builder()
        .from("A")
        .action([&fsm](const auto&) {
            // get_builder() is fine; onExit() calls addOnExitAction which guards
            fsm.get_builder().onExit("A", [](const auto&) {}).from("A").to("B");
        })
        .to("B");

    fsm.setInitialState("A");
    EXPECT_THROW(fsm.process(), std::logic_error);
}

// --- (j) setInitialState guard releases after onEnter actions complete ---

TEST_F(ReentrancyTest, SetInitialStateGuardReleasesAfterOnEnter) {
    TestFSM fsm;
    bool entered = false;
    fsm.get_builder().onEnter("A", [&entered](const auto&) { entered = true; });
    fsm.get_builder().from("A").to("B");

    fsm.setInitialState("A"); // runs onEnter, guard clears
    EXPECT_TRUE(entered);

    // Should succeed — guard was released
    EXPECT_TRUE(fsm.process());
    EXPECT_EQ(fsm.getCurrentState(), "B");
}

// --- (k) setCurrentState guard releases after enter/exit actions complete ---

TEST_F(ReentrancyTest, SetCurrentStateGuardReleasesAfterActions) {
    TestFSM fsm;
    bool exited = false;
    bool entered = false;
    fsm.get_builder().onExit("A", [&exited](const auto&) { exited = true; });
    fsm.get_builder().onEnter("B", [&entered](const auto&) { entered = true; });
    fsm.get_builder().from("A").to("B");

    fsm.setInitialState("A");
    fsm.setCurrentState("B"); // runs exit/enter, guard clears
    EXPECT_TRUE(exited);
    EXPECT_TRUE(entered);

    // Should succeed — guard was released
    fsm.setCurrentState("A");
}

// --- Move-assignment deadlock stress test (finding #7a, 2026-10-10).
// Before the fix, move-assignment locked mutex_ then other.mutex_ in fixed order.
// Two threads doing opposing moves (a=move(b) vs b=move(a)) formed the classic
// lock-ordering cycle.  std::scoped_lock acquires both atomically, eliminating the
// cycle.  This stress test exercises the exact scenario under high contention.
//
// Compiled only in the MT build — the mutex code does not exist in the ST build.
#ifdef FSMGINE_MULTI_THREADED
TEST_F(ReentrancyTest, CrossMoveAssignmentDoesNotDeadlock) {
    // Two distinct machines, each with a transition so they carry real state.
    auto build = []() {
        TestFSM fsm;
        fsm.get_builder().from("A").to("B");
        fsm.get_builder().from("B").to("A");
        fsm.setInitialState("A");
        return fsm;
    };

    constexpr int kIterations = 5000;
    std::atomic<bool> done{false};
    std::atomic<bool> deadlocked{false};

    // Two threads do opposing move-assignments in a tight loop.
    auto worker = [&](TestFSM& lhs, TestFSM& rhs) {
        for (int i = 0; i < kIterations && !done.load(std::memory_order_relaxed); ++i) {
            lhs = std::move(rhs);
            // Recreate rhs so the next iteration has something to move from.
            rhs = build();
        }
    };

    TestFSM a = build();
    TestFSM b = build();

    // Watchdog: if threads deadlock, they never finish and the test times out.
    // Start it before the threads so the 10s clock runs concurrently.
    std::thread watchdog([&]() {
        using namespace std::chrono_literals;
        for (int i = 0; i < 100; ++i) {
            std::this_thread::sleep_for(100ms);
            if (done.load(std::memory_order_relaxed))
                return;
        }
        deadlocked.store(true, std::memory_order_relaxed);
    });

    std::thread t1(worker, std::ref(a), std::ref(b));
    std::thread t2(worker, std::ref(b), std::ref(a));

    t1.join();
    t2.join();
    done.store(true, std::memory_order_relaxed); // signal watchdog
    watchdog.join();

    EXPECT_FALSE(deadlocked.load()) << "move-assignment appears to have deadlocked (10s timeout)";
}
#endif