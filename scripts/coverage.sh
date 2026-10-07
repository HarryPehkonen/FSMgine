#!/usr/bin/env bash
#
# coverage — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.coverage] cmd = "scripts/coverage.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_banner coverage
    have clang++ || block "clang++ not installed (source-based coverage needs it)"
    have python3 || block "python3 not installed (the report parser needs it)"
    # On Debian these ship with clang but off PATH, hence the glob.
    local cov profdata
    cov=$(command -v llvm-cov 2>/dev/null || ls /usr/lib/llvm-*/bin/llvm-cov 2>/dev/null | head -1)
    profdata=$(command -v llvm-profdata 2>/dev/null || ls /usr/lib/llvm-*/bin/llvm-profdata 2>/dev/null | head -1)
    [ -n "$cov" ] && [ -n "$profdata" ] || block "llvm-cov/llvm-profdata not found (no coverage tool)"
    if ! cmake -B "$CI_COV_BUILD_DIR" $CI_CFG_BUILD -DCMAKE_CXX_COMPILER=clang++ \
               -DCMAKE_CXX_FLAGS="-fprofile-instr-generate -fcoverage-mapping -g -O0" \
               -DCMAKE_EXE_LINKER_FLAGS="-fprofile-instr-generate" \
               > "$CI_LOG_DIR/cov-configure.log" 2>&1; then
        tail -15 "$CI_LOG_DIR/cov-configure.log" | sed 's/^/  /'
        note "coverage configure failed"
        return 1
    fi
    if ! cmake --build "$CI_COV_BUILD_DIR" -j"$CI_JOBS" > "$CI_LOG_DIR/cov-build.log" 2>&1; then
        grep -E "error:" "$CI_LOG_DIR/cov-build.log" | head -15 | sed 's/^/  /'
        note "instrumented build failed"
        return 1
    fi
    local bin prof
    bin=$(find "$CI_COV_BUILD_DIR" -type f -perm -u+x -name "$CI_COV_TEST_BIN" | head -1)
    [ -n "$bin" ] || { note "no instrumented test binary matching $CI_COV_TEST_BIN"; return 1; }
    # ABSOLUTE, not relative: the instrumented binary runs from its own working
    # directory (ctest sets one), so a relative LLVM_PROFILE_FILE lands somewhere else
    # and the stage reports "no profile" while the tests clearly ran.
    prof="$REPO_ROOT/$CI_LOG_DIR/coverage.profraw"
    rm -f "$prof"
    if ! LLVM_PROFILE_FILE="$prof" ctest --test-dir "$CI_COV_BUILD_DIR" --output-on-failure -j"$CI_JOBS" \
         > "$CI_LOG_DIR/cov-test.log" 2>&1; then
        tail -20 "$CI_LOG_DIR/cov-test.log" | sed 's/^/  /'
        note "the suite failed under instrumentation (log: $CI_LOG_DIR/cov-test.log)"
        return 1
    fi
    # No profile means the instrumented code never ran. Report that as unknown — never
    # as 0% coverage (the same rule as "a void is not resistance").
    [ -s "$prof" ] || { note "no profile was written: instrumented code did not execute"; return 1; }
    "$profdata" merge -sparse "$prof" -o "$CI_LOG_DIR/coverage.profdata" > /dev/null 2>&1 \
        || { note "llvm-profdata merge failed"; return 1; }
    if ! "$cov" report "$bin" -instr-profile="$CI_LOG_DIR/coverage.profdata" \
         > "$CI_LOG_DIR/coverage-report.txt" 2>&1; then
        tail -10 "$CI_LOG_DIR/coverage-report.txt" | sed 's/^/  /'
        note "llvm-cov report failed"
        return 1
    fi
    if [ "$WRITE_COV_BASELINE" = "1" ]; then
        python3 "$REPO_ROOT/tools/coverage_gate.py" --report "$CI_LOG_DIR/coverage-report.txt" \
            $CI_COV_SUBJECT --write "$CI_COV_BASELINE" || return 1
        note "recorded the coverage baseline in $CI_COV_BASELINE"
    else
        python3 "$REPO_ROOT/tools/coverage_gate.py" --report "$CI_LOG_DIR/coverage-report.txt" \
            $CI_COV_SUBJECT --baseline "$CI_COV_BASELINE" || return 1
        note "no file lost coverage (baseline: $CI_COV_BASELINE)"
    fi
    return 0
}

run_stage "$@"
