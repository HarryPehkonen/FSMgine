# FSMgine Specifications

## Overview
FSMgine is a C++ library for building and managing Finite State Machines (FSMs). It provides a fluent builder interface for defining states, transitions, and actions, with robust support for live editing via a thread-safe architecture and string interning to optimize memory usage.

## Design Philosophy and Guarantees
*   **Ease of Use:** The fluent builder API is designed to be self-documenting and prevent invalid construction sequences through its type system.
*   **Safety:** The library prioritizes memory safety through RAII and clear ownership models (disabling copy, using move). Thread safety for concurrent use is a core, albeit optional, feature.
*   **Performance:** For single-threaded applications, the locking mechanisms can be compiled out entirely to eliminate overhead. String interning is used to reduce memory footprint and improve the performance of state comparisons.
*   **Flexibility:** The library does not manage its own event loop, allowing it to be integrated seamlessly into various application architectures. Defining transitions to states that do not yet have outgoing transitions is permitted to allow for flexible definition order.

## Core Components

### 1. String Interner
The `StringInterner` class provides string interning to ensure identical strings are stored only once.

```cpp
// At top of file
#ifdef FSMGINE_MULTI_THREADED
#include <mutex>
#endif

class StringInterner {
public:
    static StringInterner& instance();
    std::string_view intern(const std::string& str);
    std::string_view intern(std::string_view sv);

    // clear() is for testing purposes only and is not thread-safe.
    void clear();
};
```
*   **Design Choice:** The `StringInterner` is implemented as a singleton for global accessibility and ease of use, avoiding the need to pass an interner instance through the entire object graph. This is a deliberate trade-off for convenience over strict dependency injection.
*   **Thread Safety:** If `FSMGINE_MULTI_THREADED` is defined, `intern()` will use a `std::mutex` to ensure safe concurrent access. Otherwise, a dummy lock is used for maximum single-threaded performance.

### 2. Transition
The `Transition` class represents a state transition with predicates and actions.

```cpp
class Transition {
public:
    using Predicate = std::function<bool()>;
    using Action = std::function<void()>;

    void addPredicate(Predicate pred);
    void addAction(Action action);
    void setTargetState(std::string_view state);
    bool evaluatePredicates() const;
    void executeActions() const;
    std::string_view getTargetState() const;
};
```

### 3. FSM
The `FSM` class is the main state machine container.

```cpp
// At top of file
#ifdef FSMGINE_MULTI_THREADED
#include <shared_mutex>
#endif

class FSM {
public:
    FSMBuilder get_builder();
    void setInitialState(std::string_view state);
    void setCurrentState(std::string_view state);
    bool step(); // Returns true if a transition occurred, false otherwise.
    std::string_view getCurrentState() const;

private:
#ifdef FSMGINE_MULTI_THREADED
    mutable std::shared_mutex m_mutex;
#endif
    // Internal state representation details in "Memory Management" section.
};
```
*   **Thread Safety:** `setInitialState` and `setCurrentState` must acquire a unique lock on the FSM's mutex if thread safety is enabled.

### 4. FSMBuilder and TransitionBuilder
The `FSMBuilder` and `TransitionBuilder` classes provide a guided, fluent interface for configuring the FSM.

```cpp
// Forward declaration
class TransitionBuilder;

class FSMBuilder {
public:
    TransitionBuilder from(const std::string& state);
    FSMBuilder& onEnter(const std::string& state, Transition::Action action);
    FSMBuilder& onExit(const std::string& state, Transition::Action action);
};

class TransitionBuilder {
public:
    TransitionBuilder& predicate(Transition::Predicate pred);
    TransitionBuilder& action(Transition::Action action);
    void to(const std::string& state); // Terminal method, returns void
};
```

## Builder Lifetime and Atomicity
The fluent interface is designed to be both safe and intuitive.

*   **Dangling Transitions:** The `TransitionBuilder` is a lightweight, temporary object returned by `FSMBuilder::from()`. Its sole purpose is to collect predicates and actions for a single transition. If the builder is created but its terminal `to()` method is never called, the `TransitionBuilder` object is simply destroyed and the transition is discarded. **This is the intended behavior**, not an error. It allows a user to programmatically decide whether to complete a transition's definition without side effects.
*   **Atomic Operations:** To ensure thread safety and minimize lock contention during live editing, write locks are acquired **only within the terminal methods** that modify the FSM's state:
    *   `TransitionBuilder::to()`
    *   `FSMBuilder::onEnter()`
    *   `FSMBuilder::onExit()`
    These methods will acquire a `std::unique_lock` on the FSM's internal mutex, perform the modification, and release the lock, ensuring the write is atomic. The non-terminal methods like `.from()` and `.predicate()` do not acquire locks.

