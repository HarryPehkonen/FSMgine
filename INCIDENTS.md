# INCIDENTS.md — edits to files that came from the kit, and why

`tools/ci.sh` and the hooks in `.githooks/` carry a line asking that an edit here say why. This is
that record, newest first.

## 2026-10-06 — the gate's stage set was only knowable by reading the gate

What broke:        Nothing failed, which is the point. `--list` printed the usage text instead of
                   answering, so every tool that needed the stage set had to read the SOURCE. The
                   kit's tier probe did that — a dispatch-table regex plus a forwarding-alias
                   detector — and read six of this gate's real stages as aliases the first time it met
                   them. The stage-banner helper was named `stage_banner`, which put a non-stage into
                   any list derived from function names.
Check added:       `--list` prints `stages:` (derived from the `stage_*` functions the gate actually
                   defines), `default stages:`, `fast stages:` and `full stages:`; `--help` keeps the
                   usage text, because that is what a human wants. The helper is `ci_banner`. The kit
                   probe's `kitprobes` stage now reads the stage set from `--list`.
Why it must stay:  What this gate runs by hand, what it runs on a commit and what it runs on a push
                   were only knowable by reading 780 lines of shell — and the one tool that tried got
                   it wrong. A tool that reads source breaks when the source is reworded; a tool that
                   asks cannot. The four lines above are also the cheapest documentation this gate
                   has: the difference between a hand run (5 stages) and a push (15) is now printed
                   rather than implied.

---


## 2026-10-06 — the gate is no longer a 816-line bash script (wiring record, not a breakage)

What changed:      `tools/ci.sh` is deleted. The gate is now `gate.toml` — the policy: 14 stages
                   in two tiers — run by `kit-ci`, one binary installed once per machine
                   (`cmake --install build --prefix ~/.local`), plus `scripts/gate-env.sh` (the
                   .ci.env knobs and the helpers every stage reads), `scripts/gate.sh` (the one
                   file all three callers run) and one `scripts/<stage>.sh` per stage. The two
                   hooks are the kit's files with one line changed each: they NAME the tier
                   instead of repeating a stage list. This entry is a wiring record, not an
                   incident — it is here because the hooks and the retired gate both say "if you
                   edit this file, say why in INCIDENTS.md".
Check moved:       Every stage the old gate ran is still run, under the same name and in the
                   same order. full tier: tree selftest format version docexamples dbs build lint tests coverage release asan fuzz pristine. fast tier: dbs format build lint tests.
                   The teeth were re-measured on the converted gate rather than assumed:
                   an unformatted new source file dropped in the tree makes the fast tier
                   report GATE FAILED naming the `format` stage — captured in
                   `gate-evidence/t_075c0a6f/FSMgine-teeth.txt`.
Probes:            the `kitprobes` stage and `tools/kit-probes/` are DELETED. A probe checks a
                   FILE, and the file it checked (tools/ci.sh) is gone; a stage whose verdict is
                   an accident of how a probe searches is the failure mode the gate exists to
                   prevent. Measured 2026-10-06, each probe from HEAD run against the new entry
                   point `scripts/gate.sh`:
                   * format-checks-staged.sh      GREEN against scripts/gate.sh
                   * hook-tiers-agree.sh          RED against scripts/gate.sh
                   The guarantees themselves did not go:
                   * format-checks-staged      -> scripts/format.sh — the staged-copy check is the same code, in the stage that owns formatting
                   * hook-tiers-agree          -> gate.toml — the tier lists are [tier.fast] and [tier.full], the hooks NAME a tier, and `kit-ci --list` prints what each one holds
                   and gate-stage-guards is subsumed by the engine's runner (a stage whose
                   command does not exit 0 fails the run).

---

## 2026-10-06 — the hooks name a tier; both tier lists moved into the gate

The tier lists were written down twice each, and drifted. The comment at the top of `tools/ci.sh`
documented a pre-push tier of eight stages while `.githooks/pre-push` ran fourteen — so the file
that explains the gate described a gate that did not exist. Nothing checked either against the
other, and the fast tier carried its own copy of the list too.

Both hooks now pass `fast` or `full`; the lists live in `tools/ci.sh` as `CI_FAST_STAGES` and
`CI_FULL_STAGES` (the default list is left as it was, a five-stage quick set, so `./tools/ci.sh`
stays cheap by hand); `kitprobes` joins the full tier with the probe below.

`tools/kit-probes/hook-tiers-agree.sh` is new, together with the `kitprobes` stage that runs it. It
fails when a hook names a stage instead of a tier, when a stage the gate defines is in no tier, when
the fast tier is not a subset of the full one, or when the lists printed in the docs and the
variables stop agreeing — the four ways this drifted in the first place.
