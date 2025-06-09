#include <gtest/gtest.h>
#include "FSMgine/Transition.hpp"

using namespace fsmgine;

class TransitionTest : public ::testing::Test {
protected:
    bool predicate_called = false;
    bool action_called = false;
    int action_call_count = 0;
    
    void SetUp() override {
        predicate_called = false;
        action_called = false;
        action_call_count = 0;
    }
};

TEST_F(TransitionTest, DefaultConstructor) {
    Transition transition;
    
    EXPECT_FALSE(transition.hasPredicates());
    EXPECT_FALSE(transition.hasActions());
    EXPECT_FALSE(transition.hasTargetState());
    EXPECT_TRUE(transition.evaluatePredicates()); // No predicates = always true
}

TEST_F(TransitionTest, SetTargetState) {
    Transition transition;
    transition.setTargetState("target_state");
    
    EXPECT_TRUE(transition.hasTargetState());
    EXPECT_EQ(transition.getTargetState(), "target_state");
}

TEST_F(TransitionTest, AddSinglePredicate) {
    Transition transition;
    
    transition.addPredicate([this]() { 
        predicate_called = true; 
        return true; 
    });
    
    EXPECT_TRUE(transition.hasPredicates());
    EXPECT_TRUE(transition.evaluatePredicates());
    EXPECT_TRUE(predicate_called);
}

TEST_F(TransitionTest, PredicateReturnsFalse) {
    Transition transition;
    
    transition.addPredicate([this]() { 
        predicate_called = true; 
        return false; 
    });
    
    EXPECT_TRUE(transition.hasPredicates());
    EXPECT_FALSE(transition.evaluatePredicates());
    EXPECT_TRUE(predicate_called);
}

TEST_F(TransitionTest, MultiplePredicatesAllTrue) {
    Transition transition;
    bool pred1_called = false;
    bool pred2_called = false;
    
    transition.addPredicate([&pred1_called]() { 
        pred1_called = true; 
        return true; 
    });
    transition.addPredicate([&pred2_called]() { 
        pred2_called = true; 
        return true; 
    });
    
    EXPECT_TRUE(transition.evaluatePredicates());
    EXPECT_TRUE(pred1_called);
    EXPECT_TRUE(pred2_called);
}

TEST_F(TransitionTest, MultiplePredicatesOneFalse) {
    Transition transition;
    bool pred1_called = false;
    bool pred2_called = false;
    
    transition.addPredicate([&pred1_called]() { 
        pred1_called = true; 
        return true; 
    });
    transition.addPredicate([&pred2_called]() { 
        pred2_called = true; 
        return false; 
    });
    
    EXPECT_FALSE(transition.evaluatePredicates());
    EXPECT_TRUE(pred1_called);
    EXPECT_TRUE(pred2_called);
}

TEST_F(TransitionTest, AddSingleAction) {
    Transition transition;
    
    transition.addAction([this]() { 
        action_called = true; 
    });
    
    EXPECT_TRUE(transition.hasActions());
    transition.executeActions();
    EXPECT_TRUE(action_called);
}

TEST_F(TransitionTest, MultipleActions) {
    Transition transition;
    
    transition.addAction([this]() { action_call_count++; });
    transition.addAction([this]() { action_call_count++; });
    
    EXPECT_TRUE(transition.hasActions());
    transition.executeActions();
    EXPECT_EQ(action_call_count, 2);
}

TEST_F(TransitionTest, CompleteTransition) {
    Transition transition;
    
    transition.addPredicate([]() { return true; });
    transition.addAction([this]() { action_called = true; });
    transition.setTargetState("next_state");
    
    EXPECT_TRUE(transition.hasPredicates());
    EXPECT_TRUE(transition.hasActions());
    EXPECT_TRUE(transition.hasTargetState());
    
    EXPECT_TRUE(transition.evaluatePredicates());
    transition.executeActions();
    EXPECT_TRUE(action_called);
    EXPECT_EQ(transition.getTargetState(), "next_state");
}

TEST_F(TransitionTest, NullPredicateNotAdded) {
    Transition transition;
    
    transition.addPredicate(nullptr);
    
    EXPECT_FALSE(transition.hasPredicates());
    EXPECT_TRUE(transition.evaluatePredicates()); // Still true with no predicates
}

TEST_F(TransitionTest, NullActionNotAdded) {
    Transition transition;
    
    transition.addAction(nullptr);
    
    EXPECT_FALSE(transition.hasActions());
    // executeActions should not crash
    transition.executeActions();
}

TEST_F(TransitionTest, MoveSemantics) {
    Transition transition1;
    transition1.addPredicate([]() { return true; });
    transition1.addAction([this]() { action_called = true; });
    transition1.setTargetState("moved_state");
    
    // Move constructor
    Transition transition2 = std::move(transition1);
    
    EXPECT_TRUE(transition2.hasPredicates());
    EXPECT_TRUE(transition2.hasActions());
    EXPECT_TRUE(transition2.hasTargetState());
    EXPECT_EQ(transition2.getTargetState(), "moved_state");
    
    // Test that moved transition works
    EXPECT_TRUE(transition2.evaluatePredicates());
    transition2.executeActions();
    EXPECT_TRUE(action_called);
}