#include "FSMgine/FSMgine.hpp"
#include "FSMgine/compiled/Machine.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <string_view>
#include <vector>

namespace {

enum class State : std::uint8_t { Idle, Running, Paused, Done };
enum class EventKind : std::uint8_t { Start, Pause, Resume, Finish, Reset };
struct EventData {
    EventKind kind;
    int progress;
};

constexpr std::string_view nm(State s) {
    switch (s) {
    case State::Idle:
        return "Idle";
    case State::Running:
        return "Running";
    case State::Paused:
        return "Paused";
    case State::Done:
        return "Done";
    }
    return "?";
}

// Actions are plain function pointers and cannot capture, so a file-scope log is the only
// way for them to record anything. Deliberate: see the Action alias in compiled/Machine.hpp.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::vector<EventData> gFsmActions;
void recordFsmAction(const EventData& e) { gFsmActions.push_back(e); }
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::vector<EventData> gCompiledActions;
void recordCompiledAction(const EventData& e) { gCompiledActions.push_back(e); }

fsmgine::FSM<EventData> buildFsm() {
    fsmgine::FSM<EventData> f;
    auto b = f.get_builder();
    b.from("Idle")
        .predicate([](const EventData& e) { return e.kind == EventKind::Start; })
        .action(recordFsmAction)
        .to("Running");
    b.from("Running")
        .predicate([](const EventData& e) { return e.kind == EventKind::Pause; })
        .action(recordFsmAction)
        .to("Paused");
    b.from("Paused")
        .predicate([](const EventData& e) { return e.kind == EventKind::Resume; })
        .action(recordFsmAction)
        .to("Running");
    b.from("Running")
        .predicate(
            [](const EventData& e) { return e.kind == EventKind::Finish && e.progress >= 50; })
        .action(recordFsmAction)
        .to("Done");
    b.from("Idle")
        .predicate([](const EventData& e) { return e.kind == EventKind::Reset; })
        .action(recordFsmAction)
        .to("Idle");
    b.from("Running")
        .predicate([](const EventData& e) { return e.kind == EventKind::Reset; })
        .action(recordFsmAction)
        .to("Idle");
    b.from("Paused")
        .predicate([](const EventData& e) { return e.kind == EventKind::Reset; })
        .action(recordFsmAction)
        .to("Idle");
    b.from("Done")
        .predicate([](const EventData& e) { return e.kind == EventKind::Reset; })
        .action(recordFsmAction)
        .to("Idle");
    f.setInitialState("Idle");
    return f;
}

fsmgine::compiled::Machine<State, EventData> buildCompiled() {
    fsmgine::compiled::Machine<State, EventData> m{&EventData::kind};
    m.from(State::Idle).when(EventKind::Start).action(recordCompiledAction).to(State::Running);
    m.from(State::Running).when(EventKind::Pause).action(recordCompiledAction).to(State::Paused);
    m.from(State::Paused).when(EventKind::Resume).action(recordCompiledAction).to(State::Running);
    m.from(State::Running)
        .when(EventKind::Finish, fsmgine::compiled::ge(&EventData::progress, 50))
        .action(recordCompiledAction)
        .to(State::Done);
    m.from(State::Idle).when(EventKind::Reset).action(recordCompiledAction).to(State::Idle);
    m.from(State::Running).when(EventKind::Reset).action(recordCompiledAction).to(State::Idle);
    m.from(State::Paused).when(EventKind::Reset).action(recordCompiledAction).to(State::Idle);
    m.from(State::Done).when(EventKind::Reset).action(recordCompiledAction).to(State::Idle);
    m.setInitialState(State::Idle);
    return m;
}

// Deterministic LCG script: all five event kinds, progress spanning 0..100 so the
// refinement on Finish (>= 50) is exercised both ways.
std::vector<EventData> makeScript(std::size_t n) {
    std::vector<EventData> v;
    v.reserve(n);
    std::uint64_t x = 88172645463325252ULL;
    constexpr std::array<EventKind, 5> kinds = {
        EventKind::Start, EventKind::Pause, EventKind::Resume, EventKind::Finish, EventKind::Reset};
    for (std::size_t i = 0; i < n; ++i) {
        x = x * 6364136223846793005ULL + 1442695040888963407ULL;
        const EventKind kind = kinds.at(static_cast<std::size_t>((x >> 33) % 5));
        const int progress = static_cast<int>((x >> 17) % 101);
        v.push_back(EventData{kind, progress});
    }
    return v;
}

} // namespace

