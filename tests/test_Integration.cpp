#include <gtest/gtest.h>
#include "FSMgine/FSM.hpp"
#include "FSMgine/FSMBuilder.hpp"
#include "FSMgine/StringInterner.hpp"

using namespace fsmgine;

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        StringInterner::instance().clear();
        reset_state();
    }
    
    void reset_state() {
        coin_inserted = false;
        door_pushed = false;
        turnstile_locked = true;
        error_state = false;
        alarm_triggered = false;
        events.clear();
    }
    
    // Turnstile simulation state
    bool coin_inserted = false;
    bool door_pushed = false;
    bool turnstile_locked = true;
    bool error_state = false;
    bool alarm_triggered = false;
    std::vector<std::string> events;
    
    // Helper methods
    void insert_coin() { coin_inserted = true; }
    void push_door() { door_pushed = true; }
    void unlock_turnstile() { 
        turnstile_locked = false; 
        events.push_back("unlocked");
    }
    void lock_turnstile() { 
        turnstile_locked = true; 
        events.push_back("locked");
    }
    void trigger_alarm() { 
        alarm_triggered = true; 
        events.push_back("alarm");
    }
    void reset_events() {
        coin_inserted = false;
        door_pushed = false;
    }
};

TEST_F(IntegrationTest, TurnstileStateMachine) {
    FSM turnstile;
    
    // Build turnstile FSM
    auto builder = turnstile.get_builder();
    
    // Define state actions
    builder.onEnter("LOCKED", [this]() { lock_turnstile(); })
           .onEnter("UNLOCKED", [this]() { unlock_turnstile(); })
           .onEnter("ERROR", [this]() { trigger_alarm(); });
    
    // Define transitions
    builder.from("LOCKED")
           .predicate([this]() { return coin_inserted; })
           .action([this]() { reset_events(); })
           .to("UNLOCKED");
    
    builder.from("UNLOCKED")
           .predicate([this]() { return door_pushed; })
           .action([this]() { reset_events(); })
           .to("LOCKED");
    
    // Error transitions (trying to push without coin)
    builder.from("LOCKED")
           .predicate([this]() { return door_pushed; })
           .to("ERROR");
    
    // Recovery from error (insert coin)
    builder.from("ERROR")
           .predicate([this]() { return coin_inserted; })
           .action([this]() { reset_events(); })
           .to("UNLOCKED");
    
    // Start in locked state
    turnstile.setInitialState("LOCKED");
    EXPECT_EQ(turnstile.getCurrentState(), "LOCKED");
    EXPECT_TRUE(turnstile_locked);
    
    // Test normal operation: coin -> unlock -> push -> lock
    insert_coin();
    EXPECT_TRUE(turnstile.step()); // LOCKED -> UNLOCKED
    EXPECT_EQ(turnstile.getCurrentState(), "UNLOCKED");
    EXPECT_FALSE(turnstile_locked);
    
    push_door();
    EXPECT_TRUE(turnstile.step()); // UNLOCKED -> LOCKED
    EXPECT_EQ(turnstile.getCurrentState(), "LOCKED");
    EXPECT_TRUE(turnstile_locked);
    
    // Test error case: push without coin
    push_door();
    EXPECT_TRUE(turnstile.step()); // LOCKED -> ERROR
    EXPECT_EQ(turnstile.getCurrentState(), "ERROR");
    EXPECT_TRUE(alarm_triggered);
    
    // Test recovery: insert coin from error state
    insert_coin();
    EXPECT_TRUE(turnstile.step()); // ERROR -> UNLOCKED
    EXPECT_EQ(turnstile.getCurrentState(), "UNLOCKED");
    EXPECT_FALSE(turnstile_locked);
}

TEST_F(IntegrationTest, TrafficLightStateMachine) {
    FSM traffic_light;
    
    // Traffic light state
    std::string current_light = "RED";
    int timer = 0;
    
    auto builder = traffic_light.get_builder();
    
    // State actions
    builder.onEnter("RED", [&]() { 
        current_light = "RED"; 
        timer = 0; 
        events.push_back("red_on");
    });
    
    builder.onEnter("YELLOW", [&]() { 
        current_light = "YELLOW"; 
        timer = 0; 
        events.push_back("yellow_on");
    });
    
    builder.onEnter("GREEN", [&]() { 
        current_light = "GREEN"; 
        timer = 0; 
        events.push_back("green_on");
    });
    
    // Timer-based transitions
    builder.from("RED")
           .predicate([&]() { return timer >= 3; })
           .to("GREEN");
    
    builder.from("GREEN")
           .predicate([&]() { return timer >= 5; })
           .to("YELLOW");
    
    builder.from("YELLOW")
           .predicate([&]() { return timer >= 2; })
           .to("RED");
    
    traffic_light.setInitialState("RED");
    EXPECT_EQ(current_light, "RED");
    
    // Simulate time passage
    for (int cycle = 0; cycle < 2; ++cycle) {
        // Red phase (3 seconds)
        EXPECT_EQ(traffic_light.getCurrentState(), "RED");
        for (timer = 0; timer < 3; ++timer) {
            EXPECT_FALSE(traffic_light.step());
        }
        EXPECT_TRUE(traffic_light.step()); // RED -> GREEN
        
        // Green phase (5 seconds)
        EXPECT_EQ(traffic_light.getCurrentState(), "GREEN");
        for (timer = 0; timer < 5; ++timer) {
            EXPECT_FALSE(traffic_light.step());
        }
        EXPECT_TRUE(traffic_light.step()); // GREEN -> YELLOW
        
        // Yellow phase (2 seconds)
        EXPECT_EQ(traffic_light.getCurrentState(), "YELLOW");
        for (timer = 0; timer < 2; ++timer) {
            EXPECT_FALSE(traffic_light.step());
        }
        EXPECT_TRUE(traffic_light.step()); // YELLOW -> RED
    }
    
    // Verify events were triggered in correct order
    std::vector<std::string> expected_events = {
        "red_on",    // Initial state
        "green_on",  // First cycle
        "yellow_on",
        "red_on",    // Second cycle
        "green_on",
        "yellow_on",
        "red_on"     // Back to start
    };
    
    EXPECT_EQ(events, expected_events);
}

