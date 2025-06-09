#include "FSMgine/Transition.hpp"

namespace fsmgine {

void Transition::addPredicate(Predicate pred) {
    if (pred) {
        predicates_.push_back(std::move(pred));
    }
}

void Transition::addAction(Action action) {
    if (action) {
        actions_.push_back(std::move(action));
    }
}

void Transition::setTargetState(std::string_view state) {
    target_state_ = state;
}

bool Transition::evaluatePredicates() const {
    // If no predicates, transition is always valid
    if (predicates_.empty()) {
        return true;
    }
    
    // All predicates must be true
    for (const auto& pred : predicates_) {
        if (!pred()) {
            return false;
        }
    }
    return true;
}

void Transition::executeActions() const {
    for (const auto& action : actions_) {
        action();
    }
}

std::string_view Transition::getTargetState() const {
    return target_state_;
}

bool Transition::hasPredicates() const {
    return !predicates_.empty();
}

bool Transition::hasActions() const {
    return !actions_.empty();
}

bool Transition::hasTargetState() const {
    return !target_state_.empty();
}

} // namespace fsmgine