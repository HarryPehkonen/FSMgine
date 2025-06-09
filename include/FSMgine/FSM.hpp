#pragma once

#include <string_view>
#include <string>
#include <unordered_map>
#include <vector>
#include <stdexcept>
#include "FSMgine/Transition.hpp"
#include "FSMgine/StringInterner.hpp"

#ifdef FSMGINE_MULTI_THREADED
#include <shared_mutex>
#endif

namespace fsmgine {

// Forward declaration
class FSMBuilder;

// Custom exceptions
class invalid_argument : public std::invalid_argument {
public:
    using std::invalid_argument::invalid_argument;
};

class runtime_error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Internal state data structure
struct StateData {
    std::vector<Transition::Action> on_enter_actions;
    std::vector<Transition::Action> on_exit_actions;
    std::vector<Transition> transitions;
    
    StateData() = default;
    StateData(const StateData&) = delete;
    StateData& operator=(const StateData&) = delete;
    StateData(StateData&&) = default;
    StateData& operator=(StateData&&) = default;
};

class FSM {
public:
    FSM() = default;
    
    // Move-only semantics
    FSM(const FSM&) = delete;
    FSM& operator=(const FSM&) = delete;
    FSM(FSM&&) = default;
    FSM& operator=(FSM&&) = default;
    
    FSMBuilder get_builder();
    
    void setInitialState(std::string_view state);
    void setCurrentState(std::string_view state);
    
    bool step(); // Returns true if a transition occurred
    std::string_view getCurrentState() const;
    
    // Internal methods for builder
    void addTransition(std::string_view from_state, Transition transition);
    void addOnEnterAction(std::string_view state, Transition::Action action);
    void addOnExitAction(std::string_view state, Transition::Action action);

private:
    std::unordered_map<std::string_view, StateData> states_;
    std::string_view current_state_;
    bool has_initial_state_ = false;
    
#ifdef FSMGINE_MULTI_THREADED
    mutable std::shared_mutex mutex_;
#endif

    // Helper methods
    StateData& getOrCreateState(std::string_view state);
    void executeOnExitActions(std::string_view state) const;
    void executeOnEnterActions(std::string_view state) const;
};

} // namespace fsmgine