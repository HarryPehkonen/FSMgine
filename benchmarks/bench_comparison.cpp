// FSMgine vs. hand-rolled equivalents: four implementations of the SAME small machine,
// timed by the same <chrono>-based harness (no Google Benchmark — not installed, and
// deliberately not a dependency of this target).
//
//   states: Idle, Running, Paused, Done      events: Start, Pause, Resume, Finish, Reset
//   Idle    + Start  -> Running
//   Running + Pause  -> Paused
//   Paused  + Resume -> Running
//   Running + Finish -> Done     (guard: progress >= 50)
//   Done    + Reset  -> Idle
//   any     + Reset  -> Idle
//
// (a) FSMgine       — states/events as strings, built through the fluent builder
//     (the interpreted back end).
// (b) hand-rolled switch — enum State in a switch statement: the compile-time baseline.
// (c) hand-rolled table  — std::array<TableEntry> scanned linearly: small ints, no
//     strings, no std::function. This is the FAIR comparison: it isolates what
//     FSMgine's API costs over a hand-written DYNAMIC equivalent, rather than comparing
//     against code the compiler can fully see through.
// (d) compiled       — fsmgine::compiled::Machine<State, EventData>: the SAME machine on
//     the compiled back end, same event mix as every other column.
//
// Run `./FSMgine_comparison --markdown` for the table tools/update_bench_table.sh pastes
// into README.md; run it with no arguments for a human-readable report.

#include "FSMgine/FSM.hpp"
#include "FSMgine/FSMBuilder.hpp"
#include "FSMgine/compiled/Machine.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using fsmgine::FSM;

// --- The event and state vocabulary, shared by all three implementations ---

enum class EventKind : std::uint8_t { Start, Pause, Resume, Finish, Reset };
enum class State : std::uint8_t { Idle, Running, Paused, Done };

struct EventData {
    EventKind kind = EventKind::Start;
    int progress = 0;
};

constexpr std::array<const char*, 4> kStateNames{"Idle", "Running", "Paused", "Done"};

std::string_view stateName(State state) { return kStateNames.at(static_cast<std::size_t>(state)); }

// The guard, as one explicit predicate reused by all three implementations below —
// FSMgine's predicate(), the switch's if-condition, and the table's guard dispatch all
// call this same function, so "guard: progress >= 50" cannot drift between them.
bool guardProgressAtLeast50(const EventData& event) { return event.progress >= 50; }

// A fixed, deterministic script (no random device) that visits every transition at
// least once, both outcomes of the guarded Running+Finish transition, and Reset from
// three different source states (the "any" in any+Reset).
const std::vector<EventData> kScript{
    {EventKind::Start, 0},   // Idle -> Running
    {EventKind::Pause, 0},   // Running -> Paused
    {EventKind::Resume, 0},  // Paused -> Running
    {EventKind::Finish, 20}, // guard fails (progress < 50): no transition, stays Running
    {EventKind::Finish, 80}, // guard passes: Running -> Done
    {EventKind::Reset, 0},   // Done -> Idle
    {EventKind::Reset, 0},   // Idle -> Idle (the "any" part of any+Reset)
    {EventKind::Start, 0},   // Idle -> Running
    {EventKind::Pause, 0},   // Running -> Paused
    {EventKind::Reset, 0},   // Paused -> Idle
};

// --- (a) FSMgine ---

FSM<EventData> buildFsmgineMachine() {
    FSM<EventData> machine;
    auto builder = machine.get_builder();
    builder.from("Idle")
        .predicate([](const EventData& e) { return e.kind == EventKind::Start; })
        .to("Running");
    builder.from("Running")
        .predicate([](const EventData& e) { return e.kind == EventKind::Pause; })
        .to("Paused");
    builder.from("Paused")
        .predicate([](const EventData& e) { return e.kind == EventKind::Resume; })
        .to("Running");
    builder.from("Running")
        .predicate([](const EventData& e) { return e.kind == EventKind::Finish; })
        .predicate([](const EventData& e) { return guardProgressAtLeast50(e); })
        .to("Done");
    builder.from("Idle")
        .predicate([](const EventData& e) { return e.kind == EventKind::Reset; })
        .to("Idle");
    builder.from("Running")
        .predicate([](const EventData& e) { return e.kind == EventKind::Reset; })
        .to("Idle");
    builder.from("Paused")
        .predicate([](const EventData& e) { return e.kind == EventKind::Reset; })
        .to("Idle");
    builder.from("Done")
        .predicate([](const EventData& e) { return e.kind == EventKind::Reset; })
        .to("Idle");
    machine.setInitialState("Idle");
    return machine;
}

