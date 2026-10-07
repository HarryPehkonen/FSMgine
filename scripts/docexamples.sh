#!/usr/bin/env bash
#
# docexamples — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.docexamples] cmd = "scripts/docexamples.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_banner docexamples
    have python3 || block "python3 not installed (the doc-example checker needs it)"
    have g++ || block "g++ not installed"
    # A documented example is an executable claim. Every C++ block in README.md,
    # CLAUDE.md and include/**.hpp must compile, or an agent copies broken code.
    python3 "$REPO_ROOT/tools/check_doc_examples.py" > "$CI_LOG_DIR/docexamples.log" 2>&1
    local rc=$?
    tail -14 "$CI_LOG_DIR/docexamples.log" | sed 's/^/  /'
    if [ "$rc" != "0" ]; then
        note "a documented example does not compile (log: $CI_LOG_DIR/docexamples.log)"
        return 1
    fi
    note "every documented example compiles"
}

run_stage "$@"
