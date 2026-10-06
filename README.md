# FSMgine

A modern C++ library for building robust finite state machines with a fluent builder interface, thread-safety support, and memory-efficient string interning.

## Features

- **Fluent Builder API**: Type-safe, self-documenting interface for FSM construction
- **Thread Safety**: Optional multi-threaded support via separate library variant
- **Memory Efficient**: String interning reduces memory footprint and improves performance
- **RAII Design**: Move-only semantics and clear ownership models
- **Flexible Architecture**: No event loop management - integrates into existing applications
- **Dual Library Variants**: Separate single-threaded and multi-threaded libraries

## Library Architecture

FSMgine ships two library variants that share one API but differ in locking:

- **`libFSMgine`** — single-threaded; no synchronization overhead, no pthread.
- **`libFSMgineMT`** — multi-threaded; mutex-protected operations at the cost of synchronization overhead.

Both contain the core FSM functionality and the `StringInterner` singleton; only the MT variant adds synchronization. Link the variant that matches your threading requirements.

## Installation

```bash
git clone https://github.com/HarryPehkonen/FSMgine.git
cd FSMgine
mkdir build && cd build

# Configure (both variants by default; or pick one)
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
cmake .. -DFSMGINE_BUILD_SINGLETHREADED=ON -DFSMGINE_BUILD_MULTITHREADED=OFF
cmake .. -DFSMGINE_BUILD_SINGLETHREADED=OFF -DFSMGINE_BUILD_MULTITHREADED=ON

make
sudo make install
```

This installs `libFSMgine.{a,so}` and `libFSMgineMT.{a,so}` under `/usr/local/lib`, the shared headers under `/usr/local/include/FSMgine/`, and the CMake package configs under `/usr/local/lib/cmake/FSMgine/` and `/usr/local/lib/cmake/FSMgineMT/`.

## Quick Start

The simplest way to use FSMgine is with an event-less FSM, where state transitions are controlled by external variables.

```cpp
#include "FSMgine/FSMgine.hpp"
#include <iostream>
using namespace fsmgine;

FSM turnstile;                 // an event-less FSM
bool coin_inserted = false;
bool door_pushed = false;

turnstile.get_builder()
    .onEnter("LOCKED", [](const std::monostate& event) { std::cout << "🔒 Locked\n"; })
    .onEnter("UNLOCKED", [](const std::monostate& event) { std::cout << "🔓 Unlocked\n"; })
    .from("LOCKED")
    .predicate([&](const std::monostate& event) { return coin_inserted; })
    .action([&](const std::monostate& event) { coin_inserted = false; })
    .to("UNLOCKED");

turnstile.get_builder()
    .from("UNLOCKED")
    .predicate([&](const std::monostate& event) { return door_pushed; })
    .action([&](const std::monostate& event) { door_pushed = false; })
    .to("LOCKED");

// Run the FSM
turnstile.setInitialState("LOCKED");

// External variables drive the transitions
coin_inserted = true;
turnstile.process(); // Transitions to UNLOCKED
door_pushed = true;
turnstile.process(); // Transitions back to LOCKED
```

## Quick Start (Event-Driven)

For a more robust and scalable design, define specific events to drive the FSM. This avoids managing external state variables and is the recommended approach for most applications.