## State Machine Construction

### Simple Transition
```cpp
FSM fsm;
fsm.get_builder()
    .from("START")
    .to("END");
```

### State Enter/Exit Actions
```cpp
fsm.get_builder()
    .onEnter("LOADING", []{ /* show spinner */ })
    .onExit("LOADING", []{ /* hide spinner */ });
```

### Transition with Predicates and Actions
```cpp
fsm.get_builder()
    .from("LOCKED")
    .predicate([]() { return coin_is_inserted; })
    .action([]() { std::cout << "Coin accepted!\n"; })
    .to("UNLOCKED");
```

### Multiple Transitions
```cpp
FSM fsm;
auto builder = fsm.get_builder();

builder.from("START")
    .predicate([]() { return is_ready; })
    .to("MIDDLE");

builder.from("MIDDLE")
    .to("END");
```

## State Machine Operation

### Setting Initial State
```cpp
fsm.setInitialState("START");
```

### Taking Steps
The `step()` method attempts to find and execute a valid transition from the current state.
```cpp
bool transitioned = fsm.step(); // Returns true if a state change occurred
```

### Ambiguity Resolution
If multiple transitions from the current state have all their predicates evaluate to `true`, the **first one that was defined** will be executed. All others will be ignored for that step.

### Event-Driven Usage Pattern (Turnstile Example)
```cpp
enum class TurnstileEvent { PUSH, COIN };
TurnstileEvent current_event;

FSM turnstile;
auto builder = turnstile.get_builder();

builder.from("LOCKED")
    .predicate([&] { return current_event == TurnstileEvent::COIN; })
    .to("UNLOCKED");

builder.from("UNLOCKED")
    .predicate([&] { return current_event == TurnstileEvent::PUSH; })
    .to("LOCKED");

turnstile.setInitialState("LOCKED");

// Main application loop
while (true) {
    current_event = get_user_input();
    turnstile.step();
}
```

## Concurrency: Live Editing
FSMgine supports "live editing," meaning the FSM's structure can be modified while it is being used in other threads.

*   **FSM Lock:** The `FSM` class contains a `std::shared_mutex`.
    *   **Read Operations (`step`, `getCurrentState`):** Acquire a `std::shared_lock`, allowing concurrent execution.
    *   **Write Operations (`to`, `onEnter`, `onExit`, `setInitialState`, `setCurrentState`):** Acquire a `std::unique_lock`, blocking all other readers and writers.
*   **Preprocessor Flag:** All locking behavior is enabled by defining `FSMGINE_MULTI_THREADED`.

## Error Handling and Validation
*   **Exceptions:** Custom exceptions like `fsm::invalid_argument` and `fsm::runtime_error` are used for clear error reporting.
*   **Undefined States:** Defining a transition `to()` a state that has not been defined elsewhere (e.g., in a `from()` or `onEnter()`) is **not considered an error**. This provides flexibility, allowing users to define their FSMs in any order. The only strict validation occurs at runtime (e.g., calling `setInitialState` with a name that was never used in any definition will throw an exception).

## Memory Management and Internal Representation
*   **Move Semantics:** The `FSM` is movable but not copyable to prevent accidental deep copies and enforce clear ownership.
*   **String Interning:** All state names are interned to minimize memory footprint and optimize comparisons.
*   **State Representation:** Internally, the FSM will use a hash map to associate state names with their corresponding data. A suggested implementation is `std::unordered_map<std::string_view, StateData>`, where `StateData` is a structure containing the `onEnter`/`onExit` actions and a `std::vector` of `Transition` objects originating from that state.

## Testing Requirements
Comprehensive tests must cover:
1.  Basic state transitions and `step()` return values.
2.  `onEnter` and `onExit` action execution.
3.  Multiple predicates and actions on a single transition.
4.  Builder API sequence validation (enforced by types and behavior).
5.  String interning correctness and `string_view` handling.
6.  Move semantics for the `FSM` object.
7.  Error conditions and exception throwing for invalid runtime operations.
8.  **Concurrency:** Tests to verify thread safety of live editing and concurrent `step()` calls.
