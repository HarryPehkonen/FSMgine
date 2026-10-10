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