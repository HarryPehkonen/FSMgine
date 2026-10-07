#!/usr/bin/env bash
#
# selftest — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.selftest] cmd = "scripts/selftest.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_banner selftest
    # A gate must survive a commit that DELETES a source file. The changed-file scope once handed
    # clang-format a path this run had removed, so a deletion failed the format stage for the
    # wrong reason (hit on the v2.0.0 release commit). tools/ci_selftest.sh asserts both
    # directions — the fix works, and the pre-fix line still fails — so it cannot rot.
    [ -x "$REPO_ROOT/tools/ci_selftest.sh" ] || block "tools/ci_selftest.sh is missing or not executable"
    if ! "$REPO_ROOT/tools/ci_selftest.sh" > "$CI_LOG_DIR/selftest.log" 2>&1; then
        tail -8 "$CI_LOG_DIR/selftest.log" >&2 || true
        note "the gate's own changed-file scope is broken (log: $CI_LOG_DIR/selftest.log)"
        return 1
    fi
    note "the changed-file scope survives a deleted source file"
}

run_stage "$@"