```cpp
#include "FSMgine/FSMgine.hpp"
#include <iostream>

using namespace fsmgine;

// 1. Define events that can drive the FSM
enum class TurnstileEvent { COIN_INSERTED, DOOR_PUSHED };

int main() {
    FSM<TurnstileEvent> turnstile;   // 2. an FSM that handles these events

    // 3. Build with predicates that check the event
    turnstile.get_builder()
        .onEnter("LOCKED", [](const TurnstileEvent& triggeringEvent){ std::cout << "🔒 Locked\n"; })
        .onEnter("UNLOCKED", [](const TurnstileEvent& triggeringEvent){ std::cout << "🔓 Unlocked\n"; });
    turnstile.get_builder()
        .from("LOCKED")
        .predicate([](const TurnstileEvent& e) { return e == TurnstileEvent::COIN_INSERTED; })
        .to("UNLOCKED");
    turnstile.get_builder()
        .from("UNLOCKED")
        .predicate([](const TurnstileEvent& e) { return e == TurnstileEvent::DOOR_PUSHED; })
        .to("LOCKED");

    // 4. Run the FSM by processing events
    turnstile.setInitialState("LOCKED");
    turnstile.process(TurnstileEvent::COIN_INSERTED); // Transitions to UNLOCKED
    turnstile.process(TurnstileEvent::DOOR_PUSHED);   // Transitions back to LOCKED
}
```

## Usage Patterns

### Prefer `using fsmgine::FSM;` Over `using namespace fsmgine;`

No header injects `fsmgine::` names into the global namespace — the umbrella `FSMgine.hpp` only defines the alias `namespace fsm = fsmgine;`. A `using namespace fsmgine;` can therefore make one of your own names ambiguous: declare your own `Transition` and the compiler reports "reference to `Transition` is ambiguous", naming both candidates rather than which one you meant. Name only what you use:

```cpp
using fsmgine::FSM;
using fsmgine::EventlessFSM;
```

A forgotten template argument is legal: `FSM machine;` compiles, because `TEvent` defaults to `std::monostate` and CTAD succeeds, giving `FSM<std::monostate>`, the same type as `EventlessFSM` — a forgotten argument then surfaces as a missing `process(const TEvent&)` overload, not a deduction error.

### Encapsulating the FSM in a Class

For larger applications, encapsulate the FSM and its related state in a class to expose a clean public API and hide implementation details.

```cpp
using namespace fsmgine;

class Turnstile {
public:
    Turnstile() {
        // Lambdas capture `this` to reach the member variables
        fsm_.get_builder().from("LOCKED")
            .predicate([this](const std::monostate&) { return coin_inserted_; })
            .action([this](const std::monostate&) { coin_inserted_ = false; })
            .to("UNLOCKED");
        fsm_.get_builder().from("UNLOCKED")
            .predicate([this](const std::monostate&) { return door_pushed_; })
            .action([this](const std::monostate&) { door_pushed_ = false; })
            .to("LOCKED");
        fsm_.setInitialState("LOCKED");
    }
    void insertCoin() { coin_inserted_ = true; }   // the class owns the inputs
    void pushDoor()   { door_pushed_ = true; }
    void update()     { fsm_.process(); }          // the main loop ticks the FSM
private:
    FSM<> fsm_;
    bool coin_inserted_ = false;
    bool door_pushed_ = false;
};
```

### Transitions Without Predicates

If a transition should always occur, omit the predicate:

```cpp
fsm.get_builder()
    .from("STATE_A")
    .action([](const EventType& e) { /* always execute this */ })
    .to("STATE_B");
```

A transition with no predicate is **unconditional**: while the machine is in `STATE_A` it fires on *any* event, not merely on the events your other guards reject. That is a real feature (`Transition::predicatesPass()` returns true for an empty predicate list), so write it deliberately, and give it the last position among that state's transitions, because the **first matching transition wins**.

### One Transition Per Chain: `to()` Ends It

`to()` is the commit point: it moves the transition you have just built into the machine, and returns `void` on purpose, so a chain continued after it does not compile (`invalid use of void`) instead of quietly doing the wrong thing. The builder is spent at `to()`, but the `FSMBuilder` itself is not consumed — start the next transition with a fresh `get_builder()` or call `from()` again on the same builder:

```cpp
auto builder = fsm.get_builder();
builder.from("STATE_A").predicate([](const EventType& e) { return e.type == "x"; }).to("STATE_B");
builder.from("STATE_B").predicate([](const EventType& e) { return e.type == "y"; }).to("STATE_A");
```

