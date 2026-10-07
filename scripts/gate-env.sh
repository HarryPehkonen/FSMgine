#!/usr/bin/env bash
#
# FSMgine's gate: the configuration every stage reads, and the helpers they share.
#
# The POLICY is gate.toml (which stages exist, which tier runs which of them, how a failure
# is recognised). The ENGINE is kit-ci, one binary installed once per machine
# (cmake --install build --prefix ~/.local). Everything a stage needs BEYOND that policy -
# a build dir, a job count, a fuzz budget - lives HERE, so the policy file stays a list of
# stages.
#
# SOURCED, never executed: scripts/gate.sh does not need it; every stage script does
# (`. "$(dirname "$0")/gate-env.sh"`). The knobs are the .ci.env knobs the old tools/ci.sh
# carried, each with the default it had there, and .ci.env is still sourced last, so a
# machine with no .ci.env behaves exactly as it did before the conversion
# (2026-10-06, card t_075c0a6f).

set -uo pipefail

REPO_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$REPO_ROOT" || exit 1

# No colour from the tools: this script greps their output (warning:, error:) and
# ANSI escapes defeat the greps.
export NO_COLOR=1

# git exports GIT_INDEX_FILE to a hook when the commit is made with a PATHSPEC
# (`git commit -- <path>`): it names git's TEMPORARY index, not this repository's.
# Every process a hook starts inherits it, and any `git` command the gate runs
# inside ANOTHER repository (a FetchContent update step, a pristine clone) then
# reads this repo's index against that repo's object store and dies on the first
# blob it does not have. Unset it once, here, rather than per-invocation.
unset GIT_INDEX_FILE

