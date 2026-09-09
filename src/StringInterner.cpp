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

std::string_view StringInterner::intern(const std::string& str) {
    return intern_impl(str);
}

std::string_view StringInterner::intern(std::string_view sv) {
    return intern_impl(sv);
}

void StringInterner::clear() {
    // Testing-only. Forget the index but KEEP the storage arena, so views
    // handed out before clear() remain valid (see header contract). Re-interning
    // the same text afterwards yields a new, distinct-but-equal view.
    interned_strings_.clear();
}

} // namespace fsmgine
