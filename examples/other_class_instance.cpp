#include <FSMgine/FSMgine.hpp>
#include <iostream>
#include <string>
#include <vector>

class CharacterProcessor {
public:
    CharacterProcessor() : word_count(0), current_word("") {}
    
    // Predicates
    bool is_whitespace(char c) const {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }
    
    bool is_alpha(char c) const {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }
    
    bool is_digit(char c) const {
        return c >= '0' && c <= '9';
    }
    
    bool is_end(char c) const {
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

private:
    int word_count;
    std::string current_word;
};

int main() {
    // Create our state machine with char as the event type
    fsmgine::FSM<char> fsm;
    CharacterProcessor processor;
    
    // Build the state machine with a fluent interface
    fsm.get_builder()
        .from("START")
        .predicate([&processor](char c) { return processor.is_whitespace(c); })
        .action([&processor](char c) { processor.process_whitespace(c); })
        .to("START");

    fsm.get_builder()
        .from("START")
        .predicate([&processor](char c) { return processor.is_alpha(c); })
        .action([&processor](char c) { processor.process_alpha(c); })
        .to("START");

    fsm.get_builder()
        .from("START")
        .predicate([&processor](char c) { return processor.is_digit(c); })
        .action([&processor](char c) { processor.process_digit(c); })
        .to("START");

    fsm.get_builder()
        .from("START")
        .predicate([&processor](char c) { return processor.is_end(c); })
        .action([&processor](char c) { processor.process_end(c); })
        .to("END");
    
    // Set initial state
    fsm.setInitialState("START");
    
    // Run the state machine
    std::cout << "Starting character processor...\n";
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