// The primary test: a differential oracle against fsmgine::FSM<EventData>, comparing
// BOTH the resulting state and whether a transition fired, at every one of 100,000
// events. Comparing "fired" is required: a self-loop (Reset from Idle) fires while the
// state is unchanged, so comparing state alone would miss a divergence there.
TEST(CompiledMachineOracle, MatchesFSMStateAndFiredFlagOverOneHundredThousandEvents) {
    auto fsm = buildFsm();
    auto compiled = buildCompiled();
    const std::vector<EventData> events = makeScript(100000);

    for (std::size_t i = 0; i < events.size(); ++i) {
        const bool fsmFired = fsm.process(events[i]);
        const bool compiledFired = compiled.process(events[i]);
        ASSERT_EQ(fsmFired, compiledFired) << "fired-flag mismatch at event " << i;
        ASSERT_EQ(fsm.getCurrentState(), nm(compiled.getCurrentState()))
            << "state mismatch at event " << i;
        ASSERT_EQ(gFsmActions.size(), gCompiledActions.size())
            << "action count mismatch at event " << i;
        if (!gFsmActions.empty()) {
            ASSERT_EQ(gFsmActions.back().kind, gCompiledActions.back().kind);
            ASSERT_EQ(gFsmActions.back().progress, gCompiledActions.back().progress);
        }
    }
}

TEST(CompiledMachine, FirstMatchingTransitionWins) {
    fsmgine::compiled::Machine<State, EventData> m{&EventData::kind};
    m.from(State::Idle).when(EventKind::Start).to(State::Running);
    m.from(State::Idle).when(EventKind::Start).to(State::Done);
    m.setInitialState(State::Idle);

    EXPECT_TRUE(m.process(EventData{EventKind::Start, 0}));
    EXPECT_EQ(m.getCurrentState(), State::Running);
}

TEST(CompiledMachine, NoMatchLeavesStateUnchanged) {
    fsmgine::compiled::Machine<State, EventData> m{&EventData::kind};
    m.from(State::Idle).when(EventKind::Start).to(State::Running);
    m.setInitialState(State::Idle);

    EXPECT_FALSE(m.process(EventData{EventKind::Pause, 0}));
    EXPECT_EQ(m.getCurrentState(), State::Idle);
}

TEST(CompiledMachine, HandWrittenConstexprArrayMatchesBuilderMadeMachine) {
    constexpr std::array<fsmgine::compiled::Transition<State, EventData>, 8> kRows{{
        {State::Idle, EventKind::Start, false, fsmgine::compiled::Op::Eq, 0, State::Running},
        {State::Running, EventKind::Pause, false, fsmgine::compiled::Op::Eq, 0, State::Paused},
        {State::Paused, EventKind::Resume, false, fsmgine::compiled::Op::Eq, 0, State::Running},
        {State::Running, EventKind::Finish, true, fsmgine::compiled::Op::Ge, 50, State::Done},
        {State::Idle, EventKind::Reset, false, fsmgine::compiled::Op::Eq, 0, State::Idle},
        {State::Running, EventKind::Reset, false, fsmgine::compiled::Op::Eq, 0, State::Idle},
        {State::Paused, EventKind::Reset, false, fsmgine::compiled::Op::Eq, 0, State::Idle},
        {State::Done, EventKind::Reset, false, fsmgine::compiled::Op::Eq, 0, State::Idle},
    }};

    fsmgine::compiled::Machine<State, EventData> fromArray{
        &EventData::kind, &EventData::progress,
        std::vector<fsmgine::compiled::Transition<State, EventData>>(kRows.begin(), kRows.end())};
    fromArray.setInitialState(State::Idle);

    auto fromBuilder = buildCompiled();

    const std::vector<EventData> events = makeScript(5000);
    for (std::size_t i = 0; i < events.size(); ++i) {
        const bool firedArray = fromArray.process(events[i]);
        const bool firedBuilder = fromBuilder.process(events[i]);
        ASSERT_EQ(firedArray, firedBuilder) << "fired mismatch at event " << i;
        ASSERT_EQ(fromArray.getCurrentState(), fromBuilder.getCurrentState())
            << "state mismatch at event " << i;
    }
}

