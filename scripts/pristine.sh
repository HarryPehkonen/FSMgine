#!/usr/bin/env bash
#
# pristine — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.pristine] cmd = "scripts/pristine.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_banner pristine
    local tmp rc
    tmp=$(mktemp -d)
    if ! git archive HEAD | tar -x -C "$tmp"; then note "could not export HEAD"; rm -rf "$tmp"; return 1; fi
    (
        cd "$tmp" || exit 1
        cmake -B build -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1 &&
            cmake --build build -j"$CI_JOBS" >/dev/null 2>&1 &&
            ctest --test-dir build --output-on-failure
    ) > "$CI_LOG_DIR/pristine.log" 2>&1
    rc=$?
    if [ "$rc" != "0" ]; then
        tail -20 "$CI_LOG_DIR/pristine.log" | sed 's/^/  /'
        note "the committed tree does NOT build+test clean (log: $CI_LOG_DIR/pristine.log)"
    fi
    rm -rf "$tmp"
    [ "$rc" = "0" ] && note "a clean checkout of HEAD builds and passes its tests"
    return $rc
}

run_stage "$@"
