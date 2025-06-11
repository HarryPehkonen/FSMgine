#include <iostream>
#include "FSMgine/FSMgine.hpp"

using namespace fsmgine;

// Define the events that can drive the turnstile FSM
enum class TurnstileEvent {
    COIN_INSERTED,
    DOOR_PUSHED
};

int main() {
    // Create a turnstile state machine that processes TurnstileEvent
    FSM<TurnstileEvent> turnstile;
    
    // Build the turnstile FSM
    auto builder = turnstile.get_builder();
    
    // Define state enter actions. They can optionally use the event that caused the entry.
    builder.onEnter("LOCKED", [](const auto&){ 
        std::cout << "🔒 Turnstile is LOCKED\n"; 
    });
    
    builder.onEnter("UNLOCKED", [](const auto&){ 
        std::cout << "🔓 Turnstile is UNLOCKED\n"; 
    });
    
    builder.onEnter("ERROR", [](const auto&){ 
        std::cout << "🚨 ERROR: Tried to push without coin!\n"; 
    });
    
    // Define transitions based on events
    builder.from("LOCKED")
           .predicate([](const TurnstileEvent& e) { return e == TurnstileEvent::COIN_INSERTED; })
           .action([](const auto&) { std::cout << "💰 Coin accepted!\n"; })
           .to("UNLOCKED");
    
    builder.from("UNLOCKED")
           .predicate([](const TurnstileEvent& e) { return e == TurnstileEvent::DOOR_PUSHED; })
           .action([](const auto&) { std::cout << "🚪 Door pushed, person passed through\n"; })
           .to("LOCKED");
    
    // Error case: trying to push door when locked
    builder.from("LOCKED")
           .predicate([](const TurnstileEvent& e) { return e == TurnstileEvent::DOOR_PUSHED; })
           .to("ERROR");
    
    // Recovery from error
    builder.from("ERROR")
           .predicate([](const TurnstileEvent& e) { return e == TurnstileEvent::COIN_INSERTED; })
           .action([](const auto&) { std::cout << "💰 Coin inserted, recovering from error\n"; })
           .to("UNLOCKED");
    
    // Start the FSM
    turnstile.setInitialState("LOCKED");
    
    std::cout << "=== FSMgine Turnstile Demo ===\n";
    std::cout << "Current state: " << turnstile.getCurrentState() << "\n\n";
    
    // Simulate normal operation by processing events
    std::cout << "--- Normal Operation ---\n";
    if (turnstile.process(TurnstileEvent::COIN_INSERTED)) {
        std::cout << "Current state: " << turnstile.getCurrentState() << "\n";
    }
    
    if (turnstile.process(TurnstileEvent::DOOR_PUSHED)) {
        std::cout << "Current state: " << turnstile.getCurrentState() << "\n";
    }
    
    // Simulate error case
    std::cout << "\n--- Error Case ---\n";
    if (turnstile.process(TurnstileEvent::DOOR_PUSHED)) {
        std::cout << "Current state: " << turnstile.getCurrentState() << "\n";
    }
    
    // Recover from error
    std::cout << "\n--- Recovery ---\n";
    if (turnstile.process(TurnstileEvent::COIN_INSERTED)) {
        std::cout << "Current state: " << turnstile.getCurrentState() << "\n";
    }
    
    if (turnstile.process(TurnstileEvent::DOOR_PUSHED)) {
        std::cout << "Current state: " << turnstile.getCurrentState() << "\n";
    }
    
    std::cout << "\nDemo complete! ✨\n";
    
    return 0;
}
