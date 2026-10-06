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
