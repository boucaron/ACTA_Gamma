# Active Actions — runner (from `runner_analysis.md`)

Action plan for the runner workstream. Specs and decisions per
[`runner_analysis.md`](runner_analysis.md); the UI side (UR #15) per
[`ui_review.md`](ui_review.md) / [`ui_active_action.md`](ui_active_action.md).

## Queued actions

### Low

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| R5 | **JSON validation (to analyze)** for `output_schema` and model `configuration` (context `content` likely excluded — it may be plain text) | H2 in `ui_active_action.md` / UR #15 | analyze scope first; then `QJsonDocument::fromJson` with a clear "invalid JSON" message in the dialogs/panel editor, at minimum a "Validate JSON" button |
| R6 | UR #26 remaining: JSON highlighting / line numbers in the editor | `ui_review.md` Polish | independent polish |
| R7 | Housekeeping: decide fate of untracked `docs/llamacpp_server_README.md` (referenced by `runner_analysis.md` — commit or delete); consider `.gitignore` for build outputs | working tree | — |

## Summary

- **Now:** R5 (JSON validation, analyze first), then R6–R7
  (polish / housekeeping).
- After shipping each item: drop it from the open lists and the Summary
  (the commit is the record).
