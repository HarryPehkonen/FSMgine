#!/usr/bin/env bash
#
# version — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.version] cmd = "scripts/version.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_banner version
    have git || block "git not installed"
    # The declared version must never be BEHIND the newest tag: tags are what a consumer
    # resolves, and this repo once declared 1.0.1 while v1.3.1 was already tagged, which
    # made "the next minor bump" ambiguous. A clone without tags skips with a note, so a
    # shallow checkout is not failed for a check it cannot run.
    local declared newest
    declared=$(sed -n 's/^project(FSMgine VERSION \([0-9][0-9.]*\).*/\1/p' "$REPO_ROOT/CMakeLists.txt" | head -1)
    [ -n "$declared" ] || {
        note "no version found in CMakeLists.txt"
        return 1
    }
    newest=$(cd "$REPO_ROOT" && git describe --tags --abbrev=0 --match 'v[0-9]*' 2>/dev/null | sed 's/^v//')
    if [ -z "$newest" ]; then
        note "declared $declared; no v* tag in this clone, so drift is not checked"
        return 0
    fi
    note "declared $declared, newest tag v$newest"
    if [ "$(printf '%s\n%s\n' "$newest" "$declared" | sort -V | tail -1)" != "$declared" ]; then
        note "CMakeLists.txt declares $declared but v$newest is already tagged: bump the declared version"
        return 1
    fi
    # A tag sitting on HEAD that disagrees with the declared version is exactly the drift
    # this stage exists for: the tag and the version are written at the same moment.
    local head_tag
    head_tag=$(cd "$REPO_ROOT" && git describe --tags --exact-match --match 'v[0-9]*' 2>/dev/null || true)
    if [ -n "$head_tag" ] && [ "${head_tag#v}" != "$declared" ]; then
        note "HEAD is tagged $head_tag but CMakeLists.txt declares $declared"
        return 1
    fi
    # The release tooling is part of the process, so it must at least run.
    [ -x "$REPO_ROOT/tools/release.sh" ] || block "tools/release.sh is missing or not executable"
    if ! "$REPO_ROOT/tools/release.sh" status > "$CI_LOG_DIR/release-status.log" 2>&1; then
        note "tools/release.sh status failed (log: $CI_LOG_DIR/release-status.log)"
        return 1
    fi
    grep -q "declared version" "$CI_LOG_DIR/release-status.log" || {
        note "tools/release.sh status printed no version line"
        return 1
    }
    note "release tooling runs; $(sed -n 's/^  newest tag       : /newest tag /p' "$CI_LOG_DIR/release-status.log" | head -1)"
}

run_stage "$@"
