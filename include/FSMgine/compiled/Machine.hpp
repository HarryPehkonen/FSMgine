/// @file Machine.hpp
/// @brief A compiled finite state machine: enum states, guards as data
/// @ingroup compiled

#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/// @defgroup compiled Compiled FSM Components
/// @brief A second, additive machine type: enum states and guards-as-data, with no
/// interned strings and no std::function on the hot path.

namespace fsmgine::compiled {

/// @brief Exception thrown for programming errors against a compiled::Machine
/// @ingroup compiled
///
/// @details Thrown when a compiled::Machine is used outside its documented contract:
/// process()/getCurrentState()/currentStateName() before setInitialState() or
/// setCurrentState(), or a second refined field introduced into a machine that already
/// has one on a different member (v1 supports exactly one refined field per machine;
/// see Machine::when).
class CompiledMachineError : public std::logic_error {
public:
    /// @brief Constructs an exception describing the misuse
    /// @param message Detailed error message
    explicit CompiledMachineError(const std::string& message) : std::logic_error(message) {}
};

/// @brief The closed vocabulary of refinement comparisons available on one field
/// @ingroup compiled
enum class Op : std::uint8_t { Eq, Lt, Le, Gt, Ge };

/// @brief A transition's action: a plain function pointer, so Transition stays a literal type
/// usable in a constexpr std::array
/// @tparam Event The user's event type
/// @ingroup compiled
///
/// @details Unlike fsmgine::Transition's Action (a std::function), this cannot capture: an
/// action needing context must reach it through a file-scope object. One action per
/// transition, and it runs before the state change, matching the interpreted path.
template <class Event> using Action = void (*)(const Event&);

/// @brief One row of a compiled machine's transition table
/// @tparam State The user's state enum
/// @tparam Event The user's event type
/// @ingroup compiled
///
/// @details A literal type: copyable, usable in a constexpr std::array, and holding no
/// std::function — the action, if any, is a plain function pointer. `op` and `value` are read
/// only when `refined` is true; a transition with `refined == false` fires on the event kind
/// alone. `action` is optional (nullptr means none) and runs before the state change.
template <class State, class Event> struct Transition {
    /// @brief The type of Event's `kind` member, read via decltype(Event::kind)
    using EventKind = decltype(Event::kind);

    State from{};
    EventKind event{};
    bool refined = false;
    Op op = Op::Eq;
    int value = 0;
    State to{};
    Action<Event> action = nullptr;
};

/// @brief A single-field refinement, produced by eq()/lt()/le()/gt()/ge() and consumed by
/// Machine::when()
/// @tparam Event The user's event type
/// @ingroup compiled
template <class Event> struct Refinement {
    int Event::* member;
    Op op;
    int value;
};

/// @brief Builds an Op::Eq refinement on an int field of Event
/// @ingroup compiled
template <class Event>
[[nodiscard]] constexpr Refinement<Event> eq(int Event::* member, int value) {
    return Refinement<Event>{member, Op::Eq, value};
}

/// @brief Builds an Op::Lt refinement on an int field of Event
/// @ingroup compiled
template <class Event>
[[nodiscard]] constexpr Refinement<Event> lt(int Event::* member, int value) {
    return Refinement<Event>{member, Op::Lt, value};
}

/// @brief Builds an Op::Le refinement on an int field of Event
/// @ingroup compiled
template <class Event>
[[nodiscard]] constexpr Refinement<Event> le(int Event::* member, int value) {
    return Refinement<Event>{member, Op::Le, value};
}

/// @brief Builds an Op::Gt refinement on an int field of Event
/// @ingroup compiled
template <class Event>
[[nodiscard]] constexpr Refinement<Event> gt(int Event::* member, int value) {
    return Refinement<Event>{member, Op::Gt, value};
}

/// @brief Builds an Op::Ge refinement on an int field of Event
/// @ingroup compiled
template <class Event>
[[nodiscard]] constexpr Refinement<Event> ge(int Event::* member, int value) {
    return Refinement<Event>{member, Op::Ge, value};
}

/// @brief A compiled finite state machine: enum states, guards as data
/// @tparam State The user's state enum
/// @tparam Event The user's event struct, which must have a public member named `kind`
/// (its type is this machine's EventKind, read via decltype(Event::kind); C++17 class
/// templates cannot deduce a third parameter from a two-argument explicit list such as
/// `Machine<State, EventData>`, which is why the member name is a convention rather than
/// a template parameter)
/// @ingroup compiled
///
/// @details Semantics match fsmgine::FSM<TEvent> exactly: the first matching transition
/// in declaration order wins, a transition with no refinement fires on the event kind
/// alone, and when nothing matches process() returns false and the state is unchanged
/// (no sentinel value).
///
/// Unlike fsmgine::FSM, nothing here is interned, no string is hashed and no
/// std::function is ever called: a row is plain data (two enums, a bool, an Op and an
/// int), so the hot path is integer comparisons over a table. The price is v1's one
/// deliberate limit: AT MOST ONE refined field per machine, set either by the
/// constructor's `refinedMember` or by the first when(kind, refinement) call. There is
/// no lambda escape hatch in v1 — richer per-transition logic belongs on
/// fsmgine::FSM<TEvent> instead.
///
/// @par Thread Safety
/// Not synchronized, and deliberately so: FSMGINE_MULTI_THREADED gates locking on
/// fsmgine::FSM because that class serializes access to StringInterner's shared,
/// process-global state. A compiled::Machine touches no shared state at all — its whole
/// point is removing the interned-string lookup from the hot path — so there is nothing
/// to lock and no compiled::MachineMT variant.
///
/// @par Example
/// @code{.cpp}
/// #include <FSMgine/compiled/Machine.hpp>
/// #include <cstdint>
///
/// enum class State : std::uint8_t { Idle, Running, Done };
/// enum class EventKind : std::uint8_t { Start, Finish };
/// struct EventData { EventKind kind; int progress; };
///
/// int main() {
///     fsmgine::compiled::Machine<State, EventData> m{&EventData::kind};
///     m.from(State::Idle).when(EventKind::Start).to(State::Running);
///     m.from(State::Running)
///         .when(EventKind::Finish, fsmgine::compiled::ge(&EventData::progress, 50))
///         .to(State::Done);
///     m.setInitialState(State::Idle);
///     m.process(EventData{EventKind::Start, 0});  // -> Running
///     return 0;
/// }
/// @endcode
template <class State, class Event> class Machine {
public:
    /// @brief The type of Event's `kind` member, read via decltype(Event::kind)
    using EventKind = decltype(Event::kind);

    /// @brief A function supplying a human-readable name for a state, for currentStateName()
    using NameFn = std::string_view (*)(State);

    /// @brief Constructs a machine told which Event member carries the event kind
    /// @param kindMember Pointer to the Event member process() reads to get the event kind
    /// @param refinedMember Pointer to the single int Event member any refined transition
    /// may compare against; leave as nullptr if no transition is refined, or if it will be
    /// learned from the first when(kind, refinement) call
    /// @param rows A pre-built transition table, e.g. copied from a constexpr std::array;
    /// additional rows may still be appended afterwards with from()/when()/to()
    explicit Machine(EventKind Event::* kindMember, int Event::* refinedMember = nullptr,
                     std::vector<Transition<State, Event>> rows = {})
        : kindMember_(kindMember), refinedMember_(refinedMember), rows_(std::move(rows)) {}

    /// @brief Starts building a transition from the given state
    /// @param state The source state of the transition being built
    /// @return *this, to continue the chain with when()
    Machine& from(State state) {
        buildFrom_ = state;
        return *this;
    }

    /// @brief Continues the transition being built: fires on the event kind alone
    /// @param kind The event kind that triggers this transition
    /// @return *this, to continue the chain with to()
    Machine& when(EventKind kind) {
        buildRow_ = Transition<State, Event>{buildFrom_, kind, false, Op::Eq, 0, buildFrom_};
        return *this;
    }

    /// @brief Continues the transition being built: fires on the event kind AND a refinement
    /// @param kind The event kind that triggers this transition
    /// @param refinement A guard built by eq()/lt()/le()/gt()/ge(), read against one int field
    /// @return *this, to continue the chain with to()
    /// @throws CompiledMachineError if this machine already has a refined transition on a
    /// DIFFERENT field: v1 supports exactly one refined field per machine
    Machine& when(EventKind kind, Refinement<Event> refinement) {
        if (refinedMember_ == nullptr) {
            refinedMember_ = refinement.member;
        } else if (refinedMember_ != refinement.member) {
            throw CompiledMachineError(
                "compiled::Machine supports exactly one refined field per machine");
        }
        buildRow_ = Transition<State, Event>{buildFrom_,       kind,      true, refinement.op,
                                             refinement.value, buildFrom_};
        return *this;
    }

    /// @brief Sets the action to run when the transition being built fires
    /// @param fn A plain function pointer taking the event; capturing is not possible, so an
    /// action needing context must reach it through a file-scope object
    /// @return *this, to continue the chain with to()
    /// @throws CompiledMachineError if this transition already has an action: v1 supports
    /// exactly one action per transition
    Machine& action(Action<Event> fn) {
        if (buildRow_.action != nullptr) {
            throw CompiledMachineError(
                "compiled::Machine supports exactly one action per transition");
        }
        buildRow_.action = fn;
        return *this;
    }

    /// @brief Commits the transition being built, to the given target state
    /// @param state The target state of the transition
    /// @return *this, to start the next transition with from()
    Machine& to(State state) {
        buildRow_.to = state;
        rows_.push_back(buildRow_);
        return *this;
    }

    /// @brief Supplies the function currentStateName() uses to render a state as text
    /// @param nameFn A function from State to a human-readable name
    /// @return *this
    Machine& withNames(NameFn nameFn) {
        nameFn_ = nameFn;
        return *this;
    }

    /// @brief Sets the initial state and marks the machine ready for process()
    /// @param state The initial state
    void setInitialState(State state) {
        currentState_ = state;
        hasInitialState_ = true;
    }

    /// @brief Sets the current state directly, bypassing the transition table
    /// @param state The state to move to
    void setCurrentState(State state) {
        currentState_ = state;
        hasInitialState_ = true;
    }

    /// @brief Gets the current state
    /// @return The current state
    /// @throws CompiledMachineError if setInitialState()/setCurrentState() was never called
    [[nodiscard]] State getCurrentState() const {
        requireInitialized();
        return currentState_;
    }

    /// @brief Gets the current state's name via the function given to withNames()
    /// @return The current state's name, or an empty string_view if withNames() was never
    /// called
    /// @throws CompiledMachineError if setInitialState()/setCurrentState() was never called
    [[nodiscard]] std::string_view currentStateName() const {
        requireInitialized();
        return nameFn_ != nullptr ? nameFn_(currentState_) : std::string_view{};
    }

    /// @brief Processes an event: the first matching transition in declaration order wins
    /// @param event The event to process
    /// @return true if a transition fired, false if nothing matched (state unchanged)
    /// @details A firing transition's action, if any, runs before the state change.
    /// @throws CompiledMachineError if setInitialState()/setCurrentState() was never called, or
    /// if a refined transition fires with no refined field on record
    bool process(const Event& event) {
        requireInitialized();

        const EventKind kind = event.*kindMember_;
        for (const Transition<State, Event>& row : rows_) {
            if (row.from != currentState_ || row.event != kind) {
                continue;
            }
            if (row.refined && !passes(row, event)) {
                continue;
            }
            if (row.action != nullptr) {
                row.action(event);
            }
            currentState_ = row.to;
            return true;
        }
        return false;
    }

private:
    void requireInitialized() const {
        if (!hasInitialState_) {
            throw CompiledMachineError("compiled::Machine has not been initialized with a state");
        }
    }

    [[nodiscard]] bool passes(const Transition<State, Event>& row, const Event& event) const {
        if (refinedMember_ == nullptr) {
            throw CompiledMachineError(
                "compiled::Machine has a refined transition but no refined field");
        }
        const int fieldValue = event.*refinedMember_;
        switch (row.op) {
        case Op::Eq:
            return fieldValue == row.value;
        case Op::Lt:
            return fieldValue < row.value;
        case Op::Le:
            return fieldValue <= row.value;
        case Op::Gt:
            return fieldValue > row.value;
        case Op::Ge:
            return fieldValue >= row.value;
        }
        return false;
    }

    EventKind Event::* kindMember_;
    int Event::* refinedMember_;
    std::vector<Transition<State, Event>> rows_;

    State currentState_{};
    bool hasInitialState_ = false;
    NameFn nameFn_ = nullptr;

    State buildFrom_{};
    Transition<State, Event> buildRow_{};
};

} // namespace fsmgine::compiled
