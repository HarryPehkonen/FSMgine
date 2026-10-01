#!/usr/bin/env bash
#
# Local CI — every gate this repo has, in one script, with no service anywhere.
#
# Adapted from the AI-DEV-STARTER kit's templates/cpp/ci.sh (the same single-runner
# contract as JSOM, Computo and jsonTools). If you edit this file, say why in
# INCIDENTS.md.
#
# No GitHub, no network, no framework: this is what the git hooks in .githooks/ run,
# and you can run it by hand at any time. It is non-destructive — nothing is
# committed, staged, reverted or reformatted for you.
#
#   tools/ci.sh                       # all default stages, in order
#   tools/ci.sh build tests           # just these stages, in the order given
#   tools/ci.sh --list                # what the stages are
#   tools/ci.sh --changed format lint # scope format/lint to files changed vs HEAD
#   tools/ci.sh --require-clean       # also fail on uncommitted tracked changes
#   git config core.hooksPath .githooks   # one-time, per clone, enables the hooks
#
# Two tiers, because a C++ full run is minutes and a commit cannot afford minutes:
#
#   fast (pre-commit)  --changed format build lint tests
#   full (pre-push)    --require-clean tree format build lint tests asan fuzz pristine
#
# Stage ORDER is not arbitrary:
#   format before lint/test — it defines the text every later stage judges;
#   build before lint      — clang-tidy reads a compile database, which the
#                            configure step in `build` produces;
#   tests after build      — and after the cheap checks have already failed.
#
# Exit status: 0 only if every stage that ran passed.
#
# One deliberate reading of "report every failure": a failing stage prints
# EVERYTHING that failed inside it (every unformatted file, every failing test,
# every finding) and then STOPS the run, because these stages are a chain.

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
#   tests/simple_test_runner.cpp  — dead code: nothing includes it and no CMakeLists
#                                   mentions it (flagged 2026-09-30, your call)
#   benchmarks/bench_FSM.cpp,
#   benchmarks/bench_StringInterner.cpp
#                                 — gated on Google Benchmark, which is NOT installed.
#                                   FSMgine's own CMake prints the remedy and falls back
#                                   to simple_timer_benchmark.cpp. NOT installed on
#                                   purpose: the gate would then depend on an optional
#                                   dev package, which is a bad deal for a public repo.
CI_UNBUILT_OK=${CI_UNBUILT_OK:-"tests/simple_test_runner.cpp benchmarks/bench_FSM.cpp benchmarks/bench_StringInterner.cpp"}
# ONE definition of each configuration's configure flags, used by BOTH the `dbs` stage
# (which only configures, to make every compile database exist) and the stage that
# builds it. Two spellings of the same flags is exactly the drift the gate exists to stop.
CI_CFG_COMMON=${CI_CFG_COMMON:-"-DCMAKE_EXPORT_COMPILE_COMMANDS=ON $CI_CMAKE_EXTRA_FLAGS"}
CI_CFG_BUILD=${CI_CFG_BUILD:-"-DCMAKE_BUILD_TYPE=${CI_BUILD_TYPE:-Release}"}
CI_CFG_RELEASE=${CI_CFG_RELEASE:-"-DCMAKE_BUILD_TYPE=$CI_RELEASE_BUILD_TYPE -DCMAKE_CXX_COMPILER=$CI_RELEASE_BUILD_CXX"}
CI_CFG_FUZZ=${CI_CFG_FUZZ:-"-DFSMGINE_BUILD_FUZZING=ON -DFSMGINE_BUILD_MULTITHREADED=OFF -DBUILD_TESTING=OFF -DCMAKE_CXX_COMPILER=clang++"}
# Paths that are never our source, even when git tracks them: build-asan/ holds 61
# TRACKED files (committed by accident), including CMake's generated
# CompilerIdCXX/CMakeCXXCompilerId.cpp — which no formatter can ever satisfy, because
# CMake rewrites it. Without this the format stage is permanently red.
CI_SOURCE_EXCLUDE=${CI_SOURCE_EXCLUDE:-'^(build|build-[^/]*|\.ci|\.cache|cmake-build-[^/]*)/'}
# The gate's own footprint must never be a dirty-tree finding.
CI_IGNORED_PATHS="build build-asan build-fuzz build-tsan .ci-logs .ci .ci.env fuzz/corpus"
SCOPE_ALL=1                 # 0 = --changed: format/lint only see files changed vs HEAD
EXTRA_CHECKS=""             # --extra-checks: appended to the clang-tidy check list

