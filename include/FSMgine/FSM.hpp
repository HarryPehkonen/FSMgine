/// @file FSM.hpp
/// @brief Core finite state machine implementation
/// @ingroup core

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
#include <mutex>
#endif

/// @defgroup core Core FSM Components
/// @brief Core components of the FSMgine library

namespace fsmgine {

// Forward declaration
template<typename TEvent>
class FSMBuilder;

template<typename TEvent>
class TransitionBuilder;

/// @brief Exception thrown when attempting to access a state that doesn't exist
/// @ingroup core
class FSMStateNotFoundError : public std::runtime_error {
public:
    /// @brief Constructs an exception for a missing state
    /// @param state The name of the state that was not found
    explicit FSMStateNotFoundError(const std::string& state) 
        : std::runtime_error("FSM state not found: " + state) {}
};

/// @brief Exception thrown when FSM operations are attempted before initialization
/// @ingroup core
class FSMNotInitializedError : public std::runtime_error {
public:
    /// @brief Constructs an exception for uninitialized FSM
    FSMNotInitializedError() 
        : std::runtime_error("FSM has not been initialized with a state") {}
};

/// @brief Exception thrown for invalid state operations
/// @ingroup core
class FSMInvalidStateError : public std::invalid_argument {
public:
    /// @brief Constructs an exception for invalid state operations
    /// @param message Detailed error message
    explicit FSMInvalidStateError(const std::string& message)
        : std::invalid_argument(message) {}
};

/// @brief A high-performance finite state machine implementation
/// @tparam TEvent The event type used for transitions (defaults to std::monostate for event-less FSMs)
/// @ingroup core
/// 
/// @details The FSM class provides a flexible and efficient state machine implementation
/// with the following features:
/// - Type-safe state and event handling
/// - Support for guards (predicates) and actions on transitions
/// - On-enter and on-exit actions for states
/// - String interning for optimized state name storage
/// - Optional thread-safety with FSMGINE_MULTI_THREADED
/// 
/// @par Example
/// @code{.cpp}
/// // Define an event type
/// struct Event { std::string type; };
/// 
/// // Create an FSM
/// FSM<Event> machine;
/// machine.get_builder()
///     .from("Idle").to("Working").when([](const Event& e) { return e.type == "start"; })
///     .from("Working").to("Idle").when([](const Event& e) { return e.type == "stop"; })
///     .build("Idle");
/// 
/// // Process events
/// machine.process(Event{"start"});  // Transitions to "Working"
/// @endcode
template<typename TEvent = std::monostate>
class FSM {
public:
    /// @brief Type alias for transition predicates
    /// @details Functions that evaluate whether a transition should occur based on an event
    using Predicate = std::function<bool(const TEvent&)>;
    
    /// @brief Type alias for transition actions
    /// @details Functions executed during transitions or state changes
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
    /// @brief Default constructor
    FSM() = default;
    
    // Copy operations are deleted
    FSM(const FSM&) = delete;
    FSM& operator=(const FSM&) = delete;

    /// @brief Move constructor
    /// @param other FSM to move from
    FSM(FSM&& other) noexcept {
#ifdef FSMGINE_MULTI_THREADED
        std::unique_lock<std::mutex> lock(other.mutex_);
#endif
        states_ = std::move(other.states_);
        current_state_ = other.current_state_;
        has_initial_state_ = other.has_initial_state_;
    }

    /// @brief Move assignment operator
    /// @param other FSM to move from
    /// @return Reference to this FSM
    FSM& operator=(FSM&& other) noexcept {
        if (this != &other) {
#ifdef FSMGINE_MULTI_THREADED
            std::unique_lock<std::mutex> lock(mutex_);
            std::unique_lock<std::mutex> other_lock(other.mutex_);
#endif
            states_ = std::move(other.states_);
            current_state_ = other.current_state_;
            has_initial_state_ = other.has_initial_state_;
        }
        return *this;
    }
    
    /// @brief Creates a builder for fluent FSM construction
    /// @return A new FSMBuilder instance for this FSM
    /// @par Example
    /// @code{.cpp}
    /// fsm.get_builder()
    ///    .from("A").to("B").when([](const auto& e) { return true; })
    ///    .build("A");
    /// @endcode
    FSMBuilder<TEvent> get_builder();
    
    /// @brief Sets the initial state of the FSM
    /// @param state The name of the initial state
    /// @throws FSMInvalidStateError if the state doesn't exist
    /// @note This also executes any on-enter actions for the initial state
    void setInitialState(std::string_view state);
    
    /// @brief Changes the current state of the FSM
    /// @param state The name of the state to transition to
    /// @throws FSMInvalidStateError if the state doesn't exist
    /// @note This executes on-exit actions for the current state and on-enter actions for the new state
    void setCurrentState(std::string_view state);
    
    /// @brief Gets the name of the current state
    /// @return The current state name
    /// @throws FSMNotInitializedError if no initial state has been set
    std::string_view getCurrentState() const;
    
    /// @brief Processes an event and potentially transitions to a new state
    /// @param event The event to process
    /// @return true if a transition occurred, false otherwise
    /// @throws FSMNotInitializedError if no initial state has been set
    /// @throws FSMStateNotFoundError if the current state is invalid
    /// @throws FSMInvalidStateError if a transition has no target state
    bool process(const TEvent& event);
    
