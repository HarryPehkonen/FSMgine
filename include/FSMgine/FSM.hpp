#pragma once

#include <string_view>
#include <string>
#include <unordered_map>
#include <vector>
#include <stdexcept>
#include <variant> // For std::monostate
#include "FSMgine/Transition.hpp"
#include "FSMgine/StringInterner.hpp"

#ifdef FSMGINE_MULTI_THREADED
#include <shared_mutex>
#endif

namespace fsmgine {

// Forward declaration
template<typename TEvent>
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

template<typename TEvent = std::monostate>
class FSM {
public:
    using Predicate = std::function<bool(const TEvent&)>;
    using Action = std::function<void(const TEvent&)>;

private:
    // Internal state data structure
    struct StateData {
        std::vector<Action> on_enter_actions;
        std::vector<Action> on_exit_actions;
        std::vector<Transition<TEvent>> transitions;
        
        StateData() = default;
        StateData(const StateData&) = delete;
        StateData& operator=(const StateData&) = delete;
        StateData(StateData&&) = default;
        StateData& operator=(StateData&&) = default;
    };

public:
    FSM() = default;
    
    // Move-only semantics
    FSM(const FSM&) = delete;
    FSM& operator=(const FSM&) = delete;
    FSM(FSM&&) = default;
    FSM& operator=(FSM&&) = default;
    
    FSMBuilder<TEvent> get_builder();
    
    void setInitialState(std::string_view state);
    void setCurrentState(std::string_view state);
    
    bool process(const TEvent& event);
    
    // Convenience for event-less FSMs to preserve `step()`
    bool step() {
        static_assert(std::is_same_v<TEvent, std::monostate>, "step() can only be used with event-less FSMs (FSM<> or FSM<std::monostate>).");
        return process(std::monostate{});
    }

    std::string_view getCurrentState() const;
    
    // Internal methods for builder
    void addTransition(std::string_view from_state, Transition<TEvent> transition);
    void addOnEnterAction(std::string_view state, Action action);
    void addOnExitAction(std::string_view state, Action action);

private:
    std::unordered_map<std::string_view, StateData> states_;
    std::string_view current_state_;
    bool has_initial_state_ = false;
    
#ifdef FSMGINE_MULTI_THREADED
    mutable std::shared_mutex mutex_;
#endif

