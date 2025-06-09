#include "FSMgine/StringInterner.hpp"

namespace fsmgine {

StringInterner& StringInterner::instance() {
    static StringInterner instance_;
    return instance_;
}

std::string_view StringInterner::intern(const std::string& str) {
#ifdef FSMGINE_MULTI_THREADED
    std::lock_guard<std::mutex> lock(mutex_);
#endif
    
    auto [it, inserted] = interned_strings_.insert(str);
    return std::string_view(*it);
}

std::string_view StringInterner::intern(std::string_view sv) {
#ifdef FSMGINE_MULTI_THREADED
    std::lock_guard<std::mutex> lock(mutex_);
#endif
    
    auto [it, inserted] = interned_strings_.emplace(sv);
    return std::string_view(*it);
}

void StringInterner::clear() {
    // Note: This is not thread-safe and is intended for testing only
    interned_strings_.clear();
}

} // namespace fsmgine