#include <gtest/gtest.h>
#include "FSMgine/Transition.hpp"

using namespace fsmgine;

// Use a simple int as the event type for testing
using TestTransition = Transition<int>;

class TransitionTest : public ::testing::Test {
protected:
    bool action_called = false;
    int action_call_count = 0;
    
    void SetUp() override {
        action_called = false;
        action_call_count = 0;
    }
};

TEST_F(TransitionTest, DefaultConstructor) {
    TestTransition transition;
    
    EXPECT_FALSE(transition.hasPredicates());
    EXPECT_FALSE(transition.hasActions());
    EXPECT_FALSE(transition.hasTargetState());
    EXPECT_TRUE(transition.evaluatePredicates(0)); // Event value doesn't matter
}

TEST_F(TransitionTest, SetTargetState) {
    TestTransition transition;
    transition.setTargetState("target_state");
    
    EXPECT_TRUE(transition.hasTargetState());
    EXPECT_EQ(transition.getTargetState(), "target_state");
}

TEST_F(TransitionTest, AddSinglePredicate) {
    TestTransition transition;
    bool predicate_called = false;
    
    transition.addPredicate([&](const int& e) { 
        EXPECT_EQ(e, 42);
        predicate_called = true; 
        return true; 
    });
    
    EXPECT_TRUE(transition.hasPredicates());
    EXPECT_TRUE(transition.evaluatePredicates(42));
    EXPECT_TRUE(predicate_called);
}

TEST_F(TransitionTest, PredicateReturnsFalse) {
    TestTransition transition;
    
    transition.addPredicate([&](const int& e) { return e > 10; });
    
    EXPECT_FALSE(transition.evaluatePredicates(5));
    EXPECT_TRUE(transition.evaluatePredicates(15));
}

TEST_F(TransitionTest, MultiplePredicatesAllTrue) {
    TestTransition transition;
    
    transition.addPredicate([](const int& e) { return e > 10; });
    transition.addPredicate([](const int& e) { return e < 20; });
    
    EXPECT_FALSE(transition.evaluatePredicates(5));
    EXPECT_TRUE(transition.evaluatePredicates(15));
    EXPECT_FALSE(transition.evaluatePredicates(25));
}

TEST_F(TransitionTest, AddSingleAction) {
    TestTransition transition;
    
    transition.addAction([this](const int& e) { 
        EXPECT_EQ(e, 99);
        action_called = true; 
    });
    
    EXPECT_TRUE(transition.hasActions());
    transition.executeActions(99);
    EXPECT_TRUE(action_called);
}

TEST_F(TransitionTest, MultipleActions) {
    TestTransition transition;
    
    transition.addAction([this](const int& e) { action_call_count += e; });
    transition.addAction([this](const int& e) { action_call_count += e; });
    
    transition.executeActions(10);
    EXPECT_EQ(action_call_count, 20);
}

TEST_F(TransitionTest, NullPredicateNotAdded) {
    TestTransition transition;
    transition.addPredicate(nullptr);
    EXPECT_FALSE(transition.hasPredicates());
}

TEST_F(TransitionTest, NullActionNotAdded) {
    TestTransition transition;
    transition.addAction(nullptr);
    EXPECT_FALSE(transition.hasActions());
}

TEST_F(TransitionTest, MoveSemantics) {
    TestTransition transition1;
    transition1.addPredicate([](const int&) { return true; });
    transition1.addAction([this](const int&) { action_called = true; });
    transition1.setTargetState("moved_state");
    
    TestTransition transition2 = std::move(transition1);
    
    EXPECT_TRUE(transition2.hasPredicates());
    EXPECT_TRUE(transition2.hasActions());
    EXPECT_EQ(transition2.getTargetState(), "moved_state");
    
    transition2.executeActions(0);
    EXPECT_TRUE(action_called);
}