# ---------------------------------------------------------------- defaults + config
CI_JOBS=${CI_JOBS:-$(nproc 2>/dev/null || echo 4)}
CI_BUILD_DIR=${CI_BUILD_DIR:-build}
CI_ASAN_BUILD_DIR=${CI_ASAN_BUILD_DIR:-.ci/build-asan}
# NOTE (2026-09-30): the sanitizer build dir lives under .ci/ rather than ./build-asan
# because 61 files of build-asan/ are TRACKED in git (committed by accident in 51bdd9c).
# Building there modifies tracked files, which makes --require-clean unsatisfiable on
# every push. Untrack it with `git rm -r --cached build-asan` and this can move back.
CI_FUZZ_BUILD_DIR=${CI_FUZZ_BUILD_DIR:-build-fuzz}
CI_RELEASE_BUILD_DIR=${CI_RELEASE_BUILD_DIR:-.ci/build-release}
# The SECOND configuration, and deliberately a different COMPILER: the default build
# is gcc, this one is clang++ -O2. Two classes it can see that the default cannot:
# warnings that only exist at -O2, and anything gcc accepts that clang rejects (a
# clang-only -Werror diagnostic was the first real bug this gate found in FSMgine).
CI_RELEASE_BUILD_TYPE=${CI_RELEASE_BUILD_TYPE:-Release}
CI_RELEASE_BUILD_CXX=${CI_RELEASE_BUILD_CXX:-clang++}
CI_LOG_DIR=${CI_LOG_DIR:-.ci-logs}
CI_STRICT_TOOLS=${CI_STRICT_TOOLS:-0}          # 1 = a missing tool fails instead of SKIPping
CI_FAST_STAGES=${CI_FAST_STAGES:-"dbs format build lint tests"}
CI_FULL_STAGES=${CI_FULL_STAGES:-"tree selftest format version docexamples kitprobes dbs build lint tests coverage release asan fuzz pristine"}
CI_DEFAULT_STAGES=${CI_DEFAULT_STAGES:-"tree format build lint tests"}
CI_SOURCE_GLOBS=${CI_SOURCE_GLOBS:-"'*.cpp' '*.cc' '*.cxx' '*.hpp' '*.hh' '*.h'"}
CI_FUZZ_SECONDS=${CI_FUZZ_SECONDS:-60}         # "quick fuzzing on push" = 60 s
CI_TEST_CMD=${CI_TEST_CMD:-"ctest --test-dir $CI_BUILD_DIR --output-on-failure -j $CI_JOBS"}
CI_TIDY_BASELINE=${CI_TIDY_BASELINE:-.ci/tidy-baseline.txt}
# Compile EVERY tracked source. examples/ and benchmarks/ are OFF in the project's own
# defaults, so without this the gate never compiles 10 of the 24 tracked sources and
# reports a green over a subject it never looked at. Measured 2026-09-30: turning them on
# found 21 unused-parameter errors (gcc and clang alike) plus 2 dead `this` captures
# (clang-only) in code that had never been compiled.
CI_CMAKE_EXTRA_FLAGS=${CI_CMAKE_EXTRA_FLAGS:-"-DBUILD_EXAMPLES=ON -DBUILD_BENCHMARKS=ON"}
# Every database the lint stage unions, so a TU cannot hide in a build directory the gate
# forgot about (the fuzz targets have their own).
CI_LINT_DBS=${CI_LINT_DBS:-"$CI_BUILD_DIR/compile_commands.json $CI_RELEASE_BUILD_DIR/compile_commands.json $CI_FUZZ_BUILD_DIR/compile_commands.json"}
# Tracked sources that NO target compiles, each with the honest reason — named here
# rather than left silent, and reprinted on every green lint so they can never become
# invisible:
#   benchmarks/bench_FSM.cpp,
#   benchmarks/bench_StringInterner.cpp
#                                 — gated on Google Benchmark, which is NOT installed.
#                                   FSMgine's own CMake prints the remedy and falls back
#                                   to simple_timer_benchmark.cpp. NOT installed on
#                                   purpose: the gate would then depend on an optional
#                                   dev package, which is a bad deal for a public repo.
CI_UNBUILT_OK=${CI_UNBUILT_OK:-"benchmarks/bench_FSM.cpp benchmarks/bench_StringInterner.cpp"}
# ONE definition of each configuration's configure flags, used by BOTH the `dbs` stage
# (which only configures, to make every compile database exist) and the stage that
# builds it. Two spellings of the same flags is exactly the drift the gate exists to stop.
CI_CFG_COMMON=${CI_CFG_COMMON:-"-DCMAKE_EXPORT_COMPILE_COMMANDS=ON $CI_CMAKE_EXTRA_FLAGS"}
CI_CFG_BUILD=${CI_CFG_BUILD:-"-DCMAKE_BUILD_TYPE=${CI_BUILD_TYPE:-Release}"}
CI_CFG_RELEASE=${CI_CFG_RELEASE:-"-DCMAKE_BUILD_TYPE=$CI_RELEASE_BUILD_TYPE -DCMAKE_CXX_COMPILER=$CI_RELEASE_BUILD_CXX"}
CI_CFG_FUZZ=${CI_CFG_FUZZ:-"-DFSMGINE_BUILD_FUZZING=ON -DFSMGINE_BUILD_MULTITHREADED=OFF -DBUILD_TESTING=OFF -DCMAKE_CXX_COMPILER=clang++"}
# Coverage: source-based (clang), gated on MISSED LINES PER FILE over the LIBRARY — the
# files the suite is supposed to exercise. examples/ and benchmarks/ are compiled but
# never executed by the tests, so gating them would fail on code that has no tests *by
# design* — they are in the report, just not in the subject.
# llvm-cov/llvm-profdata ship with clang on Debian but off PATH; the stage globs for them.
CI_COV_BUILD_DIR=${CI_COV_BUILD_DIR:-.ci/build-coverage}
CI_COV_BASELINE=${CI_COV_BASELINE:-.ci/coverage-baseline.txt}
CI_COV_SUBJECT=${CI_COV_SUBJECT:-"--subject include/FSMgine --subject src"}
CI_COV_TEST_BIN=${CI_COV_TEST_BIN:-*test*}
# Paths that are never our source, even when git tracks them: build-asan/ holds 61
# TRACKED files (committed by accident), including CMake's generated
# CompilerIdCXX/CMakeCXXCompilerId.cpp — which no formatter can ever satisfy, because
# CMake rewrites it. Without this the format stage is permanently red.
CI_SOURCE_EXCLUDE=${CI_SOURCE_EXCLUDE:-'^(build|build-[^/]*|\.ci|\.cache|cmake-build-[^/]*)/'}
# The gate's own footprint must never be a dirty-tree finding.
CI_IGNORED_PATHS="build build-asan build-fuzz build-tsan .ci-logs .ci .ci.env fuzz/corpus"

# The old tools/ci.sh took these as FLAGS (--require-clean, --changed, --extra-checks,
# --write-tidy-baseline, --write-coverage-baseline). kit-ci's vocabulary has no per-run flags
# for a stage: a caller sets the knob in the ENVIRONMENT it launches the gate with, and the
# default below is what the script had. .githooks/pre-push does exactly that with
# CI_REQUIRE_CLEAN=1. (Until this line the assignment overwrote whatever the caller exported,
# so the push hook's flag-as-env-var had no effect at all - measured 2026-10-06.)

SCOPE_ALL=${CI_SCOPE_ALL:-1}                 # 0 = --changed: format/lint only see files changed vs HEAD
EXTRA_CHECKS=${CI_EXTRA_CHECKS:-}             # --extra-checks: appended to the clang-tidy check list

if [ -f .ci.env ]; then
    # shellcheck disable=SC1091
    . ./.ci.env
fi

REQUIRE_CLEAN=${CI_REQUIRE_CLEAN:-0}
WRITE_BASELINE=${CI_WRITE_BASELINE:-0}
WRITE_COV_BASELINE=${CI_WRITE_COV_BASELINE:-0}
STAGES_REQUESTED=()

