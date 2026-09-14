// Retention check — regression test for the 2026-09-13 out-of-memory finding.
//
// Why this exists
// ---------------
// StringInterner is a process-global singleton whose storage arena is append-only
// by design: `clear()` resets the lookup index but deliberately KEEPS the storage
// so that every view handed out earlier stays valid for the interner's lifetime.
// A fuzz target that interns *unboundedly many distinct* names therefore grows
// resident memory for as long as it runs. On 2026-09-13 that is exactly what
// happened: the FSMgine target hit libFuzzer's own guard after 12 minutes —
//
//     ERROR: libFuzzer: out-of-memory (used: 2050Mb; limit: 2048Mb)
//
// What it asserts
// ---------------
// Driving LLVMFuzzerTestOneInput with many inputs must not grow resident memory
// without bound — which means the harness has to draw its names from a bounded
// vocabulary. This test FAILS against an unbounded harness and passes once
// fuzz/fuzz_fsm.cpp bounds its name space, so it is the guard against someone
// re-introducing unbounded name generation. Memory is measured, not assumed.
//
// Build (fuzzing configuration):
//   cmake -B build-fuzz -DFSMGINE_BUILD_FUZZING=ON \
//         -DFSMGINE_BUILD_MULTITHREADED=OFF -DBUILD_TESTING=OFF \
//         -DCMAKE_CXX_COMPILER=clang++ ..
//   cmake --build build-fuzz --target retention_check
//   ./build-fuzz/fuzz/retention_check
//
// Resident memory comes from /proc/self/statm, so this is Linux-only — fine for a
// libFuzzer target. The fuzzing build instruments the library with ASan/UBSan;
// ASan's quarantine reaches a steady state early, so the assertion is on the
// growth between two long halves of the run (after a warm-up) rather than on
// absolute RSS.

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);

namespace {

constexpr std::size_t kInputBytes = 2048;
constexpr std::size_t kWarmupInputs = 5000;
constexpr std::size_t kTotalInputs = 30000;

// Growth allowed between the two halves of the run. Bounded-name harnesses sit
// near zero; an unbounded one grows with the number of distinct names interned.
constexpr long kMaxGrowthKib = 96 * 1024;  // 96 MiB

// Resident set size in KiB (second field of statm is resident pages).
long rss_kib() {
    static const long page_kib =
        static_cast<long>(::sysconf(_SC_PAGESIZE)) / 1024;
    std::ifstream statm("/proc/self/statm");
    long total_pages = 0;
    long resident_pages = 0;
    statm >> total_pages >> resident_pages;
    return resident_pages * page_kib;
}

std::uint32_t next(std::uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return state;
}

void fill(std::vector<std::uint8_t>& buf, std::uint32_t& state) {
    for (std::uint8_t& byte : buf) {
        byte = static_cast<std::uint8_t>(next(state) >> 24);
    }
}

}  // namespace

int main() {
    std::vector<std::uint8_t> buf(kInputBytes);
    std::uint32_t state = 0x12345678u;

    for (std::size_t i = 0; i < kWarmupInputs; ++i) {
        fill(buf, state);
        LLVMFuzzerTestOneInput(buf.data(), buf.size());
    }

    const long before = rss_kib();
    for (std::size_t i = kWarmupInputs; i < kTotalInputs; ++i) {
        fill(buf, state);
        LLVMFuzzerTestOneInput(buf.data(), buf.size());
    }
    const long after = rss_kib();

    const long growth = after - before;
    std::cout << "Retention check: " << kTotalInputs << " inputs x " << kInputBytes
              << " bytes; RSS " << before / 1024 << " MiB -> " << after / 1024
              << " MiB; growth " << growth / 1024 << " MiB (limit "
              << kMaxGrowthKib / 1024 << " MiB)" << std::endl;

    if (growth > kMaxGrowthKib) {
        std::cout << "FAIL: resident memory grew without bound - the target's name "
                     "generation is not bounded (see StringInterner::clear(): the "
                     "arena is never released)." << std::endl;
        return 1;
    }

    std::cout << "PASS: resident memory stayed bounded" << std::endl;
    return 0;
}
