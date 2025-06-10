# FSMgine

A modern C++ library for building robust finite state machines with a fluent builder interface, thread-safety support, and memory-efficient string interning.

## Features

- **Fluent Builder API**: Type-safe, self-documenting interface for FSM construction
- **Thread Safety**: Optional multi-threaded support with compile-time flags
- **Memory Efficient**: String interning reduces memory footprint and improves performance
- **RAII Design**: Move-only semantics and clear ownership models
- **Flexible Architecture**: No event loop management - integrates into existing applications
- **Comprehensive Testing**: 34+ unit and integration tests

## Installation

### Option 1: Install from Source

```bash
# Clone the repository
git clone https://github.com/HarryPehkonen/FSMgine.git
cd FSMgine

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local

# Build
make

# Install
sudo make install
```

### Option 2: Install Locally

If you don't want to install system-wide, you can install to a local directory:

```bash
# Clone and build as above, but specify a local install path
cmake .. -DCMAKE_INSTALL_PREFIX=/path/to/local/install
make
make install
```

### Build Options

When building FSMgine, you can customize it with these options:
- `-DFSMGINE_MULTI_THREADED=ON` - Enable thread safety
- `-DBUILD_TESTING=OFF` - Skip building tests
- `-DBUILD_EXAMPLES=ON` - Build example programs

## Using FSMgine in Your Project

### Method 1: Using CMake's find_package

1. Create a new project with the following structure:
```
your_project/
├── CMakeLists.txt
└── src/
    └── main.cpp
```

2. Create a `CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.20)
project(your_project)

# If you installed locally, specify the path
# set(FSMgine_DIR "/path/to/local/install/lib/cmake/FSMgine")

find_package(FSMgine REQUIRED)

add_executable(your_project src/main.cpp)
target_link_libraries(your_project PRIVATE FSMgine::FSMgine)
```

3. Create `src/main.cpp`:
```cpp
#include <FSMgine/FSMgine.hpp>
#include <iostream>

int main() {
    fsmgine::FSM fsm;
    
    // Build your state machine
    fsm.get_builder()
        .from("START")
        .to("END");
    
    fsm.setInitialState("START");
    fsm.step();
    
    std::cout << "Current state: " << fsm.getCurrentState() << std::endl;
    return 0;
}
```

### Method 2: Using as a Subdirectory

You can also include FSMgine directly in your project:

```cmake
cmake_minimum_required(VERSION 3.20)
project(your_project)

add_subdirectory(path/to/FSMgine)
add_executable(your_project src/main.cpp)
target_link_libraries(your_project PRIVATE FSMgine::FSMgine)
```

### Method 3: Using vcpkg (Coming Soon)

```bash
vcpkg install FSMgine
```

Then in your CMakeLists.txt:
```cmake
find_package(FSMgine CONFIG REQUIRED)
target_link_libraries(your_project PRIVATE FSMgine::FSMgine)
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
    .to("UNLOCKED")
    .from("UNLOCKED")
    .predicate([&]() { return door_pushed; })
    .action([&]() { door_pushed = false; })
    .to("LOCKED");

// Run the FSM
turnstile.setInitialState("LOCKED");
coin_inserted = true;
turnstile.step(); // Transitions to UNLOCKED
```

## Building

### Requirements
- C++17 or later
- CMake 3.20+
- Google Test (for testing)

### Build Instructions

```bash
# Basic build
mkdir build && cd build
cmake ..
make

# With tests
cmake .. -DGTEST_ROOT=/path/to/gtest  # if needed
make
./tests/FSMgine_tests

# With examples
cmake .. -DBUILD_EXAMPLES=ON
make
./turnstile_example

# With multi-threading support
cmake .. -DFSMGINE_MULTI_THREADED=ON
make
```

## Architecture

### Core Components

- **StringInterner**: Singleton for string optimization and memory efficiency
- **Transition**: Represents state transitions with predicates and actions  
- **FSM**: Main state machine container with thread-safe operations
- **FSMBuilder/TransitionBuilder**: Fluent interface for FSM construction

### Threading Model

FSMgine supports optional thread safety via the `FSMGINE_MULTI_THREADED` preprocessor flag:

- **Read operations** (`step()`, `getCurrentState()`): Concurrent via `std::shared_mutex`
- **Write operations** (`to()`, `onEnter()`, `onExit()`): Exclusive locks
- **Single-threaded builds**: All locking overhead compiled out

## Usage Patterns

### Basic State Machine

```cpp
FSM fsm;
fsm.get_builder()
    .from("START")
    .to("END");

fsm.setInitialState("START");
fsm.step(); // START -> END
```

### Conditional Transitions

```cpp
bool condition = false;
fsm.get_builder()
    .from("WAITING")
    .predicate([&]() { return condition; })
    .to("READY");

fsm.setInitialState("WAITING");
fsm.step(); // No transition (condition false)

condition = true;
fsm.step(); // WAITING -> READY
```

### Actions and State Callbacks

```cpp
int counter = 0;

fsm.get_builder()
    .onEnter("ACTIVE", [&]() { std::cout << "Entering active state\n"; })
    .onExit("ACTIVE", [&]() { std::cout << "Leaving active state\n"; })
    .from("ACTIVE")
    .action([&]() { counter++; })
    .to("DONE");
```

### Complex Workflows

See `examples/turnstile_example.cpp` and the integration tests for comprehensive examples including:
- Turnstile state machine with error handling
- Traffic light timing system  
- Complex workflow with retry logic

## Design Principles

- **Ease of Use**: Fluent API prevents invalid construction sequences
- **Safety**: RAII, move-only semantics, thread-safe operations
- **Performance**: Zero-overhead single-threaded mode, string interning
- **Flexibility**: No event loop constraints, flexible definition order

## API Reference

### FSM Class

```cpp
class FSM {
    FSMBuilder get_builder();
    void setInitialState(std::string_view state);
    void setCurrentState(std::string_view state);
    bool step(); // Returns true if transition occurred
    std::string_view getCurrentState() const;
};
```

### Builder Classes

```cpp
class FSMBuilder {
    TransitionBuilder from(const std::string& state);
    FSMBuilder& onEnter(const std::string& state, Action action);
    FSMBuilder& onExit(const std::string& state, Action action);
};

class TransitionBuilder {
    TransitionBuilder& predicate(Predicate pred);
    TransitionBuilder& action(Action action);
    void to(const std::string& state); // Terminal method
};
```

## Testing

Run the comprehensive test suite:

```bash
cd build
make
./tests/FSMgine_tests
```

The test suite includes:
- Unit tests for all components
- Integration tests with real-world scenarios  
- Thread safety validation
- Memory management verification

## License

This project is released into the public domain under the Unlicense. See LICENSE for details.

## Contributing

1. Follow existing code style and patterns
2. Add tests for new functionality
3. Ensure all tests pass
4. Update documentation as needed