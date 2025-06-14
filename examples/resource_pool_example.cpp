#include "FSMgine/FSMgine.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <atomic>
#include <random>

using namespace fsmgine;

// Simple event type for our FSM
struct ResourceEvent {
    bool is_acquire;
    ResourceEvent(bool acquire = false) : is_acquire(acquire) {}
};

// Resource pool state machine
class ResourcePool {
private:
    FSM<ResourceEvent> fsm;
    std::atomic<int> available_resources;
    std::mutex cout_mutex;  // For synchronized output

    void log(const std::string& message) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "[" << std::this_thread::get_id() << "] " << message << std::endl;
    }

public:
    ResourcePool(int initial_resources) : available_resources(initial_resources) {
        // Build the state machine
        fsm.get_builder()
            .onEnter("IDLE", [this](const ResourceEvent&) { log("Pool is idle"); })
            .onEnter("BUSY", [this](const ResourceEvent&) { log("Pool is busy"); })
            .onEnter("EMPTY", [this](const ResourceEvent&) { log("Pool is empty"); });

        // Transition: IDLE -> BUSY (when resources are available)
        fsm.get_builder()
            .from("IDLE")
            .predicate([this](const ResourceEvent& e) { return e.is_acquire && available_resources > 0; })
            .action([this](const ResourceEvent&) { 
                available_resources--;
                log("Resource acquired. Remaining: " + std::to_string(available_resources));
            })
            .to("BUSY");

        // Transition: BUSY -> IDLE (when resources are released)
        fsm.get_builder()
            .from("BUSY")
            .predicate([this](const ResourceEvent& e) { return !e.is_acquire; })
            .action([this](const ResourceEvent&) {
                available_resources++;
                log("Resource released. Available: " + std::to_string(available_resources));
            })
            .to("IDLE");

        // Transition: IDLE -> EMPTY (when no resources are available)
        fsm.get_builder()
            .from("IDLE")
            .predicate([this](const ResourceEvent& e) { return e.is_acquire && available_resources == 0; })
            .to("EMPTY");

        // Transition: EMPTY -> IDLE (when resources become available)
        fsm.get_builder()
            .from("EMPTY")
            .predicate([this](const ResourceEvent& e) { return !e.is_acquire; })
            .to("IDLE");

        fsm.setInitialState("IDLE");
    }

    bool acquireResource() {
        return fsm.process(ResourceEvent(true));
    }

    bool releaseResource() {
        return fsm.process(ResourceEvent(false));
    }

    std::string_view getCurrentState() const {
        return fsm.getCurrentState();
    }
};

void worker(ResourcePool& pool, int id, int iterations) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> sleep_dist(100, 500);

    for (int i = 0; i < iterations; ++i) {
        // Try to acquire resource
        if (pool.acquireResource()) {
            // Simulate work
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_dist(gen)));
            
            // Release resource
            pool.releaseResource();
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}

int main() {
    const int NUM_RESOURCES = 3;
    const int NUM_WORKERS = 5;
    const int ITERATIONS_PER_WORKER = 10;

    std::cout << "Starting resource pool example with " 
              << NUM_RESOURCES << " resources and " 
              << NUM_WORKERS << " workers\n\n";

    ResourcePool pool(NUM_RESOURCES);
    std::vector<std::thread> workers;

    // Start worker threads
    for (int i = 0; i < NUM_WORKERS; ++i) {
        workers.emplace_back(worker, std::ref(pool), i, ITERATIONS_PER_WORKER);
    }

    // Wait for all workers to complete
    for (auto& w : workers) {
        w.join();
    }

    std::cout << "\nAll workers completed. Final state: " << pool.getCurrentState() << std::endl;
    return 0;
}