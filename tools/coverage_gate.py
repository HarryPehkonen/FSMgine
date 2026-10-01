#!/usr/bin/env python3
"""coverage_gate — read an `llvm-cov report`, gate on PER-FILE MISSED LINES.

Why missed lines and not a percentage: a single global number can stay flat while the
subject shifts underneath it — delete an uncovered function and coverage "improves", add
a pile of untested code and a flat percentage hides it. Missed lines per file can only
improve by testing code or by deleting it, and both are visible in the diff.

Baseline file format (tracked; one line per file, `#` comments allowed):

    # path<TAB>missed_lines<TAB>total_lines<TAB>line_cover%
    include/FSMgine/FSM.hpp	7	177	96.05

A file absent from the baseline is treated as "0 missed allowed": NEW code must arrive
with tests, or be recorded deliberately with --write.

Usage:
  coverage_gate.py --report FILE --subject include/FSMgine --subject src
  coverage_gate.py --report FILE --subject include/FSMgine --baseline .ci/coverage-baseline.txt
  coverage_gate.py --report FILE --subject include/FSMgine --write .ci/coverage-baseline.txt
"""
import argparse
import re
import sys

NUM = re.compile(r"^-?[0-9]+(\.[0-9]+)?%?$")


def is_num(tok):
    """llvm-cov prints percentages with a '%' suffix and '-' for 'no branches'."""
    return tok == "-" or bool(NUM.match(tok))


def num(tok, default=0.0):
    if tok == "-":
        return default
    return float(tok.rstrip("%"))


def parse_report(text):
    """Yield (path, lines, missed_lines, line_pct, branch_pct) from llvm-cov report."""
    rows = []
    for raw in text.splitlines():
        line = raw.rstrip()
        if not line.strip() or line.startswith("-") or line.startswith("Filename"):
            continue
        parts = line.split()
        # the numeric tail is 12 columns: regions(3) functions(3) lines(3) branches(3)
        if len(parts) < 13:
            continue
        tail = parts[-12:]
        if not all(is_num(p) for p in tail):
            continue
        path = " ".join(parts[:-12])
        lines = int(num(tail[6]))
        missed = int(num(tail[7]))
        line_pct = num(tail[8])
        branch_pct = num(tail[11], default=float("nan"))
        rows.append((path, lines, missed, line_pct, branch_pct))
    return rows


def load_baseline(path):
    base = {}
    if not path:
        return base
    try:
        with open(path, encoding="utf-8") as fh:
            for ln in fh:
                if ln.startswith("#") or not ln.strip():
                    continue
                parts = ln.rstrip("\n").split("\t")
                if len(parts) >= 2 and parts[1].strip().isdigit():
                    base[parts[0]] = int(parts[1])
    except FileNotFoundError:
        pass
    return base


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--report", required=True)
    ap.add_argument("--subject", action="append", default=[],
                    help="only files whose path contains this substring are gated")
    ap.add_argument("--baseline")
    ap.add_argument("--write")
    args = ap.parse_args()

    rows = parse_report(open(args.report, encoding="utf-8", errors="replace").read())
    subject = [r for r in rows if any(s in r[0] for s in args.subject)] if args.subject else rows
    subject.sort(key=lambda r: r[0])
    if not subject:
        print("  no files matched the coverage subject — a report with no subject is not a pass")
        return 1

    tot_lines = sum(r[1] for r in subject)
    tot_missed = sum(r[2] for r in subject)
    tot_pct = 100.0 * (tot_lines - tot_missed) / tot_lines if tot_lines else 0.0

    print(f"  subject: {len(subject)} file(s), {tot_lines} instrumented lines, "
          f"{tot_missed} never run → {tot_pct:.2f}% line coverage")
    print(f"  worst files:")
    for path, lines, missed, pct, bpct in sorted(subject, key=lambda r: -r[2])[:5]:
        print(f"    {missed:>4} missed of {lines:>4} ({pct:6.2f}%)  {path}")

    if args.write:
        with open(args.write, "w", encoding="utf-8") as fh:
            fh.write("# Coverage baseline — gated on MISSED LINES per file (2026-09-30).\n")
            fh.write("# A file's missed count may go DOWN freely; it may not grow, and a file\n")
            fh.write("# absent here must arrive fully covered. Re-record with:\n")
            fh.write("#   tools/ci.sh --write-coverage-baseline coverage\n")
            fh.write("# path\tmissed_lines\ttotal_lines\tline_cover%\n")
            for path, lines, missed, pct, _ in subject:
                fh.write(f"{path}\t{missed}\t{lines}\t{pct:.2f}\n")
        print(f"  wrote {args.write} ({len(subject)} file(s))")
        return 0

    base = load_baseline(args.baseline)
    regressions, improvements = [], []
    for path, lines, missed, pct, _ in subject:
        allowed = base.get(path, 0)
        if missed > allowed:
            regressions.append((path, allowed, missed))
        elif missed < allowed:
            improvements.append((path, allowed, missed))

    if improvements:
        print("  coverage improved (re-record with --write-coverage-baseline):")
        for path, was, now in improvements:
            print(f"    {path}: {was} → {now} missed")
    if regressions:
        print("  COVERAGE REGRESSION — more uncovered lines than the baseline allows:")
        for path, allowed, now in regressions:
            print(f"    {path}: {allowed} allowed, {now} found  (+{now - allowed})")
        print("  new code must arrive with tests, or be recorded deliberately:")
        print("    tools/ci.sh --write-coverage-baseline coverage   (and say why in the commit)")
        return 1
    if not args.baseline:
        print("  no baseline file: every file was checked against 0 missed lines")
    print("  no file lost coverage")
    return 0


if __name__ == "__main__":
    sys.exit(main())
