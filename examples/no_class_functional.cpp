#include <FSMgine/FSMgine.hpp>
#include <iostream>
#include <string>

// Global state
int word_count = 0;
std::string current_word;

// Predicates
bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool is_end(char c) {
    return c == EOF;
}

// Actions
void process_alpha(char c) {
    current_word += c;
}

void process_digit(char c) {
    current_word += c;
}

void process_whitespace(char c) {
    if (!current_word.empty()) {
        word_count++;
        std::cout << "Word " << word_count << ": " << current_word << std::endl;
        current_word.clear();
    }
}

void process_end(char c) {
    // Process any remaining word
    if (!current_word.empty()) {
        word_count++;
        std::cout << "Word " << word_count << ": " << current_word << std::endl;
    }
    std::cout << "Total words processed: " << word_count << std::endl;
}

int main() {
    // Create our state machine with char as the event type
    fsmgine::FSM<char> fsm;
    
    // Reset global state
    word_count = 0;
    current_word.clear();
    
    // Build the state machine with a fluent interface
    fsm.get_builder()
        .from("START")
        .predicate(is_whitespace)
        .action(process_whitespace)
        .to("START");

    fsm.get_builder()
        .from("START")
        .predicate(is_alpha)
        .action(process_alpha)
        .to("START");

    fsm.get_builder()
        .from("START")
        .predicate(is_digit)
        .action(process_digit)
        .to("START");

    fsm.get_builder()
        .from("START")
        .predicate(is_end)
        .action(process_end)
        .to("END");
    
    // Set initial state
    fsm.setInitialState("START");
    
    // Run the state machine
    std::cout << "Starting word counter...\n";
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