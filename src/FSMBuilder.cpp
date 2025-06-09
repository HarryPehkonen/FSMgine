#include "FSMgine/FSMBuilder.hpp"
#include "FSMgine/FSM.hpp"
#include "FSMgine/StringInterner.hpp"

namespace fsmgine {

// TransitionBuilder implementation
TransitionBuilder::TransitionBuilder(FSM& fsm, std::string_view from_state)
    : fsm_(fsm), from_state_(from_state) {
}

TransitionBuilder& TransitionBuilder::predicate(Transition::Predicate pred) {
    transition_.addPredicate(std::move(pred));
    return *this;
}

TransitionBuilder& TransitionBuilder::action(Transition::Action action) {
    transition_.addAction(std::move(action));
    return *this;
}

void TransitionBuilder::to(const std::string& state) {
    auto interned_state = StringInterner::instance().intern(state);
    transition_.setTargetState(interned_state);
    fsm_.addTransition(from_state_, std::move(transition_));
}

// FSMBuilder implementation
FSMBuilder::FSMBuilder(FSM& fsm) : fsm_(fsm) {
}

TransitionBuilder FSMBuilder::from(const std::string& state) {
    auto interned_state = StringInterner::instance().intern(state);
    return TransitionBuilder(fsm_, interned_state);
}

FSMBuilder& FSMBuilder::onEnter(const std::string& state, Transition::Action action) {
    auto interned_state = StringInterner::instance().intern(state);
    fsm_.addOnEnterAction(interned_state, std::move(action));
    return *this;
}

FSMBuilder& FSMBuilder::onExit(const std::string& state, Transition::Action action) {
    auto interned_state = StringInterner::instance().intern(state);
    fsm_.addOnExitAction(interned_state, std::move(action));
    return *this;
}

} // namespace fsmgine