State fsmgineState(const FSM<EventData>& machine) {
    std::string_view name = machine.getCurrentState();
    for (std::size_t i = 0; i < kStateNames.size(); ++i) {
        if (name == kStateNames.at(i)) {
            return static_cast<State>(i);
        }
    }
    throw std::logic_error("unknown FSMgine state: " + std::string(name));
}

// --- (b) hand-rolled switch: enum State in a switch, the compile-time baseline ---

struct SwitchMachine {
    State state = State::Idle;
};

bool switchStep(SwitchMachine& machine, const EventData& event) {
    if (event.kind == EventKind::Reset) {
        machine.state = State::Idle;
        return true;
    }
    switch (machine.state) {
    case State::Idle:
        if (event.kind == EventKind::Start) {
            machine.state = State::Running;
            return true;
        }
        break;
    case State::Running:
        if (event.kind == EventKind::Pause) {
            machine.state = State::Paused;
            return true;
        }
        if (event.kind == EventKind::Finish && guardProgressAtLeast50(event)) {
            machine.state = State::Done;
            return true;
        }
        break;
    case State::Paused:
        if (event.kind == EventKind::Resume) {
            machine.state = State::Running;
            return true;
        }
        break;
    case State::Done:
        break;
    }
    return false;
}

// --- (c) hand-rolled runtime table: small ints, no strings, no std::function ---

struct TableEntry {
    State from = State::Idle;
    EventKind event = EventKind::Start;
    int guard = 0; // 0 = none, 1 = progress >= 50
    State to = State::Idle;
};

constexpr std::array<TableEntry, 8> kTable{{
    {State::Idle, EventKind::Start, 0, State::Running},
    {State::Running, EventKind::Pause, 0, State::Paused},
    {State::Paused, EventKind::Resume, 0, State::Running},
    {State::Running, EventKind::Finish, 1, State::Done},
    {State::Idle, EventKind::Reset, 0, State::Idle},
    {State::Running, EventKind::Reset, 0, State::Idle},
    {State::Paused, EventKind::Reset, 0, State::Idle},
    {State::Done, EventKind::Reset, 0, State::Idle},
}};

struct TableMachine {
    State state = State::Idle;
};

bool tableGuardPasses(int guard, const EventData& event) {
    if (guard == 1) {
        return guardProgressAtLeast50(event);
    }
    return true;
}

bool tableStep(TableMachine& machine, const EventData& event) {
    for (const auto& entry : kTable) {
        if (entry.from == machine.state && entry.event == event.kind
            && tableGuardPasses(entry.guard, event)) {
            machine.state = entry.to;
            return true;
        }
    }
    return false;
}

// --- (d) compiled: fsmgine::compiled::Machine<State, EventData>, the SAME machine ---

fsmgine::compiled::Machine<State, EventData> buildCompiledMachine() {
    fsmgine::compiled::Machine<State, EventData> machine{&EventData::kind};
    machine.from(State::Idle).when(EventKind::Start).to(State::Running);
    machine.from(State::Running).when(EventKind::Pause).to(State::Paused);
    machine.from(State::Paused).when(EventKind::Resume).to(State::Running);
    machine.from(State::Running)
        .when(EventKind::Finish, fsmgine::compiled::ge(&EventData::progress, 50))
        .to(State::Done);
    machine.from(State::Idle).when(EventKind::Reset).to(State::Idle);
    machine.from(State::Running).when(EventKind::Reset).to(State::Idle);
    machine.from(State::Paused).when(EventKind::Reset).to(State::Idle);
    machine.from(State::Done).when(EventKind::Reset).to(State::Idle);
    machine.setInitialState(State::Idle);
    return machine;
}

