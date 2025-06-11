#include <iostream>
#include <cassert>
#include <variant>
#include "FSMgine/StringInterner.hpp"
#include "FSMgine/Transition.hpp"

using namespace fsmgine;

void test_string_interner() {
    std::cout << "Testing StringInterner..." << std::endl;
    
    auto& interner = StringInterner::instance();
    interner.clear();
    
    // Test basic interning
    std::string str = "test_state";
    auto view1 = interner.intern(str);
    auto view2 = interner.intern(str);
    
    assert(view1 == view2);
    assert(view1.data() == view2.data());
    
    // Test string_view interning
    std::string_view sv = "test_state2";
    auto view3 = interner.intern(sv);
    auto view4 = interner.intern(sv);
    
    assert(view3 == view4);
    assert(view3.data() == view4.data());
    
    std::cout << "✓ StringInterner tests passed" << std::endl;
}

void test_transition() {
    std::cout << "Testing Transition..." << std::endl;
    
    Transition<int> transition;
    
    // Test default state
    assert(!transition.hasPredicates());
    assert(!transition.hasActions());
    assert(!transition.hasTargetState());
    assert(transition.evaluatePredicates(0)); // No predicates = always true
    
    // Test predicates
    bool predicate_called = false;
    transition.addPredicate([&predicate_called](const int& e) { 
        assert(e == 42);
        predicate_called = true; 
        return true; 
    });
    
    assert(transition.hasPredicates());
    assert(transition.evaluatePredicates(42));
    assert(predicate_called);
    
    // Test actions
    bool action_called = false;
    transition.addAction([&action_called](const int& e) { 
        assert(e == 100);
        action_called = true; 
    });
    
    assert(transition.hasActions());
    transition.executeActions(100);
    assert(action_called);
    
    // Test target state
    transition.setTargetState("next_state");
    assert(transition.hasTargetState());
    assert(transition.getTargetState() == "next_state");
    
    std::cout << "✓ Transition tests passed" << std::endl;
}

int main() {
    std::cout << "Running FSMgine Tests" << std::endl;
    std::cout << "===================" << std::endl;
    
    try {
        test_string_interner();
        test_transition();
        
        std::cout << std::endl << "🎉 All tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cout << "❌ Test failed: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}