    // Helper methods
    StateData& getOrCreateState(std::string_view state);
    void executeOnExitActions(std::string_view state, const TEvent& event) const;
    void executeOnEnterActions(std::string_view state, const TEvent& event) const;
};

// --- Implementation ---

template<typename TEvent>
FSMBuilder<TEvent> FSM<TEvent>::get_builder() {
    return FSMBuilder<TEvent>(*this);
}

template<typename TEvent>
void FSM<TEvent>::setInitialState(std::string_view state) {
#ifdef FSMGINE_MULTI_THREADED
    std::unique_lock<std::shared_mutex> lock(mutex_);
#endif
    
    auto interned_state = StringInterner::instance().intern(state);
    
    if (states_.find(interned_state) == states_.end()) {
        throw invalid_argument("Cannot set initial state to undefined state: " + std::string(state));
    }
    
    current_state_ = interned_state;
    has_initial_state_ = true;
    
    // For initial state, we don't have a causing event.
    // We pass a default-constructed event.
    executeOnEnterActions(current_state_, TEvent{});
}

template<typename TEvent>
void FSM<TEvent>::setCurrentState(std::string_view state) {
#ifdef FSMGINE_MULTI_THREADED
    std::unique_lock<std::shared_mutex> lock(mutex_);
#endif
    
    auto interned_state = StringInterner::instance().intern(state);
    
    if (states_.find(interned_state) == states_.end()) {
        throw invalid_argument("Cannot set current state to undefined state: " + std::string(state));
    }
    
    TEvent dummy_event{}; // Dummy event for state change actions
    if (has_initial_state_ && current_state_ != interned_state) {
        executeOnExitActions(current_state_, dummy_event);
    }
    
    current_state_ = interned_state;
    has_initial_state_ = true;
    
    executeOnEnterActions(current_state_, dummy_event);
}

template<typename TEvent>
bool FSM<TEvent>::process(const TEvent& event) {
#ifdef FSMGINE_MULTI_THREADED
    std::shared_lock<std::shared_mutex> lock(mutex_);
#endif
    
    if (!has_initial_state_) {
        throw runtime_error("Cannot process event in FSM without setting initial state");
    }
    
    auto it = states_.find(current_state_);
    if (it == states_.end()) {
        throw runtime_error("Current state not found in FSM");
    }
    
    const auto& state_data = it->second;
    
    for (const auto& transition : state_data.transitions) {
        if (transition.evaluatePredicates(event)) {
            auto target_state = transition.getTargetState();
            
            if (target_state.empty()) {
                throw runtime_error("Transition has no target state");
            }
            if (states_.find(target_state) == states_.end()) {
                throw runtime_error("Target state not found: " + std::string(target_state));
            }
            
            transition.executeActions(event);
            
            if (current_state_ != target_state) {
                executeOnExitActions(current_state_, event);
#ifdef FSMGINE_MULTI_THREADED
                lock.unlock();
                std::unique_lock<std::shared_mutex> unique_lock(mutex_);
#endif
                current_state_ = target_state;
                executeOnEnterActions(current_state_, event);
            }
            
            return true;
        }
    }
    
    return false;
}

template<typename TEvent>
std::string_view FSM<TEvent>::getCurrentState() const {
#ifdef FSMGINE_MULTI_THREADED
    std::shared_lock<std::shared_mutex> lock(mutex_);
#endif
    
    if (!has_initial_state_) {
        throw runtime_error("FSM has no current state - initial state not set");
    }
    
    return current_state_;
}

template<typename TEvent>
void FSM<TEvent>::addTransition(std::string_view from_state, Transition<TEvent> transition) {
#ifdef FSMGINE_MULTI_THREADED
    std::unique_lock<std::shared_mutex> lock(mutex_);
#endif
    
    auto interned_from_state = StringInterner::instance().intern(from_state);
    auto& state_data = getOrCreateState(interned_from_state);
    
    auto target_state = transition.getTargetState();
    if (!target_state.empty()) {
        auto interned_target_state = StringInterner::instance().intern(target_state);
        getOrCreateState(interned_target_state);
    }
    
    state_data.transitions.push_back(std::move(transition));
}

template<typename TEvent>
void FSM<TEvent>::addOnEnterAction(std::string_view state, Action action) {
#ifdef FSMGINE_MULTI_THREADED
    std::unique_lock<std::shared_mutex> lock(mutex_);
#endif
    
    auto interned_state = StringInterner::instance().intern(state);
    auto& state_data = getOrCreateState(interned_state);
    
    if (action) {
        state_data.on_enter_actions.push_back(std::move(action));
    }
}

template<typename TEvent>
void FSM<TEvent>::addOnExitAction(std::string_view state, Action action) {
#ifdef FSMGINE_MULTI_THREADED
    std::unique_lock<std::shared_mutex> lock(mutex_);
#endif
    
    auto interned_state = StringInterner::instance().intern(state);
    auto& state_data = getOrCreateState(interned_state);
    
    if (action) {
        state_data.on_exit_actions.push_back(std::move(action));
    }
}

template<typename TEvent>
typename FSM<TEvent>::StateData& FSM<TEvent>::getOrCreateState(std::string_view state) {
    auto it = states_.find(state);
    if (it == states_.end()) {
        auto [inserted_it, success] = states_.emplace(state, StateData{});
        return inserted_it->second;
    }
    return it->second;
}

template<typename TEvent>
void FSM<TEvent>::executeOnExitActions(std::string_view state, const TEvent& event) const {
    auto it = states_.find(state);
    if (it != states_.end()) {
        for (const auto& action : it->second.on_exit_actions) {
            action(event);
        }
    }
}

template<typename TEvent>
void FSM<TEvent>::executeOnEnterActions(std::string_view state, const TEvent& event) const {
    auto it = states_.find(state);
    if (it != states_.end()) {
        for (const auto& action : it->second.on_enter_actions) {
            action(event);
        }
    }
}

} // namespace fsmgine
