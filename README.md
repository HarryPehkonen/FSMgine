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

```cpp
#include "FSMgine/FSMgine.hpp"
using namespace fsmgine;

// Create an FSM
FSM turnstile;
bool coin_inserted = false;
bool door_pushed = false;

// Build with fluent interface
turnstile.get_builder()
    .onEnter("LOCKED", []() { std::cout << "🔒 Locked\n"; })
    .onEnter("UNLOCKED", []() { std::cout << "🔓 Unlocked\n"; })
    .from("LOCKED")
    .predicate([&]() { return coin_inserted; })
    .action([&]() { coin_inserted = false; })
    .to("UNLOCKED");

turnstile.get_builder()
    .from("UNLOCKED")
    .predicate([&]() { return door_pushed; })
    .action([&]() { door_pushed = false; })
    .to("LOCKED");

// Run the FSM
turnstile.setInitialState("LOCKED");
coin_inserted = true;
turnstile.step(); // Transitions to UNLOCKED
```

## Example Use Cases

### 1. Resource Pool Management
The `resource_pool_example.cpp` demonstrates a thread-safe resource pool with states for managing available resources:

```cpp
class ResourcePool {
    FSM<ResourceEvent> fsm;
    std::atomic<int> available_resources;

    // States: IDLE, BUSY, EMPTY
    // Transitions handle resource acquisition and release
    // Thread-safe operations with atomic counters
};
```

### 2. Protocol Parser
The `protocol_parser.cpp` shows how to build a state machine for parsing network protocols:

```cpp
// States: WAITING_HEADER, READING_PAYLOAD, VALIDATING
// Handles protocol parsing with error recovery
// Demonstrates complex state transitions
```

### 3. Calculator Implementation
The `calculator.cpp` example implements a calculator using a state machine:

```cpp
// States: START, NUMBER, OPERATOR, RESULT
// Handles mathematical expressions
// Shows how to maintain context between states
```

### 4. Parentheses Checker
A simple but effective example in `parentheses_checker.cpp`:

```cpp
// States: BALANCED, UNBALANCED
// Validates nested parentheses
// Demonstrates minimal but complete FSM usage
```

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