if [ -f .ci.env ]; then
    # shellcheck disable=SC1091
    . ./.ci.env
fi

REQUIRE_CLEAN=0
WRITE_BASELINE=0
STAGES_REQUESTED=()

# ---------------------------------------------------------------- plumbing
RESULT_LINES=()
FAILED_STAGE=""
RAN_STAGES=()

usage() {
    sed -n '3,/^$/p' "$0" | sed 's/^# \{0,1\}//'
    cat <<'EOF'
Stages:
  tree        the gate's own footprint is ignored; --require-clean also fails on
              uncommitted changes to tracked files
  format      clang-format drift — dry run against the repo .clang-format
  build       cmake configure (with a compile database) + build; counts warnings
  lint        clang-tidy across the database's translation units
  dbs         configure-only: makes every compile database exist so that `lint` can
              union them AND still name any tracked source no database covers
  tests       the test suite (ctest), every failure reported
  release     the SAME code in a SECOND configuration: clang++ with -O2, warning-free.
              A different COMPILER on purpose — the default build is gcc, and a
              clang-only -Werror diagnostic was the first real bug this gate found.
  asan        the SAME tests under AddressSanitizer + UndefinedBehaviorSanitizer
  fuzz        a 60 s fuzz smoke; fails on a crash artifact or a timeout
  pristine    build + test a clean checkout of HEAD (proves the COMMITTED tree)

Options:
  --changed              scope format/lint to files changed vs HEAD (the commit tier)
  --require-clean        also fail on uncommitted changes to tracked files
  --extra-checks LIST    append clang-tidy checks (e.g. re-enable an excluded family)
  --write-tidy-baseline  accept the tidy findings you inherited (writes the baseline)
EOF
}

note()  { printf '  %s\n' "$*"; }
stage_banner() { printf '\n=== %s ===\n' "$1"; }
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
        raw=$({ git diff --name-only HEAD -- 2>/dev/null; git ls-files --others --exclude-standard 2>/dev/null; } | sort -u)
    fi
    printf '%s\n' "$raw" \
        | grep -E '\.(cpp|cc|cxx|hpp|hh|h)$' \
        | grep -vE "$CI_SOURCE_EXCLUDE" || true
}