// --- Deliverable 2: correctness check, run before any timing ---

std::vector<State> runFsmgineScript() {
    auto machine = buildFsmgineMachine();
    std::vector<State> states;
    states.reserve(kScript.size());
    for (const auto& event : kScript) {
        machine.process(event);
        states.push_back(fsmgineState(machine));
    }
    return states;
}

std::vector<State> runSwitchScript() {
    SwitchMachine machine;
    std::vector<State> states;
    states.reserve(kScript.size());
    for (const auto& event : kScript) {
        switchStep(machine, event);
        states.push_back(machine.state);
    }
    return states;
}

std::vector<State> runTableScript() {
    TableMachine machine;
    std::vector<State> states;
    states.reserve(kScript.size());
    for (const auto& event : kScript) {
        tableStep(machine, event);
        states.push_back(machine.state);
    }
    return states;
}

std::vector<State> runCompiledScript() {
    auto machine = buildCompiledMachine();
    std::vector<State> states;
    states.reserve(kScript.size());
    for (const auto& event : kScript) {
        machine.process(event);
        states.push_back(machine.getCurrentState());
    }
    return states;
}

void printSequence(std::ostream& out, const std::vector<State>& states) {
    for (std::size_t i = 0; i < states.size(); ++i) {
        if (i != 0) {
            out << " -> ";
        }
        out << stateName(states.at(i));
    }
    out << '\n';
}

bool checkCorrectness() {
    auto fsmgineStates = runFsmgineScript();
    auto switchStates = runSwitchScript();
    auto tableStates = runTableScript();
    auto compiledStates = runCompiledScript();

    if (fsmgineStates == switchStates && switchStates == tableStates
        && tableStates == compiledStates) {
        return true;
    }

    std::cerr << "FATAL: the four implementations disagree on the state sequence — a "
                 "comparison between different behaviours is meaningless.\n";
    std::cerr << "  FSMgine:  ";
    printSequence(std::cerr, fsmgineStates);
    std::cerr << "  switch:   ";
    printSequence(std::cerr, switchStates);
    std::cerr << "  table:    ";
    printSequence(std::cerr, tableStates);
    std::cerr << "  compiled: ";
    printSequence(std::cerr, compiledStates);
    return false;
}

// --- Deliverable 3: timing harness ---

class Timer {
public:
    void start() { start_ = std::chrono::steady_clock::now(); }
    double elapsedNs() const {
        auto end = std::chrono::steady_clock::now();
        return std::chrono::duration<double, std::nano>(end - start_).count();
    }

private:
    std::chrono::steady_clock::time_point start_{};
};

double median(std::vector<double> samples) {
    std::sort(samples.begin(), samples.end());
    return samples.at(samples.size() / 2);
}

struct ConstructionResult {
    double microsPerBuild = 0.0;
    std::uint64_t checksum = 0;
};

// Builds a fresh machine `itersPerTrial` times, `trials` times over, and reports the
// MEDIAN per-build cost — never best-of, so one lucky trial cannot flatter a result.
// checksumFn's return value is accumulated so the builds cannot be optimised away.
template <typename BuildFn, typename ChecksumFn>
ConstructionResult benchmarkConstruction(BuildFn buildFn, ChecksumFn checksumFn, int itersPerTrial,
                                         int trials) {
    std::vector<double> samples;
    samples.reserve(static_cast<std::size_t>(trials));
    std::uint64_t checksum = 0;
    Timer timer;
    for (int t = 0; t < trials; ++t) {
        timer.start();
        for (int i = 0; i < itersPerTrial; ++i) {
            auto machine = buildFn();
            checksum += checksumFn(machine);
        }
        double micros = timer.elapsedNs() / 1000.0 / static_cast<double>(itersPerTrial);
        samples.push_back(micros);
    }
    return {median(std::move(samples)), checksum};
}

struct ProcessingResult {
    double nsPerEvent = 0.0;
    std::uint64_t checksum = 0;
};