# ---------------------------------------------------------------- plumbing
RESULT_LINES=()
FAILED_STAGE=""
RAN_STAGES=()


note()  { printf '  %s\n' "$*"; }
ci_banner() { printf '\n=== %s ===\n' "$1"; }
record() { RESULT_LINES+=("$(printf '%-9s %s' "$1" "$2")"); }

block() {   # a requested stage that could not run is a failure, not a skip
    printf 'BLOCK: %s\n' "$1" >&2
    record "${CURRENT_STAGE:-gate}" "BLOCKED — $1"
    FAILED_STAGE="${CURRENT_STAGE:-gate}"
    finish
}

finish() {
    printf '\n----------------------------------------\n'
    for line in "${RESULT_LINES[@]}"; do printf '%s\n' "$line"; done
    printf 'stages run: %s\n' "${RAN_STAGES[*]:-none}"
    if [ -n "$FAILED_STAGE" ]; then
        printf 'GATE FAILED at: %s\n' "$FAILED_STAGE"
        exit 1
    fi
    printf 'GATE PASSED\n'
    exit 0
}

have() { command -v "$1" >/dev/null 2>&1; }

# The files this run's format/lint stages own: everything, or just what changed.
scoped_sources() {
    local raw
    if [ "$SCOPE_ALL" = "1" ]; then
        raw=$(eval "git ls-files $CI_SOURCE_GLOBS")
    else
        # --diff-filter=ACMR keeps Added/Copied/Modified/Renamed and drops Deleted. A path this
        # run deleted is not a file to format or lint, and handing a missing path to
        # clang-format fails the stage with "No such file or directory" — hit on the v2.0.0
        # release commit, which deletes a test file. The existence filter below is the belt to
        # that braces: it also covers a file deleted in the working tree but not yet staged.
        # `git diff HEAD` compares the WORKING TREE and skips the index, so a file whose staged copy
        # differs from its working-tree copy (staged, then formatted on disk) was invisible here.
        raw=$({ git diff --name-only --diff-filter=ACMR HEAD -- 2>/dev/null; git diff --cached --name-only --diff-filter=ACMR -- 2>/dev/null; git ls-files --others --exclude-standard 2>/dev/null; } | sort -u)
    fi
    printf '%s\n' "$raw" \
        | grep -E '\.(cpp|cc|cxx|hpp|hh|h)$' \
        | while IFS= read -r f; do [ -f "$f" ] && printf '%s\n' "$f"; done \
        | grep -vE "$CI_SOURCE_EXCLUDE" || true
}


# ---------------------------------------------------------------- stage helpers
run_stage() {
    CURRENT_STAGE="$1"
    case "$1" in
        tree) stage_tree ;;
        selftest) stage_selftest ;;
        format) stage_format ;;
        build) stage_build ;;
        lint) stage_lint ;;
        tests) stage_tests ;;
        dbs)        stage_dbs ;;
        version)    stage_version ;;
        docexamples) stage_docexamples ;;
        coverage)   stage_coverage ;;
        release) stage_release ;;
        asan) stage_asan ;;
        fuzz) stage_fuzz ;;
        pristine) stage_pristine ;;
        kitprobes) stage_kitprobes ;;
        *) printf 'unknown stage: %s\n' "$1" >&2
           block "unknown stage '$1' — refusing to report a pass for a stage that does not exist" ;;
    esac
}


# ---------------------------------------------------------------- verdict shims
# kit-ci calls a stage and reads its EXIT STATUS: 0 is a pass, non-zero is a failure, and
# the engine names the stage and prints the first lines of a failed stage's output itself
# (SPEC.md §4). The stage bodies in this directory were split verbatim out of the old
# tools/ci.sh and still speak that script's vocabulary, so it is defined here, once:
#
#   ci_pass <stage>                 END the stage, exit 0
#   ci_fail <stage> <reason> [log]  print why, show the log's tail, exit 1
#   ci_skip <stage> <reason>        say so; the caller then exits 0 (a SKIP is not a failure,
#                                   which is also what kit-ci's `when = "tool:<name>"` means)
#   ci_begin <title>                the old banner line
ci_begin() { printf '\n==> %s\n' "$1"; }
ci_pass() { exit 0; }
ci_skip() { printf '    SKIP: %s\n' "$2"; }
ci_fail() {
    printf 'FAILED: %s — %s\n' "$1" "$2" >&2
    [ -n "${3:-}" ] && show_log "$3"
    exit 1
}
show_log() {  # show_log <file> — the tail, for out-of-order output like a build log
    local file="${1:-}"
    if [ -n "$file" ] && [ -f "$file" ]; then
        printf -- '--- %s (last 25 lines) ---\n' "$file" >&2
        tail -n 25 "$file" | sed 's/^/      /' >&2
    fi
    return 0
}
