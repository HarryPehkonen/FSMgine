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
/// @note Every public operation takes the interner mutex in the FSMgineMT variant,
///       but the lifetime question is still the caller's: resetArena() invalidates
///       views other threads may be holding, so call it at a quiet point.
// singleton: the one instance is reached through instance(), copied or moved never; copy is
// deleted and the = default destructor is what suppresses implicit moves.
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
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
    /// @note There is no per-name release, by design: the arena is a deque precisely so
    ///       that inserting a name never invalidates views into earlier ones. Memory is
    ///       therefore released either not at all, or entirely, by resetArena(). Intern a
    ///       bounded vocabulary — state names are normally a small fixed set, so the
    ///       arena stays small — and treat unbounded name generation as the case that
    ///       needs resetArena() at a quiet point.
    std::string_view intern(std::string_view sv);

    /// @brief Releases the storage arena, invalidating every previously returned view
    /// @warning Every string_view this interner has ever returned becomes DANGLING.
    ///          Only call this when nothing holds an outstanding view: between
    ///          independent workloads (a fuzzer input, a test case, a batch job).
    /// @note This is the only operation here that releases memory: re-interning the same
    ///       text afterwards yields a new view with the same contents.
    /// @note This is the supported way to release memory, in production as well as in
    ///       tests: the arena is append-only and holds one copy per distinct name ever
    ///       interned, so a long-running program that keeps generating new names grows
    ///       without bound. The safe pattern is a quiet point — destroy every object
    ///       holding a view (machines first: they hold interned state names), then
    ///       reset. fuzz/retention_check.cpp measures the difference on one workload:
    ///       141 MiB of growth without releasing, 1 MiB with resetArena() per input.
    /// @note Nothing calls this for you. The interner is a process-global singleton
    ///       whose (defaulted) destructor releases both containers at program exit;
    ///       between here and there, memory is released all at once or not at all —
    ///       there is no per-name release (see intern()).
    /// @note Takes the interner mutex in the FSMgineMT variant; callers are still
    ///       responsible for having no live views on other threads.
    void resetArena();

    /// @brief Number of strings currently retained in the storage arena
    /// @return The arena size. resetArena() returns this to 0.
    /// @note For tests and diagnostics: this is the memory the interner holds,
    ///       independently of how many names the lookup index currently covers.
    std::size_t arenaSize() const;

private:
    /// @brief Interns a copy of sv, returning a view valid for the interner's lifetime
    std::string_view intern_impl(std::string_view sv);

    StringInterner() = default;
    ~StringInterner() = default;
    StringInterner(const StringInterner&) = delete;
    StringInterner& operator=(const StringInterner&) = delete;

    std::unordered_set<std::string_view> interned_strings_;
    std::deque<std::string> storage_; // stable arena: views stay valid for interner lifetime

#ifdef FSMGINE_MULTI_THREADED
    mutable std::mutex mutex_;
#endif
};

} // namespace fsmgine