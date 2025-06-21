/// @file StringInterner.hpp
/// @brief String interning utility for memory-efficient state name storage
/// @ingroup utilities

#pragma once

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
/// - Thread-safe operations when compiled with FSMGINE_MULTI_THREADED
/// 
/// @note This is a singleton class - use StringInterner::instance() to access
/// 
/// @par Thread Safety
/// When compiled with FSMGINE_MULTI_THREADED defined, all operations are thread-safe.
/// The clear() method is an exception and should only be used in single-threaded tests.
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
    
    /// @brief Clears all interned strings (TEST ONLY - DO NOT USE IN PRODUCTION)
    /// @warning This method is for testing purposes only and is NOT thread-safe
    /// @warning After calling clear(), all previously returned string_views become invalid
    /// @warning Using this in production code will cause undefined behavior
    /// @note This method exists solely to reset state between tests
    void clear();

private:
    StringInterner() = default;
    ~StringInterner() = default;
    StringInterner(const StringInterner&) = delete;
    StringInterner& operator=(const StringInterner&) = delete;

    std::unordered_set<std::string> interned_strings_;
    
#ifdef FSMGINE_MULTI_THREADED
    mutable std::mutex mutex_;
#endif
};

} // namespace fsmgine