/// @file FSMgine.hpp
/// @brief Main header file for the FSMgine library
/// 
/// @mainpage FSMgine Documentation
/// @section intro Introduction
/// FSMgine is a high-performance, header-only finite state machine library for C++17.
/// 
/// @section features Features
/// - Type-safe state and event handling
/// - Support for both event-driven and event-less FSMs
/// - Compile-time optimizations with string interning
/// - Optional thread-safety with FSMGINE_MULTI_THREADED
/// - Fluent builder API for easy FSM construction
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
