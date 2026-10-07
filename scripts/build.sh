#!/usr/bin/env bash
#
# build — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.build] cmd = "scripts/build.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_banner build
    have cmake || block "cmake not installed"
    if ! cmake -B "$CI_BUILD_DIR" $CI_CFG_BUILD $CI_CFG_COMMON > "$CI_LOG_DIR/configure.log" 2>&1; then
        tail -25 "$CI_LOG_DIR/configure.log" | sed 's/^/  /'
        note "configure failed (log: $CI_LOG_DIR/configure.log)"
        return 1
    fi
    cmake --build "$CI_BUILD_DIR" -j"$CI_JOBS" > "$CI_LOG_DIR/build.log" 2>&1
    local rc=$? warns
    warns=$(grep -c "warning:" "$CI_LOG_DIR/build.log" 2>/dev/null || true)
    if [ "$rc" != "0" ]; then
        grep -E "error:|warning:" "$CI_LOG_DIR/build.log" | head -25 | sed 's/^/  /'
        note "build failed (log: $CI_LOG_DIR/build.log)"
        return 1
    fi
    if [ "${warns:-0}" != "0" ]; then
        grep "warning:" "$CI_LOG_DIR/build.log" | head -20 | sed 's/^/  /'
        note "$warns warning(s) — counted even where -Werror is not wired onto a target"
        return 1
    fi
    note "configured with a compile database, built clean (-j$CI_JOBS)"
}

run_stage "$@"
