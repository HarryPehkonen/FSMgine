#pragma once

#include <string>
#include <string_view>
#include "FSMgine/Transition.hpp"

namespace fsmgine {

// Forward declaration
class FSM;

class TransitionBuilder {
public:
    explicit TransitionBuilder(FSM& fsm, std::string_view from_state);
    
    // Non-copyable, movable
    TransitionBuilder(const TransitionBuilder&) = delete;
    TransitionBuilder& operator=(const TransitionBuilder&) = delete;
    TransitionBuilder(TransitionBuilder&&) = default;
    TransitionBuilder& operator=(TransitionBuilder&&) = default;
    
    TransitionBuilder& predicate(Transition::Predicate pred);
    TransitionBuilder& action(Transition::Action action);
    void to(const std::string& state); // Terminal method
    
private:
    FSM& fsm_;
    std::string_view from_state_;
    Transition transition_;
};

class FSMBuilder {
public:
    explicit FSMBuilder(FSM& fsm);
    
    // Non-copyable, movable
    FSMBuilder(const FSMBuilder&) = delete;
    FSMBuilder& operator=(const FSMBuilder&) = delete;
    FSMBuilder(FSMBuilder&&) = default;
    FSMBuilder& operator=(FSMBuilder&&) = default;
    
    TransitionBuilder from(const std::string& state);
    FSMBuilder& onEnter(const std::string& state, Transition::Action action);
    FSMBuilder& onExit(const std::string& state, Transition::Action action);
    
private:
    FSM& fsm_;
};

} // namespace fsmgine