// Replays the fixed script through an already-built machine `passesPerTrial` times,
// `trials` times over, and reports the MEDIAN nanoseconds per event. Construction is
// not part of this loop — it is measured separately by benchmarkConstruction.
template <typename Machine, typename StepFn>
ProcessingResult benchmarkProcessing(Machine& machine, StepFn stepFn, int passesPerTrial,
                                     int trials) {
    std::vector<double> samples;
    samples.reserve(static_cast<std::size_t>(trials));
    std::uint64_t checksum = 0;
    Timer timer;
    for (int t = 0; t < trials; ++t) {
        timer.start();
        for (int p = 0; p < passesPerTrial; ++p) {
            for (const auto& event : kScript) {
                checksum += stepFn(machine, event) ? 1 : 0;
            }
        }
        double totalEvents
            = static_cast<double>(passesPerTrial) * static_cast<double>(kScript.size());
        samples.push_back(timer.elapsedNs() / totalEvents);
    }
    return {median(std::move(samples)), checksum};
}

// --- Deliverable 3: system info the markdown table is reported against ---

std::string compilerInfo() {
#if defined(FSMGINE_BENCH_COMPILER_ID) && defined(FSMGINE_BENCH_COMPILER_VERSION)
    return std::string(FSMGINE_BENCH_COMPILER_ID) + " " + FSMGINE_BENCH_COMPILER_VERSION;
#else
    return "unknown compiler";
#endif
}

std::string optimizationFlags() {
#if defined(FSMGINE_BENCH_CXX_FLAGS)
    std::string flags = FSMGINE_BENCH_CXX_FLAGS;
    auto first = flags.find_first_not_of(' ');
    if (first != std::string::npos) {
        return flags.substr(first);
    }
#endif
    return "(CMAKE_BUILD_TYPE not set)";
}

std::string cpuModel() {
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        auto pos = line.find("model name");
        if (pos == std::string::npos) {
            continue;
        }
        auto colon = line.find(':', pos);
        if (colon == std::string::npos) {
            continue;
        }
        std::string model = line.substr(colon + 1);
        auto first = model.find_first_not_of(" \t");
        if (first != std::string::npos) {
            return model.substr(first);
        }
    }
    return "unknown CPU";
}

std::string todayDate() {
    std::time_t now = std::time(nullptr);
    std::tm tmValue{};
    localtime_r(&now, &tmValue);
    std::ostringstream oss;
    oss << std::put_time(&tmValue, "%Y-%m-%d");
    return oss.str();
}

// --- Report ---

struct BenchRow {
    std::string name;
    double nsPerEvent = 0.0;
    double constructionMicros = 0.0;
    std::size_t bytesPerMachine = 0;
};

void printMarkdown(const std::vector<BenchRow>& rows, double switchNsPerEvent) {
    std::cout << "Compiler: " << compilerInfo() << " · Flags: " << optimizationFlags()
              << " · CPU: " << cpuModel() << " · Date: " << todayDate() << "\n\n";
    std::cout << "| implementation | ns per event | construction (µs) | bytes per "
                 "machine | ratio vs switch |\n";
    std::cout << "|---|---|---|---|---|\n";
    for (const auto& row : rows) {
        std::cout << "| " << row.name << " | " << std::fixed << std::setprecision(2)
                  << row.nsPerEvent << " | " << row.constructionMicros << " | "
                  << row.bytesPerMachine << " | " << std::setprecision(2)
                  << (row.nsPerEvent / switchNsPerEvent) << " |\n";
    }
}

