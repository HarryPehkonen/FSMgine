#include "FSMgine/FSM.hpp"
#include "FSMgine/FSMBuilder.hpp"

namespace fsmgine {

FSMBuilder FSM::get_builder() {
    return FSMBuilder(*this);
}

void FSM::setInitialState(std::string_view state) {
#ifdef FSMGINE_MULTI_THREADED
    std::unique_lock<std::shared_mutex> lock(mutex_);
#endif
    
    auto interned_state = StringInterner::instance().intern(state);
    
    // Validate that the state exists (has been referenced somewhere)
    auto it = states_.find(interned_state);
    if (it == states_.end()) {
        throw invalid_argument("Cannot set initial state to undefined state: " + std::string(state));
    }
    
    current_state_ = interned_state;
    has_initial_state_ = true;
    
    // Execute on-enter actions for initial state
    executeOnEnterActions(current_state_);
}

void FSM::setCurrentState(std::string_view state) {
#ifdef FSMGINE_MULTI_THREADED
    std::unique_lock<std::shared_mutex> lock(mutex_);
#endif
    
    auto interned_state = StringInterner::instance().intern(state);
    
    // Validate that the state exists
    auto it = states_.find(interned_state);
    if (it == states_.end()) {
        throw invalid_argument("Cannot set current state to undefined state: " + std::string(state));
    }
    
    if (has_initial_state_ && current_state_ != interned_state) {
        executeOnExitActions(current_state_);
    }
    
    current_state_ = interned_state;
    has_initial_state_ = true;
    
    executeOnEnterActions(current_state_);
}

bool FSM::step() {
#ifdef FSMGINE_MULTI_THREADED
    std::shared_lock<std::shared_mutex> lock(mutex_);
#endif
    
    if (!has_initial_state_) {
        throw runtime_error("Cannot step FSM without setting initial state");
    }
    
    auto it = states_.find(current_state_);
    if (it == states_.end()) {
        throw runtime_error("Current state not found in FSM");
    }
    
    const auto& state_data = it->second;
    
    // Find first valid transition
    for (const auto& transition : state_data.transitions) {
        if (transition.evaluatePredicates()) {
            auto target_state = transition.getTargetState();
            
            if (target_state.empty()) {
                throw runtime_error("Transition has no target state");
            }
            
            // Validate target state exists
            auto target_it = states_.find(target_state);
            if (target_it == states_.end()) {
                throw runtime_error("Target state not found: " + std::string(target_state));
            }
            
            // Execute transition actions
            transition.executeActions();
            
            // Only execute state change actions if we're actually changing states
            if (current_state_ != target_state) {
                executeOnExitActions(current_state_);
#ifdef FSMGINE_MULTI_THREADED
                // Upgrade to unique lock for state change
                lock.unlock();
                std::unique_lock<std::shared_mutex> unique_lock(mutex_);
#endif
                current_state_ = target_state;
                executeOnEnterActions(current_state_);
            }
            
            return true;
        }
    }
    
    return false; // No transition occurred
}

std::string_view FSM::getCurrentState() const {
#ifdef FSMGINE_MULTI_THREADED
    std::shared_lock<std::shared_mutex> lock(mutex_);
#endif
    
    if (!has_initial_state_) {
        throw runtime_error("FSM has no current state - initial state not set");
    }
    
    return current_state_;
}

void FSM::addTransition(std::string_view from_state, Transition transition) {
#ifdef FSMGINE_MULTI_THREADED
    std::unique_lock<std::shared_mutex> lock(mutex_);
#endif
    
    auto interned_from_state = StringInterner::instance().intern(from_state);
    auto& state_data = getOrCreateState(interned_from_state);
    
    // Ensure target state is also created (for validation purposes)
    auto target_state = transition.getTargetState();
    if (!target_state.empty()) {
        auto interned_target_state = StringInterner::instance().intern(target_state);
        getOrCreateState(interned_target_state);
    }
    
    state_data.transitions.push_back(std::move(transition));
}

void FSM::addOnEnterAction(std::string_view state, Transition::Action action) {
#ifdef FSMGINE_MULTI_THREADED
    std::unique_lock<std::shared_mutex> lock(mutex_);
#endif
    
    auto interned_state = StringInterner::instance().intern(state);
    auto& state_data = getOrCreateState(interned_state);
    
    if (action) {
        state_data.on_enter_actions.push_back(std::move(action));
    }
}

void FSM::addOnExitAction(std::string_view state, Transition::Action action) {
#ifdef FSMGINE_MULTI_THREADED
    std::unique_lock<std::shared_mutex> lock(mutex_);
#endif
    
    auto interned_state = StringInterner::instance().intern(state);
    auto& state_data = getOrCreateState(interned_state);
    
    if (action) {
        state_data.on_exit_actions.push_back(std::move(action));
    }
}

StateData& FSM::getOrCreateState(std::string_view state) {
    auto it = states_.find(state);
    if (it == states_.end()) {
        auto [inserted_it, success] = states_.emplace(state, StateData{});
        return inserted_it->second;
    }
    return it->second;
}

void FSM::executeOnExitActions(std::string_view state) const {
    auto it = states_.find(state);
    if (it != states_.end()) {
        for (const auto& action : it->second.on_exit_actions) {
            action();
        }
    }
}

void FSM::executeOnEnterActions(std::string_view state) const {
    auto it = states_.find(state);
    if (it != states_.end()) {
        for (const auto& action : it->second.on_enter_actions) {
            action();
        }
    }
}

} // namespace fsmgine