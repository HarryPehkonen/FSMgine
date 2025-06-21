/// @file FSMBuilder.hpp
/// @brief Fluent builder API for constructing finite state machines
/// @ingroup builder

#pragma once

#include <string>
#include <string_view>
#include "FSMgine/Transition.hpp"
#include "FSMgine/FSM.hpp"
#include "FSMgine/StringInterner.hpp"

/// @defgroup builder Builder API
/// @brief Fluent interface for FSM construction

namespace fsmgine {

/// @brief Builder for creating transitions with a fluent interface
/// @tparam TEvent The event type used for transitions
/// @ingroup builder
/// 
/// @details TransitionBuilder provides a fluent API for defining transitions
/// between states. It supports adding predicates (guards) and actions to transitions.
/// 
/// @note TransitionBuilder instances are created by FSMBuilder::from() and should
/// not be constructed directly.
/// 
/// @par Example
/// @code{.cpp}
/// fsm.get_builder()
///    .from("StateA")
///    .predicate([](const Event& e) { return e.type == "trigger"; })
///    .action([](const Event& e) { std::cout << "Transitioning!"; })
///    .to("StateB");
/// @endcode
template<typename TEvent>
class TransitionBuilder {
public:
    using Predicate = typename FSM<TEvent>::Predicate;
    using Action = typename FSM<TEvent>::Action;

    /// @brief Constructs a transition builder for a specific source state
    /// @param fsm The FSM this transition belongs to
    /// @param from_state The source state for this transition
    explicit TransitionBuilder(FSM<TEvent>& fsm, std::string_view from_state);
    
    TransitionBuilder(const TransitionBuilder&) = delete;
    TransitionBuilder& operator=(const TransitionBuilder&) = delete;
    TransitionBuilder(TransitionBuilder&&) = default;
    TransitionBuilder& operator=(TransitionBuilder&&) = default;
    
    /// @brief Adds a predicate (guard condition) to the transition
    /// @param pred A function that returns true if the transition should occur
    /// @return Reference to this builder for method chaining
    /// @note Multiple predicates can be added; all must pass for the transition to occur
    TransitionBuilder& predicate(Predicate pred);
    
    /// @brief Adds an action to execute during the transition
    /// @param action A function to execute when this transition occurs
    /// @return Reference to this builder for method chaining
    /// @note Multiple actions can be added; they execute in the order added
    TransitionBuilder& action(Action action);
    
    /// @brief Completes the transition by specifying the target state
    /// @param state The target state for this transition
    /// @note This method finalizes and adds the transition to the FSM
    void to(const std::string& state);
    
private:
    FSM<TEvent>& fsm_;
    std::string_view from_state_;
    Transition<TEvent> transition_;
};

/// @brief Main builder class for constructing FSMs with a fluent interface
/// @tparam TEvent The event type used for transitions
/// @ingroup builder
/// 
/// @details FSMBuilder provides a fluent API for constructing finite state machines.
/// It supports:
/// - Defining transitions between states
/// - Adding on-enter and on-exit actions to states
/// - Building complex state machines in a readable, declarative style
/// 
/// @par Example
/// @code{.cpp}
/// FSM<MyEvent> fsm;
/// fsm.get_builder()
///     // Define state actions
///     .onEnter("Idle", [](const MyEvent&) { std::cout << "Entering Idle\n"; })
///     .onExit("Idle", [](const MyEvent&) { std::cout << "Leaving Idle\n"; })
///     
///     // Define transitions
///     .from("Idle")
///         .predicate([](const MyEvent& e) { return e.type == "start"; })
///         .to("Running")
///     
///     .from("Running")
///         .predicate([](const MyEvent& e) { return e.type == "stop"; })
///         .to("Idle");
/// 
/// fsm.setInitialState("Idle");
/// @endcode
template<typename TEvent>
class FSMBuilder {
public:
    using Action = typename FSM<TEvent>::Action;

    /// @brief Constructs a builder for the given FSM
    /// @param fsm The FSM to build
    explicit FSMBuilder(FSM<TEvent>& fsm);
    
    FSMBuilder(const FSMBuilder&) = delete;
    FSMBuilder& operator=(const FSMBuilder&) = delete;
    FSMBuilder(FSMBuilder&&) = default;
    FSMBuilder& operator=(FSMBuilder&&) = default;
    
    /// @brief Starts building a transition from the specified state
    /// @param state The source state for the transition
    /// @return A TransitionBuilder for defining the transition details
    TransitionBuilder<TEvent> from(const std::string& state);
    
    /// @brief Adds an action to execute when entering a state
    /// @param state The state to add the action to
    /// @param action The action to execute when entering the state
    /// @return Reference to this builder for method chaining
    FSMBuilder& onEnter(const std::string& state, Action action);
    
    /// @brief Adds an action to execute when exiting a state
    /// @param state The state to add the action to
    /// @param action The action to execute when exiting the state
    /// @return Reference to this builder for method chaining
    FSMBuilder& onExit(const std::string& state, Action action);
    
private:
    FSM<TEvent>& fsm_;
};

// --- Implementation ---

// TransitionBuilder
template<typename TEvent>
TransitionBuilder<TEvent>::TransitionBuilder(FSM<TEvent>& fsm, std::string_view from_state)
    : fsm_(fsm), from_state_(from_state) {
}

template<typename TEvent>
TransitionBuilder<TEvent>& TransitionBuilder<TEvent>::predicate(Predicate pred) {
    transition_.addPredicate(std::move(pred));
    return *this;
}

template<typename TEvent>
TransitionBuilder<TEvent>& TransitionBuilder<TEvent>::action(Action action) {
    transition_.addAction(std::move(action));
    return *this;
}

template<typename TEvent>
void TransitionBuilder<TEvent>::to(const std::string& state) {
    // Optimization: Cache StringInterner reference
    auto& interner = StringInterner::instance();
    auto interned_state = interner.intern(state);
    transition_.setTargetState(interned_state);
    fsm_.addTransition(from_state_, std::move(transition_));
}

// FSMBuilder
template<typename TEvent>
FSMBuilder<TEvent>::FSMBuilder(FSM<TEvent>& fsm) : fsm_(fsm) {
}

template<typename TEvent>
TransitionBuilder<TEvent> FSMBuilder<TEvent>::from(const std::string& state) {
    // Optimization: Cache StringInterner reference
    auto& interner = StringInterner::instance();
    auto interned_state = interner.intern(state);
    return TransitionBuilder<TEvent>(fsm_, interned_state);
}

template<typename TEvent>
FSMBuilder<TEvent>& FSMBuilder<TEvent>::onEnter(const std::string& state, Action action) {
    // Optimization: Cache StringInterner reference
    auto& interner = StringInterner::instance();
    auto interned_state = interner.intern(state);
    fsm_.addOnEnterAction(interned_state, std::move(action));
    return *this;
}

template<typename TEvent>
FSMBuilder<TEvent>& FSMBuilder<TEvent>::onExit(const std::string& state, Action action) {
    // Optimization: Cache StringInterner reference
    auto& interner = StringInterner::instance();
    auto interned_state = interner.intern(state);
    fsm_.addOnExitAction(interned_state, std::move(action));
    return *this;
}

} // namespace fsmgine
