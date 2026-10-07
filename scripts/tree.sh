#!/usr/bin/env bash
#
# tree — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.tree] cmd = "scripts/tree.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_banner tree
    local rc=0 p dirty probe
    for p in $CI_IGNORED_PATHS; do
        [ -e "$p" ] || continue
        # A directory is checked by probing INSIDE it: `.ci/*` ignores the contents
        # (so git status stays clean) while the tracked baseline keeps the dir itself
        # visible — checking the bare path would call that a failure.
        probe="$p"
        [ -d "$p" ] && probe="$p/.ci-probe"
        if ! git check-ignore -q "$probe" 2>/dev/null; then
            note "NOT ignored: $p — the gate writes here; add it to .gitignore"
            rc=1
        fi
    done
    if [ "$REQUIRE_CLEAN" = "1" ]; then
        dirty=$(git status --porcelain --untracked-files=no | head -20)
        if [ -n "$dirty" ]; then
            note "uncommitted changes to tracked files (--require-clean):"
            printf '%s\n' "$dirty" | sed 's/^/    /'
            rc=1
        fi
    fi
    [ "$rc" = "0" ] && note "gate footprint ignored; nothing uncommitted in tracked files"
    return $rc
}

run_stage "$@"
