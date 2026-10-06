# INCIDENTS.md — edits to files that came from the kit, and why

`tools/ci.sh` and the hooks in `.githooks/` carry a line asking that an edit here say why. This is
that record, newest first.

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