    /// @brief Processes a transition for event-less FSMs
    /// @return true if a transition occurred, false otherwise
    /// @note This method is only available for FSM<> or FSM<std::monostate>
    bool process() {
        static_assert(std::is_same_v<TEvent, std::monostate>, "process() can only be used with event-less FSMs (FSM<> or FSM<std::monostate>).");
        return process(std::monostate{});
    }
    
private:
    // Friend declarations for builder access
    friend class FSMBuilder<TEvent>;
    friend class TransitionBuilder<TEvent>;
    
    // Adds a transition from a state (internal use by builder)
    void addTransition(std::string_view from_state, Transition<TEvent> transition);
    
    // Adds an on-enter action to a state (internal use by builder)
    void addOnEnterAction(std::string_view state, Action action);
    
    // Adds an on-exit action to a state (internal use by builder)
    void addOnExitAction(std::string_view state, Action action);

    std::unordered_map<std::string_view, StateData> states_;
    std::string_view current_state_;
    bool has_initial_state_ = false;
    
#ifdef FSMGINE_MULTI_THREADED
    mutable std::mutex mutex_;
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
    std::unique_lock<std::mutex> lock(mutex_);
#endif
    
    // Optimization 1: Cache StringInterner reference
    auto& interner = StringInterner::instance();
    auto interned_state = interner.intern(state);
    
    // Optimization 2: Single map lookup instead of redundant find
    auto it = states_.find(interned_state);
    if (it == states_.end()) {
        // Optimization 3: Optimized exception string construction
        std::string error_msg;
        error_msg.reserve(50 + state.size());
        error_msg.append("Cannot set initial state to undefined state: ");
        error_msg.append(state);
        throw FSMInvalidStateError(error_msg);
    }
    
    current_state_ = interned_state;
    has_initial_state_ = true;
    
    // Optimization 4: Static dummy event to avoid repeated object construction
    static const TEvent dummy_event{};
    executeOnEnterActions(current_state_, dummy_event);
}

template<typename TEvent>
void FSM<TEvent>::setCurrentState(std::string_view state) {
#ifdef FSMGINE_MULTI_THREADED
    std::unique_lock<std::mutex> lock(mutex_);
#endif
    
    // Optimization 1: Cache StringInterner reference
    auto& interner = StringInterner::instance();
    auto interned_state = interner.intern(state);
    
    // Optimization 2: Single map lookup instead of redundant find
    auto it = states_.find(interned_state);
    if (it == states_.end()) {
        // Optimization 3: Optimized exception string construction
        std::string error_msg;
        error_msg.reserve(50 + state.size());
        error_msg.append("Cannot set current state to undefined state: ");
        error_msg.append(state);
        throw FSMInvalidStateError(error_msg);
    }
    
    // Optimization 4: Static dummy event to avoid repeated object construction
    static const TEvent dummy_event{};
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
    std::unique_lock<std::mutex> lock(mutex_);
#endif
    
    if (!has_initial_state_) {
        throw FSMNotInitializedError();
    }
    
    auto it = states_.find(current_state_);
    if (it == states_.end()) {
        throw FSMStateNotFoundError(std::string(current_state_));
    }
    
    const auto& state_data = it->second;
    
    for (const auto& transition : state_data.transitions) {
        if (transition.predicatesPass(event)) {
            auto target_state = transition.getTargetState();
            
            if (target_state.empty()) {
                throw FSMInvalidStateError("Transition has no target state");
            }
            
            // Optimization 1: Combine target state validation with lookup needed later
            auto target_it = states_.find(target_state);
            if (target_it == states_.end()) {
                throw FSMStateNotFoundError(std::string(target_state));
            }
            
            transition.executeActions(event);
            
            if (current_state_ != target_state) {
                executeOnExitActions(current_state_, event);
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
    std::unique_lock<std::mutex> lock(mutex_);
#endif
    
    if (!has_initial_state_) {
        throw FSMNotInitializedError();
    }
    
    return current_state_;
}

template<typename TEvent>
void FSM<TEvent>::addTransition(std::string_view from_state, Transition<TEvent> transition) {
#ifdef FSMGINE_MULTI_THREADED
    std::unique_lock<std::mutex> lock(mutex_);
#endif
    
    // Optimization 1: Cache StringInterner reference to avoid repeated singleton calls
    auto& interner = StringInterner::instance();
    auto interned_from_state = interner.intern(from_state);
    auto& state_data = getOrCreateState(interned_from_state);
    
    auto target_state = transition.getTargetState();
    if (!target_state.empty()) {
        auto interned_target_state = interner.intern(target_state);
        getOrCreateState(interned_target_state);
    }
    
    state_data.transitions.push_back(std::move(transition));
}

template<typename TEvent>
void FSM<TEvent>::addOnEnterAction(std::string_view state, Action action) {
#ifdef FSMGINE_MULTI_THREADED
    std::unique_lock<std::mutex> lock(mutex_);
#endif
    
    // Optimization 1: Cache StringInterner reference
    auto& interner = StringInterner::instance();
    auto interned_state = interner.intern(state);
    auto& state_data = getOrCreateState(interned_state);
    
    if (action) {
        state_data.on_enter_actions.push_back(std::move(action));
    }
}

template<typename TEvent>
void FSM<TEvent>::addOnExitAction(std::string_view state, Action action) {
#ifdef FSMGINE_MULTI_THREADED
    std::unique_lock<std::mutex> lock(mutex_);
#endif
    
    // Optimization 1: Cache StringInterner reference
    auto& interner = StringInterner::instance();
    auto interned_state = interner.intern(state);
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