TEST(CompiledMachine, CurrentStateNameReturnsNameTableEntry) {
    fsmgine::compiled::Machine<State, EventData> m{&EventData::kind};
    m.from(State::Idle).when(EventKind::Start).to(State::Running);
    m.withNames(nm);
    m.setInitialState(State::Idle);

    EXPECT_EQ(m.currentStateName(), "Idle");
    m.process(EventData{EventKind::Start, 0});
    EXPECT_EQ(m.currentStateName(), "Running");
}

TEST(CompiledMachine, ActionFiresOnceWithTheFiringEvent) {
    gCompiledActions.clear();
    fsmgine::compiled::Machine<State, EventData> m{&EventData::kind};
    m.from(State::Idle).when(EventKind::Start).action(recordCompiledAction).to(State::Running);
    m.setInitialState(State::Idle);
    EXPECT_TRUE(m.process(EventData{EventKind::Start, 7}));
    ASSERT_EQ(gCompiledActions.size(), 1U);
    EXPECT_EQ(gCompiledActions.at(0).progress, 7);
    EXPECT_FALSE(m.process(EventData{EventKind::Start, 9}));
    EXPECT_EQ(gCompiledActions.size(), 1U);
}

TEST(CompiledMachine, ActionFiresOnASelfLoop) {
    gCompiledActions.clear();
    fsmgine::compiled::Machine<State, EventData> m{&EventData::kind};
    m.from(State::Idle).when(EventKind::Reset).action(recordCompiledAction).to(State::Idle);
    m.setInitialState(State::Idle);
    EXPECT_TRUE(m.process(EventData{EventKind::Reset, 0}));
    EXPECT_EQ(m.getCurrentState(), State::Idle);
    EXPECT_EQ(gCompiledActions.size(), 1U);
}

TEST(CompiledMachine, EveryComparisonOperatorComparesAsDeclared) {
    struct Case {
        fsmgine::compiled::Op op;
        int threshold;
        int value;
        bool expectFired;
    };
    constexpr std::array<Case, 9> cases{{
        {fsmgine::compiled::Op::Lt, 50, 40, true},
        {fsmgine::compiled::Op::Lt, 50, 50, false},
        {fsmgine::compiled::Op::Lt, 50, 60, false},
        {fsmgine::compiled::Op::Le, 50, 40, true},
        {fsmgine::compiled::Op::Le, 50, 50, true},
        {fsmgine::compiled::Op::Le, 50, 60, false},
        {fsmgine::compiled::Op::Gt, 50, 40, false},
        {fsmgine::compiled::Op::Gt, 50, 50, false},
        {fsmgine::compiled::Op::Gt, 50, 60, true},
    }};
    for (const Case& c : cases) {
        auto refine = [&c]() -> fsmgine::compiled::Refinement<EventData> {
            switch (c.op) {
            case fsmgine::compiled::Op::Lt:
                return fsmgine::compiled::lt(&EventData::progress, c.threshold);
            case fsmgine::compiled::Op::Le:
                return fsmgine::compiled::le(&EventData::progress, c.threshold);
            case fsmgine::compiled::Op::Gt:
                return fsmgine::compiled::gt(&EventData::progress, c.threshold);
            case fsmgine::compiled::Op::Ge:
                return fsmgine::compiled::ge(&EventData::progress, c.threshold);
            case fsmgine::compiled::Op::Eq:
                return fsmgine::compiled::eq(&EventData::progress, c.threshold);
            }
            return fsmgine::compiled::eq(&EventData::progress, c.threshold);
        };
        fsmgine::compiled::Machine<State, EventData> m{&EventData::kind};
        m.from(State::Idle).when(EventKind::Finish, refine()).to(State::Done);
        m.setInitialState(State::Idle);
        EXPECT_EQ(m.process(EventData{EventKind::Finish, c.value}), c.expectFired);
        EXPECT_EQ(m.getCurrentState(), c.expectFired ? State::Done : State::Idle);
    }
}

TEST(CompiledMachine, ThrowsOnASecondActionForOneTransition) {
    fsmgine::compiled::Machine<State, EventData> m{&EventData::kind};
    EXPECT_THROW((void)m.from(State::Idle)
                     .when(EventKind::Start)
                     .action(recordCompiledAction)
                     .action(recordCompiledAction),
                 fsmgine::compiled::CompiledMachineError);
}