void printPlain(const std::vector<BenchRow>& rows, double switchNsPerEvent) {
    std::cout << std::fixed << std::setprecision(2);
    for (const auto& row : rows) {
        std::cout << row.name << ":\n"
                  << "  " << row.nsPerEvent << " ns/event\n"
                  << "  " << row.constructionMicros << " us/construction\n"
                  << "  " << row.bytesPerMachine << " bytes/machine\n"
                  << "  " << (row.nsPerEvent / switchNsPerEvent) << "x switch baseline\n";
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        bool markdown = false;
        for (int i = 1; i < argc; ++i) {
            // C++17 has no std::span; argv must be indexed as a pointer here.
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            if (std::string_view(argv[i]) == "--markdown") {
                markdown = true;
            }
        }

        if (!markdown) {
            std::cout << "FSMgine vs hand-rolled: correctness check\n";
        }
        if (!checkCorrectness()) {
            return 1;
        }
        if (!markdown) {
            std::cout << "  all three implementations agree on the state sequence\n\n";
        }

        constexpr int kConstructionItersPerTrial = 2000;
        constexpr int kConstructionTrials = 7;
        constexpr int kProcessingPassesPerTrial = 20000;
        constexpr int kProcessingTrials = 7;

        auto fsmgineConstruction = benchmarkConstruction(
            []() { return buildFsmgineMachine(); },
            [](const FSM<EventData>& m) { return static_cast<std::uint64_t>(fsmgineState(m)); },
            kConstructionItersPerTrial, kConstructionTrials);
        auto switchConstruction = benchmarkConstruction(
            []() { return SwitchMachine{}; },
            [](const SwitchMachine& m) { return static_cast<std::uint64_t>(m.state); },
            kConstructionItersPerTrial, kConstructionTrials);
        auto tableConstruction = benchmarkConstruction(
            []() { return TableMachine{}; },
            [](const TableMachine& m) { return static_cast<std::uint64_t>(m.state); },
            kConstructionItersPerTrial, kConstructionTrials);
        auto compiledConstruction
            = benchmarkConstruction([]() { return buildCompiledMachine(); },
                                    [](const fsmgine::compiled::Machine<State, EventData>& m) {
                                        return static_cast<std::uint64_t>(m.getCurrentState());
                                    },
                                    kConstructionItersPerTrial, kConstructionTrials);

        auto fsmgineMachine = buildFsmgineMachine();
        auto fsmgineProcessing = benchmarkProcessing(
            fsmgineMachine, [](FSM<EventData>& m, const EventData& e) { return m.process(e); },
            kProcessingPassesPerTrial, kProcessingTrials);

        SwitchMachine switchMachine;
        auto switchProcessing = benchmarkProcessing(switchMachine, switchStep,
                                                    kProcessingPassesPerTrial, kProcessingTrials);

        TableMachine tableMachine;
        auto tableProcessing = benchmarkProcessing(tableMachine, tableStep,
                                                   kProcessingPassesPerTrial, kProcessingTrials);

        auto compiledMachine = buildCompiledMachine();
        auto compiledProcessing = benchmarkProcessing(
            compiledMachine,
            [](fsmgine::compiled::Machine<State, EventData>& m, const EventData& e) {
                return m.process(e);
            },
            kProcessingPassesPerTrial, kProcessingTrials);

        std::vector<BenchRow> rows{
            {"FSMgine", fsmgineProcessing.nsPerEvent, fsmgineConstruction.microsPerBuild,
             sizeof(FSM<EventData>)},
            {"hand-rolled switch", switchProcessing.nsPerEvent, switchConstruction.microsPerBuild,
             sizeof(SwitchMachine)},
            {"hand-rolled table", tableProcessing.nsPerEvent, tableConstruction.microsPerBuild,
             sizeof(TableMachine)},
            {"compiled", compiledProcessing.nsPerEvent, compiledConstruction.microsPerBuild,
             sizeof(fsmgine::compiled::Machine<State, EventData>)},
        };

        if (markdown) {
            printMarkdown(rows, switchProcessing.nsPerEvent);
            std::uint64_t totalChecksum = fsmgineConstruction.checksum + switchConstruction.checksum
                                          + tableConstruction.checksum
                                          + compiledConstruction.checksum
                                          + fsmgineProcessing.checksum + switchProcessing.checksum
                                          + tableProcessing.checksum + compiledProcessing.checksum;
            std::cout << "\n<!-- checksum: " << totalChecksum
                      << " (accumulated return values; prevents dead-code elimination) -->\n";
        } else {
            printPlain(rows, switchProcessing.nsPerEvent);
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "bench_comparison: " << e.what() << '\n';
        return 1;
    }
}
