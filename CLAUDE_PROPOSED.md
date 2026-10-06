# CLAUDE.md — proposed changes (Hermes, 2026-10-06)

Claude Code owns `CLAUDE.md`, so nothing here is applied. Two sentences in it went stale today,
when the gate's hooks stopped spelling out their stages:

## 1. "The Local Gate" section — the stage list and the hook descriptions

Current text:

> Stages: `tree selftest format version docexamples dbs build lint tests coverage release
> asan fuzz pristine`.

> Two git hooks are checked in but not armed automatically — run `git config
> core.hooksPath .githooks` once per clone. **pre-commit** then runs `--changed dbs
> format build lint tests` (seconds, scoped to touched files); **pre-push** runs the
> full, now-fourteen-stage list plus `--require-clean` (~2-3 minutes).

Proposed:

> Stages: `tree selftest format version docexamples kitprobes dbs build lint tests
> coverage release asan fuzz pristine`. The two hook tiers are the gate's own
> `CI_FAST_STAGES` and `CI_FULL_STAGES`: **pre-commit** runs `--changed fast` (seconds,
> scoped to touched files), **pre-push** runs `--require-clean full` (the fifteen-stage
> list, ~2-3 minutes). A hook that spelled its stages out instead of naming the tier is
> what let the two drift apart; `tools/kit-probes/hook-tiers-agree.sh` now fails the
> gate if either does it again.

The `kitprobes` stage is new: it runs every script in `tools/kit-probes/`, each of which
re-derives one kit fix's guarantee on this copy.
