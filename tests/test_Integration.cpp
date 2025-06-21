#include <gtest/gtest.h>
#include <variant>
#include "FSMgine/FSM.hpp"
#include "FSMgine/FSMBuilder.hpp"
#include "FSMgine/StringInterner.hpp"

using namespace fsmgine;

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        StringInterner::instance().clear();
        events.clear();
    }
    
    std::vector<std::string> events;
};

// Event type for Turnstile FSM
enum class TurnstileEvent { COIN, PUSH };

TEST_F(IntegrationTest, TurnstileStateMachine) {
    FSM<TurnstileEvent> turnstile;
    bool turnstile_locked = true;
    bool alarm_triggered = false;
    
    // Build turnstile FSM
    auto builder = turnstile.get_builder();
    
    // Define state actions
    builder.onEnter("LOCKED", [&](const auto&) { turnstile_locked = true; events.push_back("locked"); })
           .onEnter("UNLOCKED", [&](const auto&) { turnstile_locked = false; events.push_back("unlocked"); })
           .onEnter("ERROR", [&](const auto&) { alarm_triggered = true; events.push_back("alarm"); });
    
    // Define transitions
    builder.from("LOCKED")
           .predicate([](const TurnstileEvent& e) { return e == TurnstileEvent::COIN; })
           .to("UNLOCKED");
    
    builder.from("UNLOCKED")
           .predicate([](const TurnstileEvent& e) { return e == TurnstileEvent::PUSH; })
           .to("LOCKED");
    
    builder.from("LOCKED")
           .predicate([](const TurnstileEvent& e) { return e == TurnstileEvent::PUSH; })
           .to("ERROR");
    
    builder.from("ERROR")
           .predicate([](const TurnstileEvent& e) { return e == TurnstileEvent::COIN; })
           .to("UNLOCKED");
    
    turnstile.setInitialState("LOCKED");
    EXPECT_EQ(turnstile.getCurrentState(), "LOCKED");
    EXPECT_TRUE(turnstile_locked);
    
    // Test normal operation
    EXPECT_TRUE(turnstile.process(TurnstileEvent::COIN));
    EXPECT_EQ(turnstile.getCurrentState(), "UNLOCKED");
    EXPECT_FALSE(turnstile_locked);
    
    EXPECT_TRUE(turnstile.process(TurnstileEvent::PUSH));
    EXPECT_EQ(turnstile.getCurrentState(), "LOCKED");
    EXPECT_TRUE(turnstile_locked);
    
    // Test error case
    EXPECT_TRUE(turnstile.process(TurnstileEvent::PUSH));
    EXPECT_EQ(turnstile.getCurrentState(), "ERROR");
    EXPECT_TRUE(alarm_triggered);
    
    // Test recovery
    EXPECT_TRUE(turnstile.process(TurnstileEvent::COIN));
    EXPECT_EQ(turnstile.getCurrentState(), "UNLOCKED");
    EXPECT_FALSE(turnstile_locked);
}

TEST_F(IntegrationTest, TrafficLightStateMachine) {
    FSM<> traffic_light; // Event-less FSM
    
    int timer = 0;
    
    auto builder = traffic_light.get_builder();
    
    builder.onEnter("RED",    [&](const auto&) { events.push_back("red_on"); timer = 0; });
    builder.onEnter("YELLOW", [&](const auto&) { events.push_back("yellow_on"); timer = 0; });
    builder.onEnter("GREEN",  [&](const auto&) { events.push_back("green_on"); timer = 0; });
    
    builder.from("RED").predicate([&](const auto&) { return timer >= 3; }).to("GREEN");
    builder.from("GREEN").predicate([&](const auto&) { return timer >= 5; }).to("YELLOW");
    builder.from("YELLOW").predicate([&](const auto&) { return timer >= 2; }).to("RED");
    
    traffic_light.setInitialState("RED");
    
    for (int cycle = 0; cycle < 2; ++cycle) {
        EXPECT_EQ(traffic_light.getCurrentState(), "RED");
        for (timer = 0; timer < 3; ++timer) EXPECT_FALSE(traffic_light.process());
        EXPECT_TRUE(traffic_light.process());
        
        EXPECT_EQ(traffic_light.getCurrentState(), "GREEN");
        for (timer = 0; timer < 5; ++timer) EXPECT_FALSE(traffic_light.process());
        EXPECT_TRUE(traffic_light.process());
        
        EXPECT_EQ(traffic_light.getCurrentState(), "YELLOW");
        for (timer = 0; timer < 2; ++timer) EXPECT_FALSE(traffic_light.process());
        EXPECT_TRUE(traffic_light.process());
    }
    
    std::vector<std::string> expected = {"red_on", "green_on", "yellow_on", "red_on", "green_on", "yellow_on", "red_on"};
    EXPECT_EQ(events, expected);
}

TEST_F(IntegrationTest, ComplexWorkflowStateMachine) {
    FSM<> workflow; // Event-less FSM
    
    bool task_ready = false, task_completed = false, approval_received = false, error_occurred = false;
    int retry_count = 0;
    const int max_retries = 3;
    
    auto builder = workflow.get_builder();
    
    builder.onEnter("IDLE",             [&](const auto&){ events.push_back("idle"); });
    builder.onEnter("PROCESSING",       [&](const auto&){ events.push_back("processing"); });
    builder.onEnter("WAITING_APPROVAL", [&](const auto&){ events.push_back("waiting"); });
    builder.onEnter("COMPLETED",        [&](const auto&){ events.push_back("completed"); });
    builder.onEnter("FAILED",           [&](const auto&){ events.push_back("failed"); });
    builder.onEnter("RETRY",            [&](const auto&){ events.push_back("retry"); });
    
    builder.from("IDLE").predicate([&](const auto&){ return task_ready; }).to("PROCESSING");
    builder.from("PROCESSING").predicate([&](const auto&){ return task_completed && !error_occurred; }).to("WAITING_APPROVAL");
    builder.from("PROCESSING").predicate([&](const auto&){ return error_occurred && retry_count < max_retries; }).action([&](const auto&){ retry_count++; error_occurred = false; }).to("RETRY");
    builder.from("PROCESSING").predicate([&](const auto&){ return error_occurred && retry_count >= max_retries; }).to("FAILED");
    builder.from("RETRY").to("PROCESSING");
    builder.from("WAITING_APPROVAL").predicate([&](const auto&){ return approval_received; }).to("COMPLETED");
    builder.from("WAITING_APPROVAL").predicate([&](const auto&){ return !approval_received && task_ready; }).action([&](const auto&){ task_completed = false; }).to("PROCESSING");
    
    workflow.setInitialState("IDLE");
    
    // Successful workflow
    task_ready = true;
    workflow.process(); // -> PROCESSING
    task_completed = true;
    workflow.process(); // -> WAITING_APPROVAL
    approval_received = true;
    workflow.process(); // -> COMPLETED
    EXPECT_EQ(workflow.getCurrentState(), "COMPLETED");

    // Reset for error scenario
    workflow.setCurrentState("IDLE");
    task_ready = false; task_completed = false; approval_received = false; error_occurred = false; retry_count = 0;
    events.clear();
    
    // Test retry workflow
    task_ready = true;
    workflow.process(); // -> PROCESSING
    for (int i = 0; i < max_retries; ++i) {
        error_occurred = true;
        workflow.process(); // -> RETRY
        workflow.process(); // -> PROCESSING
    }
    error_occurred = true;
    workflow.process(); // -> FAILED
    
    EXPECT_EQ(workflow.getCurrentState(), "FAILED");
    EXPECT_EQ(retry_count, 3);
}
