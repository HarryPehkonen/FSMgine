#include <iostream>
#include "FSMgine/FSMgine.hpp"

using namespace fsmgine;

int main() {
    // Create a simple turnstile state machine
    FSM turnstile;
    
    // Simulation state
    bool coin_inserted = false;
    bool door_pushed = false;
    
    // Build the turnstile FSM
    auto builder = turnstile.get_builder();
    
    // Define state enter/exit actions
    builder.onEnter("LOCKED", []() { 
        std::cout << "🔒 Turnstile is LOCKED\n"; 
    });
    
    builder.onEnter("UNLOCKED", []() { 
        std::cout << "🔓 Turnstile is UNLOCKED\n"; 
    });
    
    builder.onEnter("ERROR", []() { 
        std::cout << "🚨 ERROR: Tried to push without coin!\n"; 
    });
    
    // Define transitions
    builder.from("LOCKED")
           .predicate([&]() { return coin_inserted; })
           .action([&]() { 
               std::cout << "💰 Coin accepted!\n"; 
               coin_inserted = false;
           })
           .to("UNLOCKED");
    
    builder.from("UNLOCKED")
           .predicate([&]() { return door_pushed; })
           .action([&]() { 
               std::cout << "🚪 Door pushed, person passed through\n"; 
               door_pushed = false;
           })
           .to("LOCKED");
    
    // Error case: trying to push door when locked
    builder.from("LOCKED")
           .predicate([&]() { return door_pushed; })
           .action([&]() { door_pushed = false; })
           .to("ERROR");
    
    // Recovery from error
    builder.from("ERROR")
           .predicate([&]() { return coin_inserted; })
           .action([&]() { 
               std::cout << "💰 Coin inserted, recovering from error\n"; 
               coin_inserted = false;
           })
           .to("UNLOCKED");
    
    // Start the FSM
    turnstile.setInitialState("LOCKED");
    
    std::cout << "=== FSMgine Turnstile Demo ===\n";
    std::cout << "Current state: " << turnstile.getCurrentState() << "\n\n";
    
    // Simulate normal operation
    std::cout << "--- Normal Operation ---\n";
    coin_inserted = true;
    if (turnstile.step()) {
        std::cout << "Current state: " << turnstile.getCurrentState() << "\n";
    }
    
    door_pushed = true;
    if (turnstile.step()) {
        std::cout << "Current state: " << turnstile.getCurrentState() << "\n";
    }
    
    // Simulate error case
    std::cout << "\n--- Error Case ---\n";
    door_pushed = true;
    if (turnstile.step()) {
        std::cout << "Current state: " << turnstile.getCurrentState() << "\n";
    }
    
    // Recover from error
    std::cout << "\n--- Recovery ---\n";
    coin_inserted = true;
    if (turnstile.step()) {
        std::cout << "Current state: " << turnstile.getCurrentState() << "\n";
    }
    
    door_pushed = true;
    if (turnstile.step()) {
        std::cout << "Current state: " << turnstile.getCurrentState() << "\n";
    }
    
    std::cout << "\nDemo complete! ✨\n";
    
    return 0;
}