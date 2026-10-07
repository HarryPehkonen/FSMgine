#!/usr/bin/env bash
#
# release — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.release] cmd = "scripts/release.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_banner release
    have cmake || block "cmake not installed"
    have "$CI_RELEASE_BUILD_CXX" || block "$CI_RELEASE_BUILD_CXX not installed"
    if ! cmake -B "$CI_RELEASE_BUILD_DIR" $CI_CFG_RELEASE $CI_CFG_COMMON \
               > "$CI_LOG_DIR/release-configure.log" 2>&1; then
        tail -20 "$CI_LOG_DIR/release-configure.log" | sed 's/^/  /'
        note "release configure failed"
        return 1
    fi
    cmake --build "$CI_RELEASE_BUILD_DIR" -j"$CI_JOBS" > "$CI_LOG_DIR/release-build.log" 2>&1
    local rc=$? warns
    warns=$(grep -c "warning:" "$CI_LOG_DIR/release-build.log" 2>/dev/null || true)
    if [ "$rc" != "0" ] || [ "${warns:-0}" != "0" ]; then
        grep -E "error:|warning:" "$CI_LOG_DIR/release-build.log" | head -25 | sed 's/^/  /'
        note "$CI_RELEASE_BUILD_CXX -O2 is not clean (log: $CI_LOG_DIR/release-build.log)"
        return 1
    fi
    if ! ctest --test-dir "$CI_RELEASE_BUILD_DIR" --output-on-failure -j"$CI_JOBS" \
               > "$CI_LOG_DIR/release-test.log" 2>&1; then
        tail -25 "$CI_LOG_DIR/release-test.log" | sed 's/^/  /'
        note "release tests failed (log: $CI_LOG_DIR/release-test.log)"
        return 1
    fi
    note "$CI_RELEASE_BUILD_CXX -O2: warning-free, tests pass"
}

run_stage "$@"
