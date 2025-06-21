# FSMgine

A modern C++ library for building robust finite state machines with a fluent builder interface, thread-safety support, and memory-efficient string interning.  This library was written with much support from several AI.

## Features

- **Fluent Builder API**: Type-safe, self-documenting interface for FSM construction
- **Thread Safety**: Optional multi-threaded support with compile-time flags
- **Memory Efficient**: String interning reduces memory footprint and improves performance
- **RAII Design**: Move-only semantics and clear ownership models
- **Flexible Architecture**: No event loop management - integrates into existing applications

## Installation

```bash
# Clone the repository
git clone https://github.com/HarryPehkonen/FSMgine.git
cd FSMgine

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local

# Build and install
make
sudo make install
```

## Quick Start

The simplest way to use FSMgine is with an event-less FSM, where state transitions are controlled by external variables.

```cpp
#include "FSMgine/FSMgine.hpp"
#include <iostream>

using namespace fsmgine;

// Create an event-less FSM
FSM turnstile;
bool coin_inserted = false;
bool door_pushed = false;

// Build with fluent interface
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

// Simulate external events changing the state
coin_inserted = true;
turnstile.process(); // Transitions to UNLOCKED

door_pushed = true;
turnstile.process(); // Transitions back to LOCKED
```

## Quick Start (Event-Driven)

For a more robust and scalable design, you can define specific events to drive the FSM. This avoids managing external state variables and is the recommended approach for most applications.

```cpp
#include "FSMgine/FSMgine.hpp"
#include <iostream>

using namespace fsmgine;

// 1. Define events that can drive the FSM
enum class TurnstileEvent { COIN_INSERTED, DOOR_PUSHED };

int main() {
    // 2. Create an FSM that handles these events
    FSM<TurnstileEvent> turnstile;

    // 3. Build the FSM using predicates that check the event
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

### Encapsulating the FSM in a Class

For larger applications, it's best practice to encapsulate the FSM and its related state within a class. This provides a clean public API and hides implementation details.

```cpp
class Turnstile {
public:
    Turnstile() {
        // Lambdas can capture `this` to access member variables
        fsm_.get_builder()
            .from("LOCKED")
            .predicate([this](const std::monostate& event) { return coin_inserted_; })
            .action([this](const std::monostate& event) { coin_inserted_ = false; })
            .to("UNLOCKED");

        fsm_.get_builder()
            .from("UNLOCKED")
            .predicate([this](const std::monostate& event) { return door_pushed_; })
            .action([this](const std::monostate& event) { door_pushed_ = false; })
            .to("LOCKED");

        fsm_.setInitialState("LOCKED");
    }

    // Public methods control the FSM's inputs
    void insertCoin() { coin_inserted_ = true; }
    void pushDoor() { door_pushed_ = true; }

    // The main loop calls process() to run the logic
    void update() { fsm_.process(); }

private:
    FSM<> fsm_;
    bool coin_inserted_ = false;
    bool door_pushed_ = false;
};
```

## State Management

FSMgine provides two methods for setting the current state:

- **`setInitialState(state)`**: Use this for first-time FSM initialization. It sets the current state and executes any `onEnter` actions for that state. This should be called once after building your FSM to establish the starting state.

- **`setCurrentState(state)`**: Use this for runtime state changes when you need to forcibly change the state outside of normal transitions. It executes `onExit` actions for the current state (if any) and `onEnter` actions for the new state. This is useful for reset functionality or error recovery scenarios.

## Example Use Cases

These examples demonstrate how to apply FSMgine to solve common problems. They illustrate patterns for managing state and logic within the FSM's actions and predicates.

### 1. Resource Pool Management
Demonstrates a thread-safe resource pool.
- **Pattern:** The FSM models the state of the pool (`IDLE`, `BUSY`, `EMPTY`). FSM actions modify an `std::atomic<int>` counter for available resources. This shows how to use the FSM's built-in locking (via `-DFSMGINE_MULTI_THREADED=ON`) in combination with atomic variables to manage concurrent access safely.

### 2. Protocol Parser
Shows how to build a state machine for parsing a simple network protocol string.
- **Pattern:** The FSM transitions character by character through states like `WAITING_HEADER`, `READING_PAYLOAD`, and `VALIDATING`. Actions append characters to buffer strings (`current_command`, `current_param`). This pattern is ideal for stream processing and validation tasks.

### 3. Calculator Implementation
Implements a calculator using two FSMs: one for tokenizing the input string and another for parsing and evaluating the expression.
- **Pattern:** Shows a more advanced, two-stage FSM design. The tokenizer FSM produces a stream of `Token` objects, which are then fed as events into the parser FSM. Actions in the parser manipulate stacks for values and operators, demonstrating how to maintain complex context between states.

### 4. Parentheses Checker
A simple but effective example that validates balanced parentheses in a string.
- **Pattern:** An FSM processes the input character by character. An `onEnter` action pushes opening parentheses onto a `std::stack`, while other transitions pop and validate closing parentheses. This is a minimal but complete example of using an FSM for validation logic.

## Integration

### CMake Integration

```cmake
cmake_minimum_required(VERSION 3.20)
project(your_project)

find_package(FSMgine REQUIRED)
add_executable(your_project src/main.cpp)
target_link_libraries(your_project PRIVATE FSMgine::FSMgine)
```

### Build Options

- `-DFSMGINE_MULTI_THREADED=ON`: Enable thread safety
- `-DBUILD_TESTING=OFF`: Skip building tests
- `-DBUILD_EXAMPLES=ON`: Build example programs

## Requirements

- C++17 or later
- CMake 3.20+
- Google Test (for testing)

## License

Please see the LICENSE file.  This code is released to the public domain.  Specifics are in the file.