Reusing a spent `TransitionBuilder` is a programming error: after `to()` it holds an empty transition with no guard, so a second `to()` would register a **catch-all**, and calling `predicate()` or `action()` after `to()` is equally wrong. Both throw `FSMBuildError`; `TransitionBuilder::to()` documents this, and the tests pin it.

## State Management

FSMgine provides two methods for setting the current state:

- **`setInitialState(state)`**: first-time initialization. Sets the current state and runs any `onEnter` actions for it; call it once after building the FSM.
- **`setCurrentState(state)`**: runtime state changes outside normal transitions. Runs `onExit` actions for the current state (if any) and `onEnter` actions for the new one; useful for reset or error recovery.

## String Interning and Memory

Every state name is interned: the FSM stores a `std::string_view` into an append-only, process-global arena rather than copying text, which turns name comparisons into pointer comparisons and makes repeated names free. Two consequences matter before you ship:

- **A returned `string_view` stays valid for the interner's lifetime.** That is the whole point of the design, and it is why `intern()` is safe to call from anywhere.
- **The arena is append-only.** It retains every *distinct* name it has ever been given; that memory is not returned until you release it. A long-running process that interns a large number of unique names — a code generator, a batch job, a fuzzer — will grow with it.

`StringInterner::resetArena()` releases the arena and **invalidates every view the interner ever handed out**. Call it between independent workloads, when nothing holds an outstanding view:

```cpp
using namespace fsmgine;   // each example stands on its own

auto& interner = StringInterner::instance();
// ... work with interned names ...
interner.resetArena();   // arena released; every previous view is now dangling
```

`StringInterner::arenaSize()` reports how many strings the arena currently retains — useful for diagnostics and tests. There is no per-name release: memory is released either not at all or entirely, by `resetArena()`, and nothing releases it for you before program exit. Intern a bounded vocabulary (state names are normally a small fixed set) and treat unbounded name generation as the case that needs `resetArena()` at a quiet point, with machines destroyed first.

## Compiled Back End

`fsmgine::FSM<TEvent>` (the interpreted back end, everything above this section) is built for machines that are **defined at run time**: states are strings, guards are `std::function` closures, and actions can capture whatever context they need. That flexibility has a cost — see the benchmark table below.

