#include <gtest/gtest.h>
#include "FSMgine/FSM.hpp"
#include "FSMgine/FSMBuilder.hpp"
#include "FSMgine/StringInterner.hpp"

using namespace fsmgine;

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
    FSM fsm;
    
    // Should throw when trying to get current state without setting initial state
    EXPECT_THROW(fsm.getCurrentState(), runtime_error);
    EXPECT_THROW(fsm.step(), runtime_error);
}

TEST_F(FSMTest, SimpleTransition) {
    FSM fsm;
    
    // Build simple transition
    fsm.get_builder()
        .from("START")
        .to("END");
    
    fsm.setInitialState("START");
    EXPECT_EQ(fsm.getCurrentState(), "START");
    
    // Step should transition to END
    bool transitioned = fsm.step();
    EXPECT_TRUE(transitioned);
    EXPECT_EQ(fsm.getCurrentState(), "END");
    
    // Step again should not transition (no outgoing transitions from END)
    transitioned = fsm.step();
    EXPECT_FALSE(transitioned);
    EXPECT_EQ(fsm.getCurrentState(), "END");
}

TEST_F(FSMTest, TransitionWithPredicate) {
    FSM fsm;
    bool condition = false;
    
    fsm.get_builder()
        .from("WAITING")
        .predicate([&condition]() { return condition; })
        .to("READY");
    
    fsm.setInitialState("WAITING");
    EXPECT_EQ(fsm.getCurrentState(), "WAITING");
    
    // Step should not transition (predicate is false)
    bool transitioned = fsm.step();
    EXPECT_FALSE(transitioned);
    EXPECT_EQ(fsm.getCurrentState(), "WAITING");
    
    // Set condition to true
    condition = true;
    transitioned = fsm.step();
    EXPECT_TRUE(transitioned);
    EXPECT_EQ(fsm.getCurrentState(), "READY");
}

TEST_F(FSMTest, TransitionWithAction) {
    FSM fsm;
    
    fsm.get_builder()
        .from("START")
        .action([this]() { action_call_count++; })
        .to("END");
    
    fsm.setInitialState("START");
    EXPECT_EQ(action_call_count, 0);
    
    bool transitioned = fsm.step();
    EXPECT_TRUE(transitioned);
    EXPECT_EQ(action_call_count, 1);
    EXPECT_EQ(fsm.getCurrentState(), "END");
}

TEST_F(FSMTest, MultiplePredicatesAndActions) {
    FSM fsm;
    bool cond1 = true;
    bool cond2 = true;
    
    fsm.get_builder()
        .from("START")
        .predicate([&cond1]() { return cond1; })
        .predicate([&cond2]() { return cond2; })
        .action([this]() { action_call_count++; })
        .action([this]() { action_call_count++; })
        .to("END");
    
    fsm.setInitialState("START");
    
    // Both predicates true - should transition and execute both actions
    bool transitioned = fsm.step();
    EXPECT_TRUE(transitioned);
    EXPECT_EQ(action_call_count, 2);
    EXPECT_EQ(fsm.getCurrentState(), "END");
}

TEST_F(FSMTest, MultipleTransitionsFirstValidWins) {
    FSM fsm;
    bool cond1 = false;
    bool cond2 = true;
    
    auto builder = fsm.get_builder();
    
    // First transition (will fail)
    builder.from("START")
        .predicate([&cond1]() { return cond1; })
        .action([this]() { action_call_count = 10; })
        .to("BRANCH1");
    
    // Second transition (will succeed)
    builder.from("START")
        .predicate([&cond2]() { return cond2; })
        .action([this]() { action_call_count = 20; })
        .to("BRANCH2");
    
    fsm.setInitialState("START");
    
    bool transitioned = fsm.step();
    EXPECT_TRUE(transitioned);
    EXPECT_EQ(action_call_count, 20); // Second transition's action
    EXPECT_EQ(fsm.getCurrentState(), "BRANCH2");
}

