# FSMgine
Finite State Machine Engine creates finite state machines via code generation

## Getting Started

FSMgine helps you create finite state machines in C++ through a simple DSL (Domain Specific Language). Here's a quick example:

```cpp
/* FSMgine definition: MyStateMachine
(IDLE PRED isReady ACTION doSetup READY)
(READY PRED hasEvent ACTION processEvent IDLE)
(READY ACTION enterError ERROR)
*/
#define FSM_MyStateMachine_transitions {}
```

### How it Works

1. Define your states and transitions using the FSMgine DSL inside a comment block
2. Each transition rule follows the format: `(FROM_STATE [PRED predicate]* [ACTION action]* TO_STATE)`
3. FSMgine will generate the transition code for you

### Example Implementation

Here's a complete example showing how to use FSMgine:

```cpp
#include <string>
#include <vector>
#include <functional>
#include <iostream>

// Define your Transition struct
struct Transition {
    std::string_view from_state;
    std::vector<std::function<bool()>> predicates;
    std::vector<std::function<void()>> actions;
    std::string_view to_state;
};

// Define your FSM class
class MyStateMachine {
public:
    MyStateMachine(const std::string& initial_state) {
        current_state_ = initial_state;
        transitions_ = FSM_MyStateMachine_transitions;
    }

    void step() {
        for (const auto& rule : transitions_) {
            if (rule.from_state == current_state_) {
                bool all_preds_true = true;
                for (const auto& pred : rule.predicates) {
                    if (!pred()) {
                        all_preds_true = false;
                        break;
                    }
                }
                if (all_preds_true) {
                    for (const auto& action : rule.actions) {
                        action();
                    }
                    current_state_ = rule.to_state;
                    return;
                }
            }
        }
    }

    // Predicates
    bool isReady() { return ready_flag_; }
    bool hasEvent() { return event_flag_; }

    // Actions
    void doSetup() { ready_flag_ = true; }
    void processEvent() { event_flag_ = false; }
    void enterError() { error_flag_ = true; }

private:
    std::string_view current_state_;
    std::vector<Transition> transitions_;
    bool ready_flag_ = false;
    bool event_flag_ = false;
    bool error_flag_ = false;
};
```

### Usage

```cpp
int main() {
    MyStateMachine fsm("IDLE");
    fsm.step();  // Will check transitions from IDLE state
    return 0;
}
```

## Building

To build FSMgine:

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

## Running

To run FSMgine:

```bash
fsmgine < source_file.cpp > compiled_file.cpp
```

Then proceed to compile compiled_file.cpp with your favourite compiler, such as g++ or clang++.

## License

Please see the LICENSE file.