TEST(CompiledMachine, ThrowsOnASecondRefinedField) {
    struct Event2 {
        EventKind kind;
        int progress;
        int other;
    };
    fsmgine::compiled::Machine<State, Event2> m{&Event2::kind};
    m.from(State::Idle)
        .when(EventKind::Finish, fsmgine::compiled::ge(&Event2::progress, 50))
        .to(State::Done);
    EXPECT_THROW(
        (void)m.from(State::Idle).when(EventKind::Finish, fsmgine::compiled::ge(&Event2::other, 1)),
        fsmgine::compiled::CompiledMachineError);
}

TEST(CompiledMachine, ThrowsOnARefinedRowWithNoRefinedField) {
    constexpr std::array<fsmgine::compiled::Transition<State, EventData>, 1> kRows{{
        {State::Idle, EventKind::Finish, true, fsmgine::compiled::Op::Ge, 50, State::Done},
    }};
    fsmgine::compiled::Machine<State, EventData> m{
        &EventData::kind, nullptr,
        std::vector<fsmgine::compiled::Transition<State, EventData>>(kRows.begin(), kRows.end())};
    m.setInitialState(State::Idle);
    EXPECT_THROW((void)m.process(EventData{EventKind::Finish, 99}),
                 fsmgine::compiled::CompiledMachineError);
}

// A hand-written constexpr row carrying a real action: a function pointer is a valid
// constant expression, but "should work" is not evidence, so this proves it.
TEST(CompiledMachine, ConstexprRowWithAnActionFiresIt) {
    gCompiledActions.clear();
    constexpr std::array<fsmgine::compiled::Transition<State, EventData>, 1> kRows{{
        {State::Idle, EventKind::Start, false, fsmgine::compiled::Op::Eq, 0, State::Running,
         recordCompiledAction},
    }};
    fsmgine::compiled::Machine<State, EventData> m{
        &EventData::kind, nullptr,
        std::vector<fsmgine::compiled::Transition<State, EventData>>(kRows.begin(), kRows.end())};
    m.setInitialState(State::Idle);
    EXPECT_TRUE(m.process(EventData{EventKind::Start, 3}));
    EXPECT_EQ(m.getCurrentState(), State::Running);
    ASSERT_EQ(gCompiledActions.size(), 1U);
    EXPECT_EQ(gCompiledActions.at(0).progress, 3);
}

TEST(CompiledMachine, SetCurrentStateBypassesTheTable) {
    fsmgine::compiled::Machine<State, EventData> m{&EventData::kind};
    m.from(State::Idle).when(EventKind::Start).to(State::Running);
    m.setCurrentState(State::Paused);
    EXPECT_EQ(m.getCurrentState(), State::Paused);
}

TEST(CompiledMachine, ThrowsWhenUsedBeforeAnyStateIsSet) {
    fsmgine::compiled::Machine<State, EventData> m{&EventData::kind};
    EXPECT_THROW((void)m.getCurrentState(), fsmgine::compiled::CompiledMachineError);
    EXPECT_THROW((void)m.currentStateName(), fsmgine::compiled::CompiledMachineError);
    EXPECT_THROW((void)m.process(EventData{EventKind::Start, 0}),
                 fsmgine::compiled::CompiledMachineError);
}

TEST(CompiledMachine, CurrentStateNameIsEmptyWithoutWithNames) {
    fsmgine::compiled::Machine<State, EventData> m{&EventData::kind};
    m.from(State::Idle).when(EventKind::Start).to(State::Running);
    m.setInitialState(State::Idle);
    EXPECT_TRUE(m.currentStateName().empty());
}

// eq() is the one refinement helper no other test uses, so both it and the Op::Eq
// branch inside the guard evaluation were cold.
TEST(CompiledMachine, EqualityRefinementMatchesOnlyThatValue) {
    fsmgine::compiled::Machine<State, EventData> m{&EventData::kind};
    m.from(State::Idle)
        .when(EventKind::Finish, fsmgine::compiled::eq(&EventData::progress, 42))
        .to(State::Done);
    m.setInitialState(State::Idle);
    EXPECT_FALSE(m.process(EventData{EventKind::Finish, 41}));
    EXPECT_EQ(m.getCurrentState(), State::Idle);
    EXPECT_TRUE(m.process(EventData{EventKind::Finish, 42}));
    EXPECT_EQ(m.getCurrentState(), State::Done);
}

