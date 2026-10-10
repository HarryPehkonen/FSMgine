// Regression test: a non-default-constructible event type must compile
// with setInitialState() and setCurrentState().
//
// Before the fix, FSM.hpp used `static const TEvent dummy_event{};` inside
// setInitialState/setCurrentState, which requires TEvent to be
// default-constructible. This test exercises the compile-time constraint.

#include "FSMgine/FSM.hpp"
#include "FSMgine/FSMBuilder.hpp"
#include "FSMgine/StringInterner.hpp"
#include <gtest/gtest.h>
#include <string>

// An event type that deliberately has no default constructor.
struct NonDefaultEvent {
    std::string type;

    NonDefaultEvent() = delete;
    explicit NonDefaultEvent(std::string t) : type(std::move(t)) {}
};

using NonDefaultFSM = fsmgine::FSM<NonDefaultEvent>;

class NonDefaultEventTest : public ::testing::Test {
protected:
    void SetUp() override { fsmgine::StringInterner::instance().resetArena(); }
};

TEST_F(NonDefaultEventTest, SetInitialStateCompiles) {
    NonDefaultFSM fsm;
    fsm.get_builder()
        .from("Idle")
        .predicate([](const NonDefaultEvent& e) { return e.type == "go"; })
        .to("Running");

    // This must compile even though NonDefaultEvent has no default ctor.
    fsm.setInitialState("Idle");
    EXPECT_EQ(fsm.getCurrentState(), "Idle");
}

TEST_F(NonDefaultEventTest, SetCurrentStateCompiles) {
    NonDefaultFSM fsm;
    fsm.get_builder().from("A").to("B");
    fsm.get_builder().from("B").to("A");

    fsm.setInitialState("A");
    // This must also compile.
    fsm.setCurrentState("B");
    EXPECT_EQ(fsm.getCurrentState(), "B");
}

TEST_F(NonDefaultEventTest, ProcessWithRealEvent) {
    NonDefaultFSM fsm;
    fsm.get_builder()
        .from("Idle")
        .predicate([](const NonDefaultEvent& e) { return e.type == "start"; })
        .to("Running");

    fsm.setInitialState("Idle");
    bool transitioned = fsm.process(NonDefaultEvent{"start"});
    EXPECT_TRUE(transitioned);
    EXPECT_EQ(fsm.getCurrentState(), "Running");
}