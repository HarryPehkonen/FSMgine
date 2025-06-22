/// @file FSMgine.hpp
/// @brief Main header file for the FSMgine library
/// 
/// @section intro Introduction
/// FSMgine is a high-performance finite state machine library for C++17, available in
/// both single-threaded and multi-threaded variants.
/// 
/// @section features Features
/// - Type-safe state and event handling
/// - Support for both event-driven and event-less FSMs
/// - Compile-time optimizations with string interning
/// - Two library variants: FSMgine (single-threaded) and FSMgineMT (multi-threaded)
/// - Fluent builder API for easy FSM construction
/// 
/// @section variants Library Variants
/// FSMgine provides two library variants:
/// - **FSMgine**: Single-threaded variant with no synchronization overhead
/// - **FSMgineMT**: Multi-threaded variant with mutex-based thread safety
/// 
/// Choose the appropriate variant based on your application's threading requirements.
/// 
/// @section usage Basic Usage
/// @code{.cpp}
/// #include <FSMgine/FSMgine.hpp>
/// 
/// // Event-less FSM example
/// fsm::EventlessFSM turnstile;
/// turnstile.get_builder()
///     .from("Locked").predicate([](const auto&) { return true; }).to("Unlocked")
///     .from("Unlocked").predicate([](const auto&) { return true; }).to("Locked");
/// 
/// turnstile.setInitialState("Locked");
/// @endcode
/// 
/// @section linking Linking
/// @code{.cmake}
/// # For single-threaded applications:
/// find_package(FSMgine REQUIRED)
/// target_link_libraries(my_app PRIVATE FSMgine::FSMgine)
/// 
/// # For multi-threaded applications:
/// find_package(FSMgineMT REQUIRED)
/// target_link_libraries(my_app PRIVATE FSMgine::FSMgineMT)
/// @endcode
/// 
/// @section modules Modules
/// - @ref core "Core Components" - Main FSM implementation
/// - @ref builder "Builder API" - Fluent interface for FSM construction
/// - @ref transitions "Transition System" - State transition management
/// - @ref utilities "Utility Components" - String interning and helpers

#pragma once

#include <variant>

// Main FSMgine header - includes everything you need
#include "FSMgine/StringInterner.hpp"
#include "FSMgine/Transition.hpp"
#include "FSMgine/FSM.hpp"
#include "FSMgine/FSMBuilder.hpp"

/// @namespace fsm
/// @brief Convenience namespace alias for fsmgine
namespace fsm = fsmgine;

/// @namespace fsmgine
/// @brief Main namespace for the FSMgine library
namespace fsmgine {
    /// @brief Type alias for event-less finite state machines
    /// @details FSMs that don't require events for transitions, using std::monostate internally
    using EventlessFSM = FSM<>;
}
