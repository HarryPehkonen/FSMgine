#!/usr/bin/env bash
# ci_selftest.sh — tests the gate's own changed-file scope, on a throwaway clone.
#
# WHY: the gate's --changed scope once handed clang-format a path this run had DELETED, so any
# commit that removed a source file failed the format stage with "No such file or directory".
# Found on the v2.0.0 release commit (it deletes a test file). A gate that cannot survive a
# deletion is a gate that blocks the very commits that tidy a repo, and nothing tested it.
#
# This asserts BOTH directions, so the fix cannot rot:
#   1. with the fix, a commit that deletes a source file passes the format stage;
#   2. with the pre-fix line restored, the same situation FAILS — i.e. the test has teeth.
#
#   tools/ci_selftest.sh
set -uo pipefail

REPO_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
SCRATCH=$(mktemp -d)
trap 'rm -rf "$SCRATCH"' EXIT

die() { printf 'ci_selftest: %s\n' "$*" >&2; exit 1; }

git clone --quiet --local --no-hardlinks "$REPO_ROOT" "$SCRATCH/clone" || die "clone failed"
# The clone starts from HEAD; the file under test is the WORKING COPY (a fix being developed is
# not committed yet, and this test must be runnable before it is).
cp "$REPO_ROOT/tools/ci.sh" "$SCRATCH/clone/tools/ci.sh" || die "could not copy ci.sh"
cd "$SCRATCH/clone" || die "no clone"
git config user.email selftest@example.invalid; git config user.name selftest
git rm --quiet tests/test_Integration.cpp || die "could not stage a deletion"
echo "  staged a deletion of tests/test_Integration.cpp"

run_format_stage() { ./tools/ci.sh --changed format > "$SCRATCH/out.$1" 2>&1; echo $?; }

echo
echo "  1. the fixed gate (a deletion must not break the format stage)"
rc=$(run_format_stage fixed)
[ "$rc" = "0" ] || { tail -6 "$SCRATCH/out.fixed" | sed 's/^/    /'; die "format stage FAILED with the fix in place"; }
grep -q "No such file" "$SCRATCH/out.fixed" && die "the stage still names a missing file"
echo "     ok: format ok, exit 0"

echo
echo "  2. the pre-fix gate (the same situation must FAIL, or this test proves nothing)"
python3 - "$SCRATCH/clone/tools/ci.sh" <<'PY'
import pathlib, sys
p = pathlib.Path(sys.argv[1]); t = p.read_text()
fixed = "        raw=$({ git diff --name-only --diff-filter=ACMR HEAD -- 2>/dev/null;"
pre = "        raw=$({ git diff --name-only HEAD -- 2>/dev/null;"
assert t.count(fixed) == 1, "fixed line not found"
t = t.replace(fixed, pre)
# and drop the existence filter, which also hides the original failure
assert t.count('| while IFS= read -r f; do [ -f "$f" ] && printf ') == 1
t = t.replace('        | while IFS= read -r f; do [ -f "$f" ] && printf \'%s\\n\' "$f"; done \\\n', "")
p.write_text(t)
PY
rc=$(run_format_stage prefix)
[ "$rc" != "0" ] || die "the PREFIX gate passed: this test has no teeth"
grep -q "No such file or directory" "$SCRATCH/out.prefix" || die "pre-fix gate failed for another reason"
echo "     ok: the gate FAILS as it did in production (exit $rc)"

echo
echo "ci_selftest: PASSED — the changed scope survives deletions, and this test bites"
