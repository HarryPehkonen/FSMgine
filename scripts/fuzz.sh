#!/usr/bin/env bash
#
# fuzz — split out of the old tools/ci.sh (2026-10-06, card t_075c0a6f).
#
# Called from gate.toml as `[stage.fuzz] cmd = "scripts/fuzz.sh"`. The verdict
# vocabulary (ci_begin/ci_pass/ci_fail/ci_skip) is defined in scripts/gate-env.sh.
set -uo pipefail
. "$(dirname "$0")/gate-env.sh"

run_stage() {
    ci_banner fuzz
    have clang++ || block "clang++ not installed (libFuzzer needs clang)"
    if ! cmake -B "$CI_FUZZ_BUILD_DIR" $CI_CFG_FUZZ $CI_CFG_COMMON \
               > "$CI_LOG_DIR/fuzz-configure.log" 2>&1; then
        tail -20 "$CI_LOG_DIR/fuzz-configure.log" | sed 's/^/  /'; return 1
    fi
    if ! cmake --build "$CI_FUZZ_BUILD_DIR" --target fuzz_fsmgine -j"$CI_JOBS" \
               > "$CI_LOG_DIR/fuzz-build.log" 2>&1; then
        grep -E "error:" "$CI_LOG_DIR/fuzz-build.log" | head -20 | sed 's/^/  /'; return 1
    fi
    local exe="$CI_FUZZ_BUILD_DIR/fuzz_fsmgine"
    [ -x "$exe" ] || block "$exe was not produced"
    # The corpus lives INSIDE a gitignored build dir on purpose: a corpus written
    # into the tracked tree would make --require-clean fail during a push.
    local corpus="$CI_FUZZ_BUILD_DIR/corpus" before after rc
    mkdir -p "$corpus"
    before=$(ls -1 "$corpus" 2>/dev/null | grep -cE '^(crash|oom|timeout|leak)-' || true)
    "$exe" "$corpus" -max_total_time="$CI_FUZZ_SECONDS" -timeout=10 \
        -artifact_prefix="$corpus/" > "$CI_LOG_DIR/fuzz.log" 2>&1
    rc=$?
    after=$(ls -1 "$corpus" 2>/dev/null | grep -cE '^(crash|oom|timeout|leak)-' || true)
    if [ "${after:-0}" -gt "${before:-0}" ] || [ "$rc" != "0" ]; then
        note "fuzz finding (exit $rc):"
        ls -1t "$corpus" | grep -E '^(crash|oom|timeout|leak)-' | head -3 | sed 's/^/    /'
        grep -E "ERROR:|SUMMARY:|deadly signal" "$CI_LOG_DIR/fuzz.log" | head -6 | sed 's/^/  /'
        return 1
    fi
    note "${CI_FUZZ_SECONDS}s smoke clean — $(grep -oE 'Done [0-9]+ runs' "$CI_LOG_DIR/fuzz.log" | tail -1)"
}

run_stage "$@"
