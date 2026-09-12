# Active Actions — runner (from `runner_analysis.md`)

Action plan for the runner workstream. Specs and decisions per
[`runner_analysis.md`](runner_analysis.md); the UI side (UR #15) per
[`ui_review.md`](ui_review.md) / [`ui_active_action.md`](ui_active_action.md).

## Queued actions

### Low

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| R5 | **JSON validation** for `output_schema` — must be a JSON *object*; `model.configuration` excluded (tuning is handled in the llama.cpp router/server configuration for the time being, owner decision 2026-07-10) and `context.content` excluded (may be plain text) | H2 in `ui_active_action.md` / UR #15 | `QJsonDocument::fromJson` with a clear "invalid JSON" message (line/column) in `SkillDialog` (create + edit), blocking save on invalid JSON; empty field stays allowed |
| R6 | UR #26 remaining: JSON highlighting / line numbers in the editor | `ui_review.md` Polish | independent polish |

(R7 — housekeeping — is closed: the long-form llama.cpp server README is
not vendored here (see `llamacpp_server_contract.md`), and build outputs
are in `.gitignore`.)

## Test inventory

- `tests/stub_server.{h,c}` — in-process stub OpenAI server; `tests/run/test_run.c` — 10 scenarios, 58 checks, green under `make test`.
- `tests/argparse/test_argparse.c` — 47 pass-1/pass-2 parsing checks, green.
- `tests/llama_smoke.c` — manual smoke test against a LIVE OpenAI-compatible server (`make smoke`).
- `make -C acta_runner test-e2e` — dead-runner end-to-end suite: spawns real `acta_runner` processes (~10–15 s), separate from `make test` (see `building.md`).

## Summary

- **Now:** R5 (JSON validation; scope settled in `runner_plan.md`;
  R6 polish closed by owner decision, 2026-07-10).
- After shipping each item: drop it from the open lists and the Summary
  (the commit is the record).
