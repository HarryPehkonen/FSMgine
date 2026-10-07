#!/usr/bin/env bash
#
# lint — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.lint] cmd = "scripts/lint.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_banner lint
    have clang-tidy || { [ "$CI_STRICT_TOOLS" = "1" ] && block "clang-tidy not installed"; note "SKIP: clang-tidy not installed"; return 0; }
    have python3 || block "python3 not installed (needed to read the compile database)"
    [ -f .clang-tidy ] || block "no .clang-tidy in the repo root — the lint stage has no definition"
    local db tus
    tus=""
    for db in $CI_LINT_DBS; do
        [ -f "$db" ] || continue
        # ABSOLUTE paths here on purpose: clang-tidy maps the file back to its compile
        # command via that exact string, so feeding it a rewritten path finds no entry.
        tus="$tus$(python3 -c "import json,sys;print('\n'.join(sorted({e['file'] for e in json.load(open(sys.argv[1]))})))" "$db" || true)
"
    done
    tus=$(printf '%s' "$tus" | sort -u | grep -E '/(src|tests|fuzz|examples|benchmarks)/' || true)
    # A database that EXISTS may not COVER your sources (JSOM's scoped export taught
    # this): assert before trusting a clean result.
    [ -n "$tus" ] || block "no compile database covered any of this repo's sources"
    # Subject coverage, the same lesson one level up: a tracked .cpp that no target
    # compiles cannot be linted, so a clean lint that simply excludes it is a false pass.
    # Exemptions must be named in CI_UNBUILT_OK, not achieved by silence. (Comparison is
    # in repo-relative paths; the TU list stays absolute for clang-tidy.)
    local uncovered tracked covered
    tracked=$(git ls-files '*.cpp' '*.cc' '*.cxx' | grep -vE "$CI_SOURCE_EXCLUDE" || true)
    covered=$( { printf '%s\n' $tus | sed "s|^$REPO_ROOT/||"; printf '%s\n' $CI_UNBUILT_OK | tr ' ' '\n'; } | sort -u)
    uncovered=$(comm -23 <(printf '%s\n' $tracked | sort -u) <(printf '%s\n' "$covered") || true)
    if [ -n "$uncovered" ]; then
        note "tracked .cpp file(s) that NO target compiles — unlintable, so not a pass:"
        printf '%s\n' $uncovered | sed 's/^/    /'
        note "wire them into a target, or list them in CI_UNBUILT_OK with a reason"
        return 1
    fi
    # Never let an exemption be invisible: it is printed on every green run.
    [ -n "$CI_UNBUILT_OK" ] && note "exempt from compilation (see CI_UNBUILT_OK): $(printf '%s ' $CI_UNBUILT_OK)"
    if [ "$SCOPE_ALL" = "0" ]; then
        local changed keep="" t base
        changed=$(scoped_sources)
        for t in $tus; do
            base=$(basename "$t")
            printf '%s\n' "$changed" | grep -q "$base" && keep="$keep $t"
        done
        tus=$(printf '%s\n' $keep)
        [ -n "$tus" ] || { note "no changed translation unit in the database"; return 0; }
    fi
    local checks="-*,bugprone-*,-bugprone-easily-swappable-parameters,cppcoreguidelines-*,-cppcoreguidelines-avoid-magic-numbers,-cppcoreguidelines-non-private-member-variables-in-classes,performance-*${EXTRA_CHECKS}"
    printf '%s\n' $tus | xargs -P"$CI_JOBS" -I{} clang-tidy -p "$CI_BUILD_DIR" -checks="$checks" {} \
        > "$CI_LOG_DIR/tidy.log" 2>&1
    local n
    n=$(grep -c "warning:" "$CI_LOG_DIR/tidy.log" 2>/dev/null || true)
    # Keys are path:check — deliberately NOT line numbers. This repo was reformatted
    # in one commit, which shifts every line in every file, so a line-keyed baseline
    # would be dead on arrival. A key that reappears in the same file under the same
    # check counts as inherited; any other key is a NEW finding and fails the gate.
    grep "warning:" "$CI_LOG_DIR/tidy.log" 2>/dev/null \
        | sed -E "s|^${REPO_ROOT}/||" \
        | sed -E 's/:[0-9]+:[0-9]+: warning:.*\[([^]]+)\]$/:\1/' \
        | grep -E '^[^:]+\.(cpp|hpp|h|cc|cxx|hh):' \
        | sort -u > "$CI_LOG_DIR/tidy-keys.txt"

    if [ "$WRITE_BASELINE" = "1" ]; then
        mkdir -p "$(dirname "$CI_TIDY_BASELINE")"
        {
            printf '# Inherited clang-tidy findings, accepted when the gate was installed (%s).\n' "$(date +%F)"
            printf '#\n'
            printf '# Key = path:check. Line numbers are excluded on purpose: they move on every\n'
            printf '# reformat, and a line-keyed baseline would stop matching without saying so.\n'
            printf '#\n'
            printf '# Any key NOT in this file FAILS the gate, so this is a debt to pay down, not a\n'
            printf '# permission slip: fix the finding and delete its line, or — when the deviation is\n'
            printf '# deliberate — suppress it AT THE SITE with a reason. .clang-tidy says which\n'
            printf '# mechanism to prefer for which shape of finding, and why.\n'
            printf '#\n'
            printf '# This file is NOT the whole story: deviations attached to a named entity are\n'
            printf '# suppressed at the site, where the next reader of the header sees them, so they\n'
            printf '# never appear here. A key here is also SPENT once a file has one accepted finding\n'
            printf '# — a second problem of the same kind in that file would be invisible.\n'
            printf '#\n'
            printf '# Prune with: scripts/write-tidy-baseline.sh lint. The lint stage prints any key\n'
            printf '# that no longer fires, so rot shows up rather than staying silent.\n'
            cat "$CI_LOG_DIR/tidy-keys.txt"
        } > "$CI_TIDY_BASELINE"
        note "wrote $(grep -vc '^#' "$CI_TIDY_BASELINE") inherited finding key(s) to $CI_TIDY_BASELINE"
        note "inventory (the debt, grouped):"
        grep -v '^#' "$CI_TIDY_BASELINE" | sed -E 's/^.*://' | sort | uniq -c | sort -rn | sed 's/^/    /'
        return 0
    fi

    local newfindings=""
    if [ -f "$CI_TIDY_BASELINE" ]; then
        newfindings=$(comm -23 "$CI_LOG_DIR/tidy-keys.txt" \
                             <(grep -v '^#' "$CI_TIDY_BASELINE" | sort -u))
    else
        newfindings=$(cat "$CI_LOG_DIR/tidy-keys.txt")
    fi
    if [ -n "$newfindings" ]; then
        printf '%s\n' "$newfindings" | sed 's/^/  NEW finding: /'
        # Show the MESSAGE for the new ones only: a wall of inherited warnings buries
        # the two lines that actually failed the stage.
        while read -r key; do
            [ -n "$key" ] || continue
            grep -F "${key%%:*}" "$CI_LOG_DIR/tidy.log" | grep -F "[${key##*:}]" \
                | head -3 | sed 's/^/    /'
        done <<< "$newfindings"
        note "full log: $CI_LOG_DIR/tidy.log — accept inherited ones with: scripts/write-tidy-baseline.sh lint"
        return 1
    fi
    note "0 new findings across $(printf '%s\n' $tus | wc -l) translation unit(s) \
($(grep -vc '^#' "$CI_TIDY_BASELINE" 2>/dev/null || echo 0) inherited key(s) in $CI_TIDY_BASELINE)"
    # Self-audit the ledger. A key that no longer fires is rot: either the finding was
    # fixed (prune it) or a NOLINT now suppresses it (which should say so at the site).
    # Without this, both mechanisms rot in silence — measured: clang-tidy does not warn
    # about a NOLINT that suppresses nothing.
    # Only meaningful on a FULL subject: with --changed, lint analyses just the changed
    # translation units, so every other file's key looks stale. Seen in the commit tier:
    # 8 example mains reported stale while nothing was wrong with them.
    local stale=""
    if [ "$SCOPE_ALL" = "1" ] && [ -f "$CI_TIDY_BASELINE" ]; then
        stale=$(comm -13 "$CI_LOG_DIR/tidy-keys.txt" \
                     <(grep -v '^#' "$CI_TIDY_BASELINE" | sort -u) || true)
    fi
    if [ -n "$stale" ]; then
        note "$(printf '%s\n' "$stale" | wc -l) baseline key(s) no longer fire — prune: scripts/write-tidy-baseline.sh lint"
        printf '%s\n' "$stale" | sed 's/^/    stale: /'
    fi
}

run_stage "$@"
