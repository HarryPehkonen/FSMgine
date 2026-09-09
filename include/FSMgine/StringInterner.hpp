/// @file StringInterner.hpp
/// @brief String interning utility for memory-efficient state name storage
/// @ingroup utilities

#pragma once

#include <deque>
#include <string>
#include <string_view>
#include <unordered_set>

#ifdef FSMGINE_MULTI_THREADED
#include <mutex>
#endif

/// @defgroup utilities Utility Components
/// @brief Utility classes and helpers for the FSMgine library

namespace fsmgine {

/// @brief Provides memory-efficient string storage through string interning
/// @ingroup utilities
/// 
/// @details StringInterner implements the string interning pattern to optimize
/// string storage and comparison in the FSM. By interning strings, we achieve:
/// - Fast pointer-based string comparisons (O(1) instead of O(n))
/// - Reduced memory usage when the same state names are used multiple times
/// - Guaranteed string_view safety throughout the FSM lifetime
/// - Thread-safe operations when using the FSMgineMT library variant
/// 
/// @note This is a singleton class - use StringInterner::instance() to access
/// 
/// @par Thread Safety
/// Thread-safety depends on which library variant you're using:
/// - **FSMgine**: No thread synchronization, must be used from a single thread
/// - **FSMgineMT**: All operations are protected by mutexes for thread-safe access
/// 
/// @warning The clear() method is NOT thread-safe in either variant and should 
/// only be used in single-threaded test scenarios.
class StringInterner {
public:
    /// @brief Gets the singleton instance of StringInterner
    /// @return Reference to the global StringInterner instance
    static StringInterner& instance();
    
    /// @brief Interns a string and returns a persistent string_view
    /// @param str The string to intern
    /// @return A string_view that remains valid for the lifetime of the StringInterner
    /// @note The returned string_view points to the interned string stored internally
    std::string_view intern(const std::string& str);
    
    /// @brief Interns a string_view and returns a persistent string_view
    /// @param sv The string_view to intern
    /// @return A string_view that remains valid for the lifetime of the StringInterner
    /// @note The input string_view's data is copied and stored internally
    std::string_view intern(std::string_view sv);
    
    /// @brief Clears the interning index (TEST ONLY - DO NOT USE IN PRODUCTION)
    /// @warning This method is for testing purposes only and is NOT thread-safe
    /// @note Previously returned string_views REMAIN VALID after clear(): the
    /// underlying storage arena is retained until the StringInterner is
    /// destroyed. clear() only forgets the index, so the same string re-interned
    /// afterwards yields a new (distinct but equal) string_view.
    /// @note This method exists solely to reset state between tests
    void clear();

private:
    /// @brief Interns a copy of sv, returning a view valid for the interner's lifetime
    std::string_view intern_impl(std::string_view sv);

    StringInterner() = default;
    ~StringInterner() = default;
    StringInterner(const StringInterner&) = delete;
    StringInterner& operator=(const StringInterner&) = delete;

    std::unordered_set<std::string_view> interned_strings_;
    std::deque<std::string> storage_;  // stable arena: views stay valid for interner lifetime
    
#ifdef FSMGINE_MULTI_THREADED
    mutable std::mutex mutex_;
#endif
};

} // namespace fsmgine