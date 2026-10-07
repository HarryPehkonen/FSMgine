#!/usr/bin/env bash
#
# tests — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.tests] cmd = "scripts/tests.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_banner tests
    have ctest || block "ctest not installed"
    if ! $CI_TEST_CMD; then
        note "tests failed (above)"
        return 1
    fi
    note "all tests passed"
}

run_stage "$@"
