#!/usr/bin/env bash
#
# update_bench_table.sh — regenerates the benchmark table in README.md from the
# measured output of benchmarks/bench_comparison.cpp.
#
# WHY: the README must answer "when should I NOT use FSMgine?" with measured numbers,
# not typed-in opinion. So the table is never hand-edited — this script is the only
# thing that writes the region between the markers, and it writes exactly what the
# benchmark just printed.
#
#   tools/update_bench_table.sh
#
# Rebuilds FSMgine_comparison (Release, single-threaded library), runs it with
# --markdown, and replaces ONLY the text between <!-- BENCH-TABLE:BEGIN --> and
# <!-- BENCH-TABLE:END --> in README.md. Fails loudly if either marker is missing,
# or if the benchmark's correctness check (run first, inside the binary) fails.
set -uo pipefail
REPO_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$REPO_ROOT" || exit 1

die() { printf 'update_bench_table: %s\n' "$*" >&2; exit 1; }

BEGIN_MARKER='<!-- BENCH-TABLE:BEGIN -->'
END_MARKER='<!-- BENCH-TABLE:END -->'
README="$REPO_ROOT/README.md"
BUILD_DIR="${CI_BUILD_DIR:-build}"

[ -f "$README" ] || die "README.md not found at $README"
grep -qF "$BEGIN_MARKER" "$README" || die "README.md has no $BEGIN_MARKER marker"
grep -qF "$END_MARKER" "$README" || die "README.md has no $END_MARKER marker"

have() { command -v "$1" >/dev/null 2>&1; }
have cmake || die "cmake not installed"

cmake -B "$BUILD_DIR" -DBUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release >/dev/null \
    || die "cmake configure failed"
cmake --build "$BUILD_DIR" --target FSMgine_comparison -j "$(nproc 2>/dev/null || echo 2)" >/dev/null \
    || die "build of FSMgine_comparison failed"

BIN="$BUILD_DIR/benchmarks/FSMgine_comparison"
[ -x "$BIN" ] || die "built binary not found at $BIN"

OUTPUT=$("$BIN" --markdown) || die "benchmark run failed (correctness check or crash — see output above)"
[ -n "$OUTPUT" ] || die "benchmark produced no output"

python3 - "$README" "$BEGIN_MARKER" "$END_MARKER" "$OUTPUT" <<'PYEOF' || die "failed to rewrite README.md"
import sys

readme_path, begin_marker, end_marker, output = sys.argv[1:5]
text = open(readme_path, encoding="utf-8").read()

begin_idx = text.find(begin_marker)
end_idx = text.find(end_marker)
if begin_idx == -1 or end_idx == -1 or end_idx < begin_idx:
    sys.exit(f"update_bench_table: markers not found in order in {readme_path}")

before = text[: begin_idx + len(begin_marker)]
after = text[end_idx:]
new_text = before + "\n\n" + output.strip() + "\n\n" + after

with open(readme_path, "w", encoding="utf-8") as f:
    f.write(new_text)
PYEOF

printf 'update_bench_table: README.md updated between the BENCH-TABLE markers\n'
