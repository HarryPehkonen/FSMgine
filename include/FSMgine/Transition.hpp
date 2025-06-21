/// @file Transition.hpp
/// @brief State transition representation with guards and actions
/// @ingroup transitions

#pragma once

#include <functional>
#include <vector>
#include <string_view>

/// @defgroup transitions Transition System
/// @brief Components for managing state transitions

namespace fsmgine {

// Forward declaration
template<typename TEvent>
class TransitionBuilder;

/// @brief Represents a transition between states in a finite state machine
/// @tparam TEvent The event type that triggers transitions
/// @ingroup transitions
/// 
/// @details A Transition encapsulates:
/// - Zero or more predicates (guard conditions) that must all pass for the transition to occur
/// - Zero or more actions to execute when the transition occurs
/// - The target state to transition to
/// 
/// Transitions are typically created using the FSMBuilder fluent API rather than directly.
/// 
/// @par Predicate Evaluation
/// - If no predicates are added, the transition always passes
/// - If multiple predicates are added, ALL must return true (AND logic)
/// - Predicates are evaluated in the order they were added
/// 
/// @par Action Execution
/// - Actions are executed in the order they were added
/// - Actions are only executed if all predicates pass
/// - Actions are executed before the state change occurs
template<typename TEvent>
class Transition {
public:
    /// @brief Type alias for transition guard predicates
    using Predicate = std::function<bool(const TEvent&)>;
    
    /// @brief Type alias for transition actions
    using Action = std::function<void(const TEvent&)>;
    
    /// @brief Default constructor
    Transition() = default;
    
    Transition(const Transition&) = delete;
    Transition& operator=(const Transition&) = delete;
    
    /// @brief Move constructor (defaulted)
    Transition(Transition&&) = default;
    
    /// @brief Move assignment operator (defaulted)
    Transition& operator=(Transition&&) = default;
    
    /// @brief Adds a predicate (guard condition) to this transition
    /// @param pred A function that returns true if the transition should be allowed
    /// @note Multiple predicates can be added; all must pass for the transition to occur
    /// @note Null predicates are ignored
    /// @note This method is primarily for use by TransitionBuilder
    void addPredicate(Predicate pred);
    
    /// @brief Adds an action to execute when this transition occurs
    /// @param action A function to execute during the transition
    /// @note Multiple actions can be added; they execute in order
    /// @note Null actions are ignored
    /// @note This method is primarily for use by TransitionBuilder
    void addAction(Action action);
    
    /// @brief Sets the target state for this transition
    /// @param state The name of the state to transition to
    /// @note The state should be interned using StringInterner for consistency
    /// @note This method is primarily for use by TransitionBuilder
    void setTargetState(std::string_view state);
    
    /// @brief Evaluates all predicates for this transition
    /// @param event The event to evaluate predicates against
    /// @return true if all predicates pass (or no predicates exist), false otherwise
    bool predicatesPass(const TEvent& event) const;
    
    /// @brief Executes all actions associated with this transition
    /// @param event The event that triggered the transition
    /// @note Actions are executed in the order they were added
    void executeActions(const TEvent& event) const;
    
    /// @brief Gets the target state for this transition
    /// @return The target state name, or empty string_view if not set
    std::string_view getTargetState() const;
    
    /// @brief Gets all actions associated with this transition
    /// @return A const reference to the vector of actions
    const std::vector<Action>& getActions() const;
    
    /// @brief Checks if this transition has any predicates
    /// @return true if at least one predicate exists
    bool hasPredicates() const;
    
    /// @brief Checks if this transition has any actions
    /// @return true if at least one action exists
    bool hasActions() const;
    
    /// @brief Checks if this transition has a target state
    /// @return true if a target state has been set
    bool hasTargetState() const;

private:
    // Friend declaration for builder access
    friend class TransitionBuilder<TEvent>;
    
    std::vector<Predicate> predicates_;
    std::vector<Action> actions_;
    std::string_view target_state_;
};

// --- Implementation ---

template<typename TEvent>
bool Transition<TEvent>::predicatesPass(const TEvent& event) const {
    if (predicates_.empty()) {
        return true;
    }
    
    for (const auto& pred : predicates_) {
        if (!pred(event)) {
            return false;
        }
    }
    return true;
}

template<typename TEvent>
void Transition<TEvent>::executeActions(const TEvent& event) const {
    for (const auto& action : actions_) {
        action(event);
    }
}

template<typename TEvent>
std::string_view Transition<TEvent>::getTargetState() const {
    return target_state_;
}

template<typename TEvent>
const std::vector<typename Transition<TEvent>::Action>& Transition<TEvent>::getActions() const {
    return actions_;
}

template<typename TEvent>
bool Transition<TEvent>::hasPredicates() const {
    return !predicates_.empty();
}

template<typename TEvent>
bool Transition<TEvent>::hasActions() const {
    return !actions_.empty();
}

template<typename TEvent>
bool Transition<TEvent>::hasTargetState() const {
    return !target_state_.empty();
}

template<typename TEvent>
void Transition<TEvent>::addPredicate(Predicate pred) {
    if (pred) {
        predicates_.push_back(std::move(pred));
    }
}

template<typename TEvent>
void Transition<TEvent>::addAction(Action action) {
    if (action) {
        actions_.push_back(std::move(action));
    }
}

template<typename TEvent>
void Transition<TEvent>::setTargetState(std::string_view state) {
    target_state_ = state;
}

} // namespace fsmgine
