// The SAME turnstile machine, twice: once on fsmgine::FSM (the interpreted back end —
// string states, std::function guards, built at run time) and once on
// fsmgine::compiled::Machine (the compiled back end — an enum state, a transition table
// of plain data, no std::function on the hot path). Both are driven by one shared event
// script; the two traces are asserted to agree at every step, then printed side by side
// with a timing line for each back end's process() loop.
//
// See the "Compiled Back End" section of README.md for what the compiled back end is,
// when to prefer it, and its four v1 limits.

#include "FSMgine/FSMgine.hpp"
#include "FSMgine/compiled/Machine.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>

using fsmgine::FSM;

namespace {

// --- The event and state vocabulary, shared by both back ends ---

enum class EventKind : std::uint8_t { CoinInserted, DoorPushed };
struct EventData {
    EventKind kind;
};

enum class State : std::uint8_t { Locked, Unlocked };

constexpr std::string_view stateName(State state) {
    return state == State::Locked ? "LOCKED" : "UNLOCKED";
}

// A fixed, deterministic script: coin, push, push (ignored while already unlocked... but
// this turnstile relocks on push, so a second coin is needed), coin, push.
const std::vector<EventData> kScript{
    {EventKind::CoinInserted}, // Locked -> Unlocked
    {EventKind::DoorPushed},   // Unlocked -> Locked
    {EventKind::CoinInserted}, // Locked -> Unlocked
    {EventKind::DoorPushed},   // Unlocked -> Locked
};

// --- Interpreted back end: fsmgine::FSM<EventData> ---

FSM<EventData> buildInterpreted() {
    FSM<EventData> machine;
    auto builder = machine.get_builder();
    builder.from("LOCKED")
        .predicate([](const EventData& e) { return e.kind == EventKind::CoinInserted; })
        .to("UNLOCKED");
    builder.from("UNLOCKED")
        .predicate([](const EventData& e) { return e.kind == EventKind::DoorPushed; })
        .to("LOCKED");
    machine.setInitialState("LOCKED");
    return machine;
}

// --- Compiled back end: fsmgine::compiled::Machine<State, EventData> ---

fsmgine::compiled::Machine<State, EventData> buildCompiled() {
    fsmgine::compiled::Machine<State, EventData> machine{&EventData::kind};
    machine.from(State::Locked).when(EventKind::CoinInserted).to(State::Unlocked);
    machine.from(State::Unlocked).when(EventKind::DoorPushed).to(State::Locked);
    machine.withNames(stateName);
    machine.setInitialState(State::Locked);
    return machine;
}

} // namespace

int main() {
    auto interpreted = buildInterpreted();
    auto compiled = buildCompiled();

    std::cout << "=== Interpreted (fsmgine::FSM<EventData>) ===\n";
    std::cout << "Current state: " << interpreted.getCurrentState() << '\n';

    std::cout << "=== Compiled (fsmgine::compiled::Machine<State, EventData>) ===\n";
    std::cout << "Current state: " << compiled.currentStateName() << '\n';

    for (const auto& event : kScript) {
        const bool interpretedFired = interpreted.process(event);
        const bool compiledFired = compiled.process(event);

        if (interpretedFired != compiledFired
            || interpreted.getCurrentState() != compiled.currentStateName()) {
            std::cerr << "FATAL: the interpreted and compiled back ends disagree\n";
            return 1;
        }

        std::cout << "interpreted -> " << interpreted.getCurrentState() << "   compiled -> "
                  << compiled.currentStateName() << '\n';
    }

    // A quick timing line, not a benchmark: benchmarks/bench_comparison.cpp is the
    // measured comparison (see its "compiled" column and README's bench table).
    constexpr int kPasses = 100000;
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < kPasses; ++i) {
        for (const auto& event : kScript) {
            compiled.process(event);
        }
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const double nsPerEvent
        = std::chrono::duration<double, std::nano>(elapsed).count()
          / (static_cast<double>(kPasses) * static_cast<double>(kScript.size()));
    std::cout << "\ncompiled back end: " << nsPerEvent << " ns/event over " << kPasses
              << " passes of the script\n";

    std::cout << "\nBoth back ends agree at every step.\n";
    return 0;
}