`fsmgine::compiled::Machine<State, Event>` is a second, additive back end for the opposite case: **the machine is known when you write the code**. States are a user-defined `enum`, a transition is a row of plain data (two enums, a bool, a comparison op and an int), and `process()` is a linear scan of that table with no string hashing and no `std::function` call on the hot path. The table is plain data, so it can be written by hand or emitted by a generator: [FSMTable](https://github.com/HarryPehkonen/FSMTable) reads a small text format and writes exactly this table — including entry and exit actions, composed into the generated functions — with the generated machines' tests alongside it.

```cpp
#include <FSMgine/compiled/Machine.hpp>
#include <cstdint>
#include <iostream>

enum class State : std::uint8_t { Locked, Unlocked };
enum class EventKind : std::uint8_t { CoinInserted, DoorPushed };
struct TurnstileEvent {
    EventKind kind;
};

int main() {
    fsmgine::compiled::Machine<State, TurnstileEvent> turnstile{&TurnstileEvent::kind};
    turnstile.from(State::Locked).when(EventKind::CoinInserted).to(State::Unlocked);
    turnstile.from(State::Unlocked).when(EventKind::DoorPushed).to(State::Locked);
    turnstile.setInitialState(State::Locked);

    turnstile.process(TurnstileEvent{EventKind::CoinInserted}); // -> Unlocked
    std::cout << (turnstile.getCurrentState() == State::Unlocked) << '\n';
    return 0;
}
```

See `examples/compiled_machine.cpp` for the same machine built on both back ends, driven by one shared event script and asserted to agree at every step. `#include <FSMgine/compiled/Machine.hpp>` is a header the umbrella `FSMgine.hpp` **deliberately does not include**, so existing users of the interpreted back end pay nothing for a feature they never asked for.

### Four limits, plainly (v1)

- **Actions are plain function pointers, and cannot capture.** `Action<Event>` is `void (*)(const Event&)`, not a `std::function` — context an action needs must come from a file-scope object. An action that writes to a global reintroduces a data race the library cannot protect it from; that is the caller's responsibility.
- **One refined field per machine.** A `Transition` compares at most one `int` member of `Event` (via `eq()`/`lt()`/`le()`/`gt()`/`ge()`), and every refined transition in a given machine must refine the *same* member — a second `when(kind, refinement)` on a different member throws `CompiledMachineError`.
- **One action per transition.** A second `.action(...)` call on the same transition, before `.to()`, throws `CompiledMachineError` rather than silently replacing the first.
- **The event needs a member named `kind`.** Its type is read via `decltype(Event::kind)` and becomes the machine's `EventKind` — a naming convention, not a template parameter, because C++17 cannot deduce a third class-template parameter from the two-argument `Machine<State, Event>` the constructor is written against.

Richer per-transition logic — multiple guarded fields, captured state, arbitrary predicates — belongs on `fsmgine::FSM<TEvent>` instead.

### Thread safety

A `compiled::Machine` owns per-instance mutable state (`currentState_`) written by `process()`, `setInitialState()` and `setCurrentState()`, and it is not synchronized (there is no `compiled::MachineMT`). **One machine must not be driven from two threads at once**; build one machine per thread. Because an action cannot capture, the examples reach caller state through a file-scope pointer installed for one event; that pointer is **per-thread global, not per-machine**, so install → process → clear with nothing in between is the contract, not an accident of the examples.

What it does remove is `StringInterner`'s process-global state: nothing is interned, no string is hashed and no `std::function` is called, so machines on different threads never contend — unlike `fsmgine::FSM`, which serializes access to that shared interner, with `FSMGINE_MULTI_THREADED` gating the locking. An action that writes to a global reintroduces a race the machine cannot fix.

### Actions run before the state change

Exactly like the interpreted back end: a firing transition's action runs **before** `currentState_` is updated, so an action can still observe the state it is leaving via `getCurrentState()`.

## Example Use Cases

Common problems solved with FSMgine.

### 1. Resource Pool Management
A thread-safe resource pool: the FSM models the pool state (`IDLE`, `BUSY`, `EMPTY`) and its actions modify an `std::atomic<int>` counter for available resources. Link against `FSMGineMT` for thread-safe FSM operations combined with atomics.

### 2. Protocol Parser
Parse a simple network protocol string: the FSM steps character by character through states like `WAITING_HEADER`, `READING_PAYLOAD`, and `VALIDATING`, with actions appending to buffer strings (`current_command`, `current_param`). Ideal for stream processing and validation.
- **Generated from a text format:** [FSMTable](https://github.com/HarryPehkonen/FSMTable) builds a protocol machine of this kind onto the compiled back end — a connection lifecycle with timeouts, retransmission and a sink state — with 12 unit tests and a recorded trace in `examples/protocol/`.

### 3. Calculator Implementation
A calculator built from two FSMs: one for tokenizing the input string, one for parsing and evaluating the expression.
- **Pattern:** A two-stage design. The tokenizer FSM produces a stream of `Token` objects, fed as events into the parser FSM, whose actions manipulate value and operator stacks to maintain context between states.
- **Generated from a text format:** this two-stage shape, written as text and generated onto the compiled back end, with its own tests and a recorded session, is [FSMTable](https://github.com/HarryPehkonen/FSMTable)'s `examples/calculator/`.

### 4. Parentheses Checker
Validate balanced parentheses in a string.
- **Pattern:** An FSM processes the input character by character. An `onEnter` action pushes opening parentheses onto a `std::stack`, while other transitions pop and validate closing ones.

## Integration

### CMake Integration

FSMgine provides two library variants, used according to your threading requirements:

```cmake
cmake_minimum_required(VERSION 3.20)
project(your_project)

find_package(FSMgine REQUIRED)                    # multi-threaded: FSMgineMT
add_executable(your_project src/main.cpp)
target_link_libraries(your_project PRIVATE FSMgine::FSMgine)   # multi-threaded: FSMgine::FSMgineMT
```

The target automatically links the appropriate static library, includes the necessary headers, links `Threads::Threads` (FSMgineMT only) and sets the `FSMGINE_MULTI_THREADED` compile definition (FSMgineMT only).

### Build Options

- `-DFSMGINE_BUILD_SINGLETHREADED=ON`: Build single-threaded library (default: ON)
- `-DFSMGINE_BUILD_MULTITHREADED=ON`: Build multi-threaded library (default: ON)
- `-DTEST_MULTITHREADED=ON`: Run tests with multi-threaded library (default: matches FSMGINE_BUILD_MULTITHREADED)
- `-DEXAMPLES_USE_MULTITHREADED=ON`: Build examples with multi-threaded library (default: matches FSMGINE_BUILD_MULTITHREADED)
- `-DBUILD_TESTING=OFF`: Skip building tests
- `-DBUILD_EXAMPLES=ON`: Build example programs
- `-DBUILD_DOCUMENTATION=ON`: Enable documentation generation target
- `-DFSMGINE_BUILD_FUZZING=ON`: Build the libFuzzer targets (requires clang; default: OFF)

## Documentation

FSMgine uses Doxygen for API documentation, deployed to GitHub Pages on every push to the main branch. Browse the latest at the [FSMgine Documentation](https://harrypehkonen.github.io/FSMgine/), or build it locally:

```bash
# Install Doxygen (if not already installed)
sudo apt-get install doxygen graphviz  # Ubuntu/Debian
brew install doxygen graphviz          # macOS

# Configure with documentation enabled
cmake .. -DBUILD_DOCUMENTATION=ON

# Build documentation
make docs

# Open in browser
open docs/html/index.html              # On macOS
xdg-open docs/html/index.html          # On Linux
```

The generated documentation includes the complete API reference, usage examples, module organization, and class diagrams.

See [PERFORMANCE.md](PERFORMANCE.md) for the profiling workflow and the recorded performance results.

## Testing and Fuzzing

```bash
cmake -B build -DBUILD_TESTING=ON && cmake --build build
ctest --test-dir build
```

libFuzzer targets (clang only) live behind `-DFSMGINE_BUILD_FUZZING=ON`:

```bash
cmake -B build-fuzz -DFSMGINE_BUILD_FUZZING=ON -DBUILD_TESTING=OFF \
      -DCMAKE_CXX_COMPILER=clang++
cmake --build build-fuzz --target fuzz_fsmgine retention_check
./build-fuzz/fuzz_fsmgine <corpus-dir>   # stateful driver for the engine
./build-fuzz/retention_check             # asserts resident memory stays bounded
```

A fuzz target must call `StringInterner::resetArena()` between inputs, with everything holding a view destroyed first — see the "Fuzzing" section of `CLAUDE.md` for the rule and the reason.

## Troubleshooting

### Undefined references to `StringInterner`

Linker errors like:
```
undefined reference to `fsmgine::StringInterner::instance()'
undefined reference to `fsmgine::StringInterner::intern(...)'
```

mean you are not linking against the FSMgine library. Ensure you have called `find_package(FSMgine REQUIRED)` or `find_package(FSMgineMT REQUIRED)` in your CMakeLists.txt, added the appropriate target (`FSMgine::FSMgine` or `FSMgine::FSMgineMT`) to your link libraries, and installed the library (`sudo make install`).

### Thread-related linking errors

With FSMgineMT, the CMake configuration links pthread automatically; if pthread errors persist, install the pthread development headers.

### Mixing library variants

**Important:** Do not link both FSMgine and FSMgineMT in the same executable. Choose `FSMgine` for single-threaded applications (no synchronization overhead) or `FSMgineMT` for multi-threaded applications.

### FSMgine package not found

If CMake cannot find the package, ensure the libraries are installed (`sudo make install`), that you built the variant you are using, and that the installation prefix matches your CMake search paths — otherwise pass `PATHS /path/to/fsmgine/install` to `find_package`.

## When not to use FSMgine

`benchmarks/bench_comparison.cpp` times four implementations of the same five-transition machine (Idle/Running/Paused/Done, with a guarded Running→Done transition) under one `<chrono>`-based harness: FSMgine itself (the interpreted back end), a hand-rolled `switch` over an `enum class` (the compile-time baseline), a hand-rolled runtime table — a `std::array` of `{state, event, guard, target}` scanned linearly, with small ints instead of strings and no `std::function` — and `fsmgine::compiled::Machine` on the same states and events. All four run the same fixed, deterministic event script, and the benchmark asserts they produce the identical state sequence before timing anything. Every figure is a median over several trials, never a best-of, on one machine built and run on one laptop — indicative of where the costs come from, not a guarantee of what you'll measure on your hardware.

Two limits worth stating plainly. **"Bytes per machine" is `sizeof()` of the implementation object, not its heap footprint** — FSMgine's real memory cost also includes its state-name map, the interned-name arena and its `std::function` guards. And the hand-rolled table's construction cost is near-zero because its table is `constexpr static`, shared like a vtable; a table parsed from configuration at startup would pay more than the zero shown here.

<!-- BENCH-TABLE:BEGIN -->

Compiler: GNU 14.2.0 · Flags: -O3 -DNDEBUG · CPU: Intel(R) Core(TM) i7-3520M CPU @ 2.90GHz · Date: 2026-10-03

| implementation | ns per event | construction (µs) | bytes per machine | ratio vs switch |
|---|---|---|---|---|
| FSMgine | 34.20 | 1.35 | 80 | 12.80 |
| hand-rolled switch | 2.67 | 0.00 | 1 | 1.00 |
| hand-rolled table | 3.17 | 0.00 | 1 | 1.19 |
| compiled | 7.39 | 0.10 | 88 | 2.76 |

<!-- checksum: 5040000 (accumulated return values; prevents dead-code elimination) -->

<!-- BENCH-TABLE:END -->

Reproduce it with:

```bash
tools/update_bench_table.sh                             # rebuilds this table in place
# or, to inspect the binary directly:
cmake -B build -DBUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --target FSMgine_comparison
./build/benchmarks/FSMgine_comparison             # human-readable report
./build/benchmarks/FSMgine_comparison --markdown   # the table above
```

What the numbers suggest:

- **Hand-rolled `switch`** when the machine is fixed at compile time and the hot path matters (a five-transition machine is ten lines and needs no library), or **the hand-rolled runtime table** when machines must be defined at run time but strings and `std::function` are too expensive for the hot path.
- **`fsmgine::compiled::Machine`** (see "Compiled Back End" above) when the machine is known when you write the code and you want that same table shape — an enum state instead of an interned string, one plain function pointer per transition instead of `onEnter`/`onExit` actions, names still available through `withNames()`.
- **FSMgine** when the machine *is* data (config, plugins, user input), when string-named states help logging and introspection, or when guards and actions should be first-class values. Link the single-threaded `FSMgine` target (not `FSMgineMT`) in a hot loop — it compiles out the mutex entirely. The table itself is the evidence; read it before taking any of the above as a verdict.

## Requirements

- C++17 or later
- CMake 3.20+
- Google Test (optional, for testing)
- Threads library (required only for FSMgineMT variant)

## License

Please see the LICENSE file. This code is released to the public domain. Specifics are in the file.