# ---------------------------------------------------------------- stages
stage_tree() {
    stage_banner tree
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

stage_format() {
    stage_banner format
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
    if [ "$bad" != "0" ]; then
        note "$bad file(s) would be reformatted. Fix with: clang-format -i <file>..."
        return 1
    fi
    note "no drift across $(printf '%s\n' $files | wc -l) file(s)"
}

stage_dbs() {
    stage_banner dbs
    have cmake || block "cmake not installed"
    # Configure-only, purely so every compile database EXISTS. The lint stage unions them
    # and fails on a tracked source that no database covers — but a database written by a
    # LATER stage would make that check report files it simply had not seen yet. Ordering
    # the gate around a check is the wrong fix; making the check's inputs exist is the
    # right one. Each configure is ~0.05 s.
    local d ok=1
    for d in "$CI_BUILD_DIR:$CI_CFG_BUILD" "$CI_RELEASE_BUILD_DIR:$CI_CFG_RELEASE" \
             "$CI_FUZZ_BUILD_DIR:$CI_CFG_FUZZ"; do
        # shellcheck disable=SC2086
        cmake -B "${d%%:*}" ${d##*:} $CI_CFG_COMMON > "$CI_LOG_DIR/dbs-$(basename "${d%%:*}").log" 2>&1 || {
            note "configure failed for ${d%%:*} (log: $CI_LOG_DIR/dbs-$(basename "${d%%:*}").log)"
            ok=0
        }
    done
    [ "$ok" = "1" ] || return 1
    note "compile databases ready for the lint stage"
    return 0
}

stage_build() {
    stage_banner build
    have cmake || block "cmake not installed"
    if ! cmake -B "$CI_BUILD_DIR" $CI_CFG_BUILD $CI_CFG_COMMON > "$CI_LOG_DIR/configure.log" 2>&1; then
        tail -25 "$CI_LOG_DIR/configure.log" | sed 's/^/  /'
        note "configure failed (log: $CI_LOG_DIR/configure.log)"
        return 1
    fi
    cmake --build "$CI_BUILD_DIR" -j"$CI_JOBS" > "$CI_LOG_DIR/build.log" 2>&1
    local rc=$? warns
    warns=$(grep -c "warning:" "$CI_LOG_DIR/build.log" 2>/dev/null || true)
    if [ "$rc" != "0" ]; then
        grep -E "error:|warning:" "$CI_LOG_DIR/build.log" | head -25 | sed 's/^/  /'
        note "build failed (log: $CI_LOG_DIR/build.log)"
        return 1
    fi
    if [ "${warns:-0}" != "0" ]; then
        grep "warning:" "$CI_LOG_DIR/build.log" | head -20 | sed 's/^/  /'
        note "$warns warning(s) — counted even where -Werror is not wired onto a target"
        return 1
    fi
    note "configured with a compile database, built clean (-j$CI_JOBS)"
}

stage_lint() {
    stage_banner lint
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
            printf '# Key = path:check. Line numbers are excluded on purpose: they move on every\n'
            printf '# reformat, and a line-keyed baseline stops matching without saying so.\n'
            printf '# Fix a finding, then delete its line here. Any key NOT in this file FAILS the\n'
            printf '# gate, so this list is a debt to pay down, not a permission slip.\n'
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
        note "full log: $CI_LOG_DIR/tidy.log — accept inherited ones with: tools/ci.sh --write-tidy-baseline lint"
        return 1
    fi
    note "0 new findings across $(printf '%s\n' $tus | wc -l) translation unit(s) \
($(grep -vc '^#' "$CI_TIDY_BASELINE" 2>/dev/null || echo 0) inherited key(s) in $CI_TIDY_BASELINE)"
}

stage_tests() {
    stage_banner tests
    have ctest || block "ctest not installed"
    if ! $CI_TEST_CMD; then
        note "tests failed (above)"
        return 1
    fi
    note "all tests passed"
}

stage_asan() {
    stage_banner asan
    have clang++ || block "clang++ not installed (sanitizers need it here)"
    if ! cmake -B "$CI_ASAN_BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug \
               -DCMAKE_CXX_COMPILER=clang++ \
               -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
               -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" \
               > "$CI_LOG_DIR/asan-configure.log" 2>&1; then
        tail -20 "$CI_LOG_DIR/asan-configure.log" | sed 's/^/  /'; return 1
    fi
    if ! cmake --build "$CI_ASAN_BUILD_DIR" -j"$CI_JOBS" > "$CI_LOG_DIR/asan-build.log" 2>&1; then
        grep -E "error:" "$CI_LOG_DIR/asan-build.log" | head -20 | sed 's/^/  /'; return 1
    fi
    if ! ctest --test-dir "$CI_ASAN_BUILD_DIR" --output-on-failure -j"$CI_JOBS" > "$CI_LOG_DIR/asan-test.log" 2>&1; then
        tail -30 "$CI_LOG_DIR/asan-test.log" | sed 's/^/  /'
        note "sanitizer run failed (log: $CI_LOG_DIR/asan-test.log)"
        return 1
    fi
    note "clean under ASan + UBSan"
}

stage_fuzz() {
    stage_banner fuzz
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

stage_pristine() {
    stage_banner pristine
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

stage_release() {
    stage_banner release
    have cmake || block "cmake not installed"
    have "$CI_RELEASE_BUILD_CXX" || block "$CI_RELEASE_BUILD_CXX not installed"
    if ! cmake -B "$CI_RELEASE_BUILD_DIR" $CI_CFG_RELEASE $CI_CFG_COMMON \
               > "$CI_LOG_DIR/release-configure.log" 2>&1; then
        tail -20 "$CI_LOG_DIR/release-configure.log" | sed 's/^/  /'
        note "release configure failed"
        return 1
    fi
    cmake --build "$CI_RELEASE_BUILD_DIR" -j"$CI_JOBS" > "$CI_LOG_DIR/release-build.log" 2>&1
    local rc=$? warns
    warns=$(grep -c "warning:" "$CI_LOG_DIR/release-build.log" 2>/dev/null || true)
    if [ "$rc" != "0" ] || [ "${warns:-0}" != "0" ]; then
        grep -E "error:|warning:" "$CI_LOG_DIR/release-build.log" | head -25 | sed 's/^/  /'
        note "$CI_RELEASE_BUILD_CXX -O2 is not clean (log: $CI_LOG_DIR/release-build.log)"
        return 1
    fi
    if ! ctest --test-dir "$CI_RELEASE_BUILD_DIR" --output-on-failure -j"$CI_JOBS" \
               > "$CI_LOG_DIR/release-test.log" 2>&1; then
        tail -25 "$CI_LOG_DIR/release-test.log" | sed 's/^/  /'
        note "release tests failed (log: $CI_LOG_DIR/release-test.log)"
        return 1
    fi
    note "$CI_RELEASE_BUILD_CXX -O2: warning-free, tests pass"
}

run_stage() {
    CURRENT_STAGE="$1"
    case "$1" in
        tree) stage_tree ;;
        format) stage_format ;;
        build) stage_build ;;
        lint) stage_lint ;;
        tests) stage_tests ;;
        dbs)        stage_dbs ;;
        release) stage_release ;;
        asan) stage_asan ;;
        fuzz) stage_fuzz ;;
        pristine) stage_pristine ;;
        *) printf 'unknown stage: %s\n' "$1" >&2
           block "unknown stage '$1' — refusing to report a pass for a stage that does not exist" ;;
    esac
}

