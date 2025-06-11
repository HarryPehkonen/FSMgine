#pragma once

#include <string>
#include <string_view>
#include "FSMgine/Transition.hpp"
#include "FSMgine/FSM.hpp"
#include "FSMgine/StringInterner.hpp"

namespace fsmgine {

template<typename TEvent>
class TransitionBuilder {
public:
    using Predicate = typename FSM<TEvent>::Predicate;
    using Action = typename FSM<TEvent>::Action;

    explicit TransitionBuilder(FSM<TEvent>& fsm, std::string_view from_state);
    
    TransitionBuilder(const TransitionBuilder&) = delete;
    TransitionBuilder& operator=(const TransitionBuilder&) = delete;
    TransitionBuilder(TransitionBuilder&&) = default;
    TransitionBuilder& operator=(TransitionBuilder&&) = default;
    
    TransitionBuilder& predicate(Predicate pred);
    TransitionBuilder& action(Action action);
    void to(const std::string& state);
    
private:
    FSM<TEvent>& fsm_;
    std::string_view from_state_;
    Transition<TEvent> transition_;
};

template<typename TEvent>
class FSMBuilder {
public:
    using Action = typename FSM<TEvent>::Action;

    explicit FSMBuilder(FSM<TEvent>& fsm);
    
    FSMBuilder(const FSMBuilder&) = delete;
    FSMBuilder& operator=(const FSMBuilder&) = delete;
    FSMBuilder(FSMBuilder&&) = default;
    FSMBuilder& operator=(FSMBuilder&&) = default;
    
    TransitionBuilder<TEvent> from(const std::string& state);
    FSMBuilder& onEnter(const std::string& state, Action action);
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
    auto interned_state = StringInterner::instance().intern(state);
    transition_.setTargetState(interned_state);
    fsm_.addTransition(from_state_, std::move(transition_));
}

// FSMBuilder
template<typename TEvent>
FSMBuilder<TEvent>::FSMBuilder(FSM<TEvent>& fsm) : fsm_(fsm) {
}

template<typename TEvent>
TransitionBuilder<TEvent> FSMBuilder<TEvent>::from(const std::string& state) {
    auto interned_state = StringInterner::instance().intern(state);
    return TransitionBuilder<TEvent>(fsm_, interned_state);
}

template<typename TEvent>
FSMBuilder<TEvent>& FSMBuilder<TEvent>::onEnter(const std::string& state, Action action) {
    auto interned_state = StringInterner::instance().intern(state);
    fsm_.addOnEnterAction(interned_state, std::move(action));
    return *this;
}

template<typename TEvent>
FSMBuilder<TEvent>& FSMBuilder<TEvent>::onExit(const std::string& state, Action action) {
    auto interned_state = StringInterner::instance().intern(state);
    fsm_.addOnExitAction(interned_state, std::move(action));
    return *this;
}

} // namespace fsmgine
