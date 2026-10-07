#!/usr/bin/env bash
#
# asan — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.asan] cmd = "scripts/asan.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_banner asan
    have clang++ || block "clang++ not installed (sanitizers need it here)"
    if ! cmake -B "$CI_ASAN_BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug \
               -DCMAKE_CXX_COMPILER=clang++ \
               -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
               -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" \
               > "$CI_LOG_DIR/asan-configure.log" 2>&1; then
        tail -20 "$CI_LOG_DIR/asan-configure.log" | sed 's/^/  /'; return 1
    fi
    if ! cmake --build "$CI_ASAN_BUILD_DIR" -j"$CI_JOBS" > "$CI_LOG_DIR/asan-build.log" 2>&1; then
        grep -E "error:" "$CI_LOG_DIR/asan-build.log" | head -20 | sed 's/^/  /'; return 1
    fi
    if ! ctest --test-dir "$CI_ASAN_BUILD_DIR" --output-on-failure -j"$CI_JOBS" > "$CI_LOG_DIR/asan-test.log" 2>&1; then
        tail -30 "$CI_LOG_DIR/asan-test.log" | sed 's/^/  /'
        note "sanitizer run failed (log: $CI_LOG_DIR/asan-test.log)"
        return 1
    fi
    note "clean under ASan + UBSan"
}

run_stage "$@"