# ---------------------------------------------------------------- main
while [ $# -gt 0 ]; do
    case "$1" in
        --list | --help | -h) usage; exit 0 ;;
        --require-clean) REQUIRE_CLEAN=1 ;;
        --changed) SCOPE_ALL=0 ;;
        --write-tidy-baseline) WRITE_BASELINE=1 ;;
        --extra-checks) EXTRA_CHECKS=",${2:-}"; shift ;;
        -*) printf 'unknown option: %s\n\n' "$1" >&2; usage >&2; exit 2 ;;
        *) STAGES_REQUESTED+=("$1") ;;
    esac
    shift
done

if [ ${#STAGES_REQUESTED[@]} -eq 0 ]; then
    # shellcheck disable=SC2206
    STAGES_REQUESTED=($CI_DEFAULT_STAGES)
fi

mkdir -p "$CI_LOG_DIR"
printf 'FSMgine local CI — %s (scope: %s)\n' \
    "$(date '+%Y-%m-%d %H:%M:%S')" "$([ "$SCOPE_ALL" = "1" ] && echo all-sources || echo changed-only)"

for s in "${STAGES_REQUESTED[@]}"; do
    run_stage "$s"
    rc=$?
    RAN_STAGES+=("$s")
    if [ "$rc" = "0" ]; then
        record "$s" "ok"
    else
        record "$s" "FAILED"
        FAILED_STAGE="$s"
        break
    fi
done

finish