TEST_F(IntegrationTest, ComplexWorkflowStateMachine) {
    FSM workflow;
    
    // Workflow state
    bool task_ready = false;
    bool task_completed = false;
    bool approval_received = false;
    bool error_occurred = false;
    int retry_count = 0;
    const int max_retries = 3;
    
    auto builder = workflow.get_builder();
    
    // Define all states with enter actions
    builder.onEnter("IDLE", [&]() { 
        events.push_back("workflow_idle"); 
    });
    
    builder.onEnter("PROCESSING", [&]() { 
        events.push_back("processing_started"); 
    });
    
    builder.onEnter("WAITING_APPROVAL", [&]() { 
        events.push_back("waiting_for_approval"); 
    });
    
    builder.onEnter("COMPLETED", [&]() { 
        events.push_back("workflow_completed"); 
    });
    
    builder.onEnter("FAILED", [&]() { 
        events.push_back("workflow_failed"); 
    });
    
    builder.onEnter("RETRY", [&]() { 
        events.push_back("retrying"); 
    });
    
    // Define transitions
    // IDLE -> PROCESSING (when task is ready)
    builder.from("IDLE")
           .predicate([&]() { return task_ready; })
           .to("PROCESSING");
    
    // PROCESSING -> WAITING_APPROVAL (when task completed successfully)
    builder.from("PROCESSING")
           .predicate([&]() { return task_completed && !error_occurred; })
           .to("WAITING_APPROVAL");
    
    // PROCESSING -> RETRY (when error occurs and retries available)
    builder.from("PROCESSING")
           .predicate([&]() { return error_occurred && retry_count < max_retries; })
           .action([&]() { retry_count++; error_occurred = false; }) // Increment here
           .to("RETRY");
    
    // PROCESSING -> FAILED (when error occurs and no more retries)
    builder.from("PROCESSING")
           .predicate([&]() { return error_occurred && retry_count >= max_retries; })
           .to("FAILED");
    
    // RETRY -> PROCESSING (automatically retry)
    builder.from("RETRY")
           .predicate([]() { return true; }) // Always transition
           .to("PROCESSING");
    
    // WAITING_APPROVAL -> COMPLETED (when approval received)
    builder.from("WAITING_APPROVAL")
           .predicate([&]() { return approval_received; })
           .to("COMPLETED");
    
    // WAITING_APPROVAL -> PROCESSING (when approval denied - restart)
    builder.from("WAITING_APPROVAL")
           .predicate([&]() { return !approval_received && task_ready; })
           .action([&]() { task_completed = false; }) // Reset completion status
           .to("PROCESSING");
    
    workflow.setInitialState("IDLE");
    
    // Test successful workflow
    task_ready = true;
    EXPECT_TRUE(workflow.step()); // IDLE -> PROCESSING
    
    task_completed = true;
    EXPECT_TRUE(workflow.step()); // PROCESSING -> WAITING_APPROVAL
    
    approval_received = true;
    EXPECT_TRUE(workflow.step()); // WAITING_APPROVAL -> COMPLETED
    
    EXPECT_EQ(workflow.getCurrentState(), "COMPLETED");
    
    // Reset for error scenario
    workflow.setCurrentState("IDLE");
    task_ready = true;
    task_completed = false;
    approval_received = false;
    error_occurred = false;
    retry_count = 0;
    events.clear();
    
    // Test retry workflow
    EXPECT_TRUE(workflow.step()); // IDLE -> PROCESSING
    
    // Simulate error
    error_occurred = true;
    EXPECT_TRUE(workflow.step()); // PROCESSING -> RETRY (attempt 1)
    EXPECT_TRUE(workflow.step()); // RETRY -> PROCESSING
    
    // Error again
    error_occurred = true;
    EXPECT_TRUE(workflow.step()); // PROCESSING -> RETRY (attempt 2)
    EXPECT_TRUE(workflow.step()); // RETRY -> PROCESSING
    
    // Error again
    error_occurred = true;
    EXPECT_TRUE(workflow.step()); // PROCESSING -> RETRY (attempt 3)
    EXPECT_TRUE(workflow.step()); // RETRY -> PROCESSING
    
    // Final error - should go to FAILED
    error_occurred = true;
    EXPECT_TRUE(workflow.step()); // PROCESSING -> FAILED
    
    EXPECT_EQ(workflow.getCurrentState(), "FAILED");
    EXPECT_EQ(retry_count, 3);
}