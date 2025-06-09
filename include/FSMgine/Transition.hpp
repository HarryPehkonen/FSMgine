#pragma once

#include <functional>
#include <vector>
#include <string_view>

namespace fsmgine {

class Transition {
public:
    using Predicate = std::function<bool()>;
    using Action = std::function<void()>;
    
    Transition() = default;
    
    // Move-only semantics for performance
    Transition(const Transition&) = delete;
    Transition& operator=(const Transition&) = delete;
    Transition(Transition&&) = default;
    Transition& operator=(Transition&&) = default;
    
    void addPredicate(Predicate pred);
    void addAction(Action action);
    void setTargetState(std::string_view state);
    
    bool evaluatePredicates() const;
    void executeActions() const;
    std::string_view getTargetState() const;
    
    bool hasPredicates() const;
    bool hasActions() const;
    bool hasTargetState() const;

private:
    std::vector<Predicate> predicates_;
    std::vector<Action> actions_;
    std::string_view target_state_;
};

} // namespace fsmgine