TEST_F(FSMTest, OnEnterActions) {
    FSM fsm;
    
    fsm.get_builder()
        .onEnter("TARGET", [this]() { on_enter_called = true; })
        .from("START")
        .to("TARGET");
    
    EXPECT_FALSE(on_enter_called);
    fsm.setInitialState("START");
    EXPECT_FALSE(on_enter_called); // Not called for START
    
    fsm.step();
    EXPECT_TRUE(on_enter_called); // Called when entering TARGET
}

TEST_F(FSMTest, OnExitActions) {
    FSM fsm;
    
    fsm.get_builder()
        .onExit("START", [this]() { on_exit_called = true; })
        .from("START")
        .to("END");
    
    fsm.setInitialState("START");
    EXPECT_FALSE(on_exit_called);
    
    fsm.step();
    EXPECT_TRUE(on_exit_called); // Called when exiting START
}

TEST_F(FSMTest, SelfTransitionNoStateChangeActions) {
    FSM fsm;
    
    fsm.get_builder()
        .onEnter("LOOP", [this]() { on_enter_called = true; })
        .onExit("LOOP", [this]() { on_exit_called = true; })
        .from("LOOP")
        .action([this]() { action_call_count++; })
        .to("LOOP");
    
    fsm.setInitialState("LOOP");
    // Initial state setting should call onEnter
    EXPECT_TRUE(on_enter_called);
    on_enter_called = false; // Reset for test
    
    // Self-transition should execute action but not onExit/onEnter
    fsm.step();
    EXPECT_EQ(action_call_count, 1);
    EXPECT_FALSE(on_exit_called);
    EXPECT_FALSE(on_enter_called);
}

TEST_F(FSMTest, SetCurrentStateExecutesActions) {
    FSM fsm;
    
    fsm.get_builder()
        .onEnter("STATE1", [this]() { action_call_count = 1; })
        .onExit("STATE1", [this]() { action_call_count = 2; })
        .onEnter("STATE2", [this]() { action_call_count = 3; })
        .from("STATE1")
        .to("STATE2");
    
    fsm.setInitialState("STATE1");
    EXPECT_EQ(action_call_count, 1); // onEnter STATE1
    
    fsm.setCurrentState("STATE2");
    EXPECT_EQ(action_call_count, 3); // onExit STATE1 (=2), then onEnter STATE2 (=3)
}

TEST_F(FSMTest, ErrorHandling) {
    FSM fsm;
    
    // Cannot set initial state to undefined state
    EXPECT_THROW(fsm.setInitialState("UNDEFINED"), invalid_argument);
    
    // Cannot set current state to undefined state
    EXPECT_THROW(fsm.setCurrentState("UNDEFINED"), invalid_argument);
    
    // After defining a state, should be able to set it
    fsm.get_builder().from("DEFINED").to("ANOTHER");
    EXPECT_NO_THROW(fsm.setInitialState("DEFINED"));
}

TEST_F(FSMTest, FluentBuilderInterface) {
    FSM fsm;
    
    // Test fluent interface chaining
    fsm.get_builder()
        .onEnter("START", [this]() { action_call_count++; })
        .onExit("START", [this]() { action_call_count++; })
        .from("START")
        .predicate([]() { return true; })
        .action([this]() { action_call_count++; })
        .to("END");
    
    fsm.setInitialState("START");
    EXPECT_EQ(action_call_count, 1); // onEnter START
    
    fsm.step();
    EXPECT_EQ(action_call_count, 3); // onExit START + transition action
}

TEST_F(FSMTest, MoveSemantics) {
    FSM fsm1;
    fsm1.get_builder().from("A").to("B");
    fsm1.setInitialState("A");
    
    // Move constructor
    FSM fsm2 = std::move(fsm1);
    EXPECT_EQ(fsm2.getCurrentState(), "A");
    
    // Move assignment
    FSM fsm3;
    fsm3 = std::move(fsm2);
    EXPECT_EQ(fsm3.getCurrentState(), "A");
    
    // Original FSM should be in valid but unspecified state
    // We don't test the moved-from state as it's implementation-defined
}