TEST(CompiledMachine, ErrorMessageNamesTheProblem) {
    fsmgine::compiled::Machine<State, EventData> m{&EventData::kind};
    try {
        (void)m.process(EventData{EventKind::Start, 0});
        FAIL() << "expected CompiledMachineError";
    } catch (const fsmgine::compiled::CompiledMachineError& e) {
        EXPECT_STREQ(e.what(), "compiled::Machine has not been initialized with a state");
    }
}

// --- Builder-contract regressions, found by an independent review A/B (2026-10-04).
// Each asserts what the interpreted TransitionBuilder already guarantees: a chain
// either builds the machine its code reads, or it throws. Written first, RED first.

TEST(CompiledMachineBuilderContract, ActionBeforeWhenIsNotSilentlyDropped) {
    gCompiledActions.clear();
    fsmgine::compiled::Machine<State, EventData> m{&EventData::kind};
    // An action written before when() must still fire: the row is what the chain built.
    m.from(State::Idle).action(recordCompiledAction).when(EventKind::Start).to(State::Running);
    m.setInitialState(State::Idle);

    EXPECT_TRUE(m.process(EventData{EventKind::Start, 0}));
    EXPECT_EQ(m.getCurrentState(), State::Running);
    ASSERT_EQ(gCompiledActions.size(), 1U);
    EXPECT_EQ(gCompiledActions.front().kind, EventKind::Start);
}

TEST(CompiledMachineBuilderContract, ToWithoutWhenIsRejected) {
    fsmgine::compiled::Machine<State, EventData> m{&EventData::kind};
    // A commit with no when() would push a stale or default row: a transition that
    // does not exist. The interpreted builder throws for the analogous misuse.
    EXPECT_THROW(m.from(State::Idle).to(State::Running), fsmgine::compiled::CompiledMachineError);
}

TEST(CompiledMachineBuilderContract, ActionAfterCommitIsRejectedForTheRightReason) {
    fsmgine::compiled::Machine<State, EventData> m{&EventData::kind};
    m.from(State::Idle).when(EventKind::Start).to(State::Running);
    // Nothing is open after the commit, so the error must say that - not blame the
    // finished transition for already having an action.
    try {
        m.action(recordCompiledAction);
        FAIL() << "expected CompiledMachineError with no transition open";
    } catch (const fsmgine::compiled::CompiledMachineError& e) {
        EXPECT_NE(std::string_view(e.what()).find("no transition"), std::string_view::npos)
            << "message was: " << e.what();
    }
}

TEST(CompiledMachineBuilderContract, AnAbandonedWhenDoesNotClaimTheRefinedField) {
    struct TwoFieldEvent {
        EventKind kind{EventKind::Start};
        int progress{0};
        int retries{0};
    };
    fsmgine::compiled::Machine<State, TwoFieldEvent> m{&TwoFieldEvent::kind};
    // This when() never reaches to(), so it commits nothing - and must therefore not
    // reserve the machine's one refined field for good.
    m.from(State::Idle)
        .when(EventKind::Finish, fsmgine::compiled::ge(&TwoFieldEvent::progress, 50));
    EXPECT_NO_THROW(m.from(State::Running)
                        .when(EventKind::Finish, fsmgine::compiled::lt(&TwoFieldEvent::retries, 3))
                        .to(State::Paused));
}

TEST(CompiledMachineBuilderContract, ProcessMutatesTheInstanceSoOneMachineIsNotShareable) {
    fsmgine::compiled::Machine<State, EventData> m{&EventData::kind};
    m.from(State::Idle).when(EventKind::Start).to(State::Running);
    m.setInitialState(State::Idle);
    const State before = m.getCurrentState();
    EXPECT_TRUE(m.process(EventData{EventKind::Start, 0}));
    // process() writes the machine's own currentState_, so a shared instance driven from
    // two threads is a data race. This test pins the behaviour the docs must warn about,
    // the way fsmgine::FSM warns: one instance per thread.
    EXPECT_NE(before, m.getCurrentState());
}

TEST(CompiledMachineBuilderContract, WhenWithNoOpenTransitionIsRejected) {
    fsmgine::compiled::Machine<State, EventData> m{&EventData::kind};
    // when() with no from() before it has nothing to attach to. Covers the guard the
    // other three do not reach, so the header's throw paths stay fully exercised.
    EXPECT_THROW(m.when(EventKind::Start), fsmgine::compiled::CompiledMachineError);
}
