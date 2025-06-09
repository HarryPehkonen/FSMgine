# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

FSMgine is a C++ finite state machine library with two distinct approaches:
1. **Code Generation Tool**: Processes DSL comments in C++ files to generate transition code
2. **Runtime Library**: Provides fluent builder API for dynamic FSM construction

The project is in transition - old code generation components are being replaced with a new runtime library design.

## Build System

FSMgine uses CMake with C++17 standard:

```bash
# Build the project
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make

# Install
make install
```

## Architecture

### Core Components (New Design)
- **StringInterner**: Singleton for string optimization and memory efficiency
- **FSM**: Main state machine container with thread-safe operations
- **FSMBuilder/TransitionBuilder**: Fluent interface for FSM construction
- **Transition**: Represents state transitions with predicates and actions

### Threading Model
- Compile-time threading support via `FSMGINE_MULTI_THREADED` preprocessor flag
- Uses `std::shared_mutex` for concurrent read operations (`step()`, `getCurrentState()`)
- Write operations (`to()`, `onEnter()`, `onExit()`) acquire exclusive locks
- Single-threaded builds compile out all locking overhead

### Builder Pattern Usage
```cpp
FSM fsm;
fsm.get_builder()
    .from("START")
    .predicate([]() { return condition; })
    .action([]() { /* do something */ })
    .to("END");
```

## Key Design Decisions

- **Move-only semantics**: FSM objects cannot be copied, only moved
- **String interning**: All state names are interned for memory efficiency
- **Flexible definition order**: States can be referenced before being fully defined
- **Live editing support**: FSM structure can be modified during runtime in multi-threaded scenarios
- **No event loop management**: Library integrates into existing application architectures

## Development Commands

Since the project structure is in transition, check for the existence of build files before running commands:

```bash
# Check if CMakeLists.txt exists before building
ls CMakeLists.txt

# For code generation mode (if old structure exists)
./fsmgine < input.cpp > output.cpp
```