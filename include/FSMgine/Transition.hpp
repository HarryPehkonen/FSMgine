#pragma once

#include <functional>
#include <vector>
#include <string_view>

namespace fsmgine {

template<typename TEvent>
class Transition {
public:
    using Predicate = std::function<bool(const TEvent&)>;
    using Action = std::function<void(const TEvent&)>;
    
    Transition() = default;
    
    Transition(const Transition&) = delete;
    Transition& operator=(const Transition&) = delete;
    Transition(Transition&&) = default;
    Transition& operator=(Transition&&) = default;
    
    void addPredicate(Predicate pred);
    void addAction(Action action);
    void setTargetState(std::string_view state);
    
    bool evaluatePredicates(const TEvent& event) const;
    void executeActions(const TEvent& event) const;
    std::string_view getTargetState() const;
    
    bool hasPredicates() const;
    bool hasActions() const;
    bool hasTargetState() const;

private:
    std::vector<Predicate> predicates_;
    std::vector<Action> actions_;
    std::string_view target_state_;
};

// --- Implementation ---

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

template<typename TEvent>
bool Transition<TEvent>::evaluatePredicates(const TEvent& event) const {
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

} // namespace fsmgine
