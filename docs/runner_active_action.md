# Active Actions — runner (from `runner_analysis.md`)

Action plan for the runner workstream. Specs and decisions per
[`runner_analysis.md`](runner_analysis.md); the UI side (UR #15) per
[`ui_review.md`](ui_review.md) / [`ui_active_action.md`](ui_active_action.md).

## Queued actions

### Low

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| R5 | **JSON validation** for `output_schema` and model `configuration` — scope settled in `runner_plan.md`: both must be JSON *objects*; `context.content` is excluded (may be plain text) | H2 in `ui_active_action.md` / UR #15 | `QJsonDocument::fromJson` with a clear "invalid JSON" message (line/column) in `ModelDialog`/`SkillDialog` (create + edit), blocking save on invalid JSON; empty fields stay allowed |
| R6 | UR #26 remaining: JSON highlighting / line numbers in the editor | `ui_review.md` Polish | independent polish |

(R7 — housekeeping — is closed: `docs/llamacpp_server_README.md` is
committed as the long-form source and build outputs are in `.gitignore`.)

## Summary

- **Now:** R5 (JSON validation; scope settled in `runner_plan.md`), then
  R6 (polish).
- After shipping each item: drop it from the open lists and the Summary
  (the commit is the record).
