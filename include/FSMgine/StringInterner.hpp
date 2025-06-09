#pragma once

#include <string>
#include <string_view>
#include <unordered_set>

#ifdef FSMGINE_MULTI_THREADED
#include <mutex>
#endif

namespace fsmgine {

class StringInterner {
public:
    static StringInterner& instance();
    
    std::string_view intern(const std::string& str);
    std::string_view intern(std::string_view sv);
    
    // clear() is for testing purposes only and is not thread-safe
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