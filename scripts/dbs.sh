#!/usr/bin/env bash
#
# dbs — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.dbs] cmd = "scripts/dbs.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_banner dbs
    have cmake || block "cmake not installed"
    # Configure-only, purely so every compile database EXISTS. The lint stage unions them
    # and fails on a tracked source that no database covers — but a database written by a
    # LATER stage would make that check report files it simply had not seen yet. Ordering
    # the gate around a check is the wrong fix; making the check's inputs exist is the
    # right one. Each configure is ~0.05 s.
    local d ok=1
    for d in "$CI_BUILD_DIR:$CI_CFG_BUILD" "$CI_RELEASE_BUILD_DIR:$CI_CFG_RELEASE" \
             "$CI_FUZZ_BUILD_DIR:$CI_CFG_FUZZ"; do
        # shellcheck disable=SC2086
        cmake -B "${d%%:*}" ${d##*:} $CI_CFG_COMMON > "$CI_LOG_DIR/dbs-$(basename "${d%%:*}").log" 2>&1 || {
            note "configure failed for ${d%%:*} (log: $CI_LOG_DIR/dbs-$(basename "${d%%:*}").log)"
            ok=0
        }
    done
    [ "$ok" = "1" ] || return 1
    note "compile databases ready for the lint stage"
    return 0
}

run_stage "$@"
