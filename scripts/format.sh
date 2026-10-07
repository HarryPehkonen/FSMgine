#!/usr/bin/env bash
#
# format — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.format] cmd = "scripts/format.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_banner format
    have clang-format || { [ "$CI_STRICT_TOOLS" = "1" ] && block "clang-format not installed"; note "SKIP: clang-format not installed"; return 0; }
    [ -f .clang-format ] || block "no .clang-format in the repo root — the format stage has no definition"
    local files bad=0 out
    files=$(scoped_sources)
    if [ -z "$files" ]; then note "no source files in scope"; return 0; fi
    for f in $files; do
        if ! out=$(clang-format --style=file --dry-run --Werror "$f" 2>&1); then
            bad=$((bad + 1))
            printf '%s\n' "$out" | head -4 | sed 's/^/  /'
        fi
    done
    # The loop above read the WORKING TREE, and a commit records the INDEX. Stage an unformatted file,
    # then format it on disk -- what anyone does after this stage rejects a commit -- and the loop passes
    # while the commit still records the unformatted text; HEAD and the working tree then differ, and the
    # next --require-clean push fails in `tree` with "uncommitted changes to tracked files", a message
    # that never mentions formatting. Kit fix `format-checks-staged` (docs/KIT-FIXES.md, kit 96719c4).
    local -a staged=() index_drift=()
    local staged_f
    while IFS= read -r staged_f; do
        [ -n "$staged_f" ] && [ -f "$staged_f" ] && staged+=("$staged_f")
    done < <(git diff --cached --name-only --diff-filter=ACMR -- 2>/dev/null | grep -E '\.(cpp|cc|cxx|hpp|hh|h)$' || true)
    if [ "${#staged[@]}" -gt 0 ]; then
        for staged_f in "${staged[@]}"; do
            git show ":$staged_f" 2>/dev/null |
                clang-format --style=file --dry-run --Werror --assume-filename="$staged_f" - > /dev/null 2>&1 ||
                index_drift+=("$staged_f")
        done
        if [ "${#index_drift[@]}" -gt 0 ]; then
            note "the STAGED copy is not formatted (that is what the commit would record): $(printf '%s ' "${index_drift[@]}")"
            note "fix with: clang-format -i <file>... && git add <file>..."
            return 1
        fi
    fi

    if [ "$bad" != "0" ]; then
        note "$bad file(s) would be reformatted. Fix with: clang-format -i <file>..."
        return 1
    fi
    note "no drift across $(printf '%s\n' $files | wc -l) file(s)"
}

run_stage "$@"
