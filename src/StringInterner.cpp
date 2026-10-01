#include "FSMgine/StringInterner.hpp"

namespace fsmgine {

StringInterner& StringInterner::instance() {
    static StringInterner instance_;
    return instance_;
}

std::string_view StringInterner::intern_impl(std::string_view sv) {
#ifdef FSMGINE_MULTI_THREADED
    std::lock_guard<std::mutex> lock(mutex_);
#endif

    auto it = interned_strings_.find(sv);
    if (it != interned_strings_.end()) {
        return *it;
    }

    // Append to the stable arena first: deque insertion never invalidates
    // references to existing elements, so every previously returned
    // string_view keeps pointing at valid storage for the interner's lifetime.
    storage_.emplace_back(sv);
    std::string_view stored(storage_.back());
    interned_strings_.insert(stored);
    return stored;
}

std::string_view StringInterner::intern(const std::string& str) { return intern_impl(str); }

std::string_view StringInterner::intern(std::string_view sv) { return intern_impl(sv); }

void StringInterner::resetArena() {
#ifdef FSMGINE_MULTI_THREADED
    std::lock_guard<std::mutex> lock(mutex_);
#endif
    // Release BOTH containers, not just their contents. Swap-with-empty rather
    // than clear()+shrink_to_fit(): shrink_to_fit() is a non-binding request the
    // standard does not require implementations to honour, and for deque it is
    // not honoured in practice. The temporaries own the old blocks and buckets,
    // so both are freed before this function returns.
    std::unordered_set<std::string_view> empty_index;
    interned_strings_.swap(empty_index);

    std::deque<std::string> empty_arena;
    storage_.swap(empty_arena);
}

std::size_t StringInterner::arenaSize() const {
#ifdef FSMGINE_MULTI_THREADED
    std::lock_guard<std::mutex> lock(mutex_);
#endif
    return storage_.size();
}

} // namespace fsmgine
