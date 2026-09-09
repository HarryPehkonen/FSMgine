// FSMgine fuzz target: stateful driver for the FSM engine + StringInterner.
//
// FSMgine has no text parser (the "input" is the API), so the input byte
// stream is interpreted as a sequence of operations: interning names,
// clearing the interner, building machines/transitions, and stepping them.
// This exercises the ownership/lifetime paths of the FSM engine and the
// interner contract (views surviving clear()) under ASan/UBSan.
//
// Build with clang (libFuzzer):
//   cmake -B build-fuzz -DFSMGINE_BUILD_FUZZING=ON \
//         -DFSMGINE_BUILD_MULTITHREADED=OFF -DBUILD_TESTING=OFF \
//         -DCMAKE_CXX_COMPILER=clang++ ..
//   cmake --build build-fuzz --target fuzz_fsmgine
//   ./build-fuzz/fuzz/fuzz_fsmgine
//
// All library exceptions are expected and caught; a crash or sanitizer
// report means a real bug.

#include "FSMgine/FSMgine.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

using fsmgine::FSM;
using fsmgine::StringInterner;

// Bounded pools so a long fuzz run cannot grow without limit.
constexpr size_t kMaxNames = 64;
constexpr size_t kMaxViews = 32;

std::vector<std::string> names_;        // raw names (input-derived)
std::vector<std::string_view> views_;   // views handed out by the interner
std::unique_ptr<FSM<int>> fsm_;

struct Reader {
    const uint8_t* p;
    size_t n;

    uint8_t u8() {
        if (n == 0) {
            return 0;
        }
        const uint8_t v = *p;
        ++p;
        --n;
        return v;
    }

    // 1..16 hex characters derived from the input stream.
    std::string name() {
        std::string s;
        const size_t take = static_cast<size_t>(u8() % 16) + 1;
        for (size_t j = 0; j < take && n > 0; ++j) {
            static constexpr char kHex[] = "0123456789abcdef";
            s.push_back(kHex[u8() & 0x0f]);
        }
        return s.empty() ? std::string("s") : s;
    }
};

std::string_view pick(Reader& r, const std::vector<std::string>& from) {
    if (from.empty()) {
        return "state";
    }
    return from[static_cast<size_t>(r.u8()) % from.size()];
}

void ensure_fsm() {
    if (!fsm_) {
        fsm_ = std::make_unique<FSM<int>>();
    }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }

    Reader r{data, size};
    auto& interner = StringInterner::instance();

    while (r.n > 0) {
        const uint8_t op = r.u8() & 0x07;
        try {
            switch (op) {
                case 0: {  // intern a fresh name; retain the returned view
                    const std::string name = r.name();
                    std::string_view v = interner.intern(name);
                    if (views_.size() >= kMaxViews) {
                        views_.clear();
                    }
                    views_.push_back(v);
                    break;
                }
                case 1:  // reset the interning index (views must stay valid)
                    interner.clear();
                    break;
                case 2: {  // (re)build the live machine with a random transition
                    ensure_fsm();
                    const std::string from = r.name();
                    const std::string to = r.name();
                    auto builder = fsm_->get_builder();
                    builder.from(from)
                        .predicate([](const int& e) { return (e & 1) == 0; })
                        .to(to);
                    fsm_->setInitialState(from);
                    break;
                }
                case 3:  // jump to a state by name (exceptions expected)
                    ensure_fsm();
                    fsm_->setCurrentState(pick(r, names_));
                    break;
                case 4:  // read current state (must not crash even when unset)
                    ensure_fsm();
                    (void)fsm_->getCurrentState();
                    break;
                case 5: {  // step with a pseudo-random event
                    ensure_fsm();
                    const int event = static_cast<int>(r.u8());
                    (void)fsm_->process(event);
                    break;
                }
                case 6: {  // keep a small pool of names around
                    const std::string name = r.name();
                    if (names_.size() >= kMaxNames) {
                        names_.clear();
                    }
                    names_.push_back(name);
                    break;
                }
                case 7:  // fresh machine; also validate move semantics path
                    fsm_ = std::make_unique<FSM<int>>();
                    break;
            }
        } catch (const std::exception&) {
            // Expected: state-not-found, not-initialized, invalid-argument…
            // All library errors are exceptions; none should escape the target.
        }
    }
    return 0;
}
