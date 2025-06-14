#include <FSMgine/FSMgine.hpp>
#include <iostream>
#include <stack>
#include <string>

class ParenthesesChecker {
public:
    ParenthesesChecker() = default;
    
    // Stack operations
    void push(char c) {
        stack.push(c);
    }
    
    bool pop() {
        if (stack.empty()) {
            return false;
        }
        stack.pop();
        return true;
    }
    
    bool is_empty() const {
        return stack.empty();
    }
    
    // Predicates
    bool is_open_paren(char c) const {
        return c == '(' || c == '[' || c == '{';
    }
    
    bool is_close_paren(char c) const {
        return c == ')' || c == ']' || c == '}';
    }
    
    bool is_end(char c) const {
        return c == EOF;
    }
    
    bool matches_top(char c) const {
        if (stack.empty()) return false;
        char top = stack.top();
        return (top == '(' && c == ')') ||
               (top == '[' && c == ']') ||
               (top == '{' && c == '}');
    }
    
    // Actions
    void process_open(char c) {
        push(c);
        std::cout << "Pushed " << c << std::endl;
    }
    
    void process_close(char c) {
        if (matches_top(c)) {
            pop();
            std::cout << "Matched " << c << std::endl;
        } else {
            std::cout << "Mismatched " << c << std::endl;
        }
    }
    
    void process_end(char c) {
        if (is_empty()) {
            std::cout << "Success: All parentheses are balanced!" << std::endl;
        } else {
            std::cout << "Error: Unmatched opening parentheses remain" << std::endl;
        }
    }

private:
    std::stack<char> stack;
};

int main() {
    // Create our state machine with char as the event type
    fsmgine::FSM<char> fsm;
    ParenthesesChecker checker;
    
    // Build the state machine with a fluent interface
    fsm.get_builder()
        .from("START")
        .predicate([&checker](char c) { return checker.is_open_paren(c); })
        .action([&checker](char c) { checker.process_open(c); })
        .to("START");

    fsm.get_builder()
        .from("START")
        .predicate([&checker](char c) { return checker.is_close_paren(c); })
        .action([&checker](char c) { checker.process_close(c); })
        .to("START");

    fsm.get_builder()
        .from("START")
        .predicate([&checker](char c) { return checker.is_end(c); })
        .action([&checker](char c) { checker.process_end(c); })
        .to("END");
    
    // Set initial state
    fsm.setInitialState("START");
    
    // Run the state machine
    std::cout << "Starting parentheses checker...\n";
    std::cout << "Enter text (Ctrl+D to end):\n";
    
    // read characters from stdin until EOF
    while (fsm.getCurrentState() != "END") {
        char c;
        if (!std::cin.get(c)) {  // Check if read failed (EOF)
            c = EOF;  // Set to EOF if read failed
        }
        fsm.process(c);
    }
    
    return 0;
} 