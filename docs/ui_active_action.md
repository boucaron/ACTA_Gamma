# Active Actions — from `ui_review.md`

Action plan derived from [`ui_review.md`](ui_review.md). Item numbers
(`UR #N`) reference that document, which now lists only the open items
(completed items were removed from it; item numbers keep the original
numbering).

## Queued (from `ui_review.md`)

### High

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| H2 | **JSON validation** for `output_schema` — must be a JSON *object*; model `configuration` **excluded** (tuning is handled in the llama.cpp router/server configuration for the time being — owner decision, 2026-07-10) and context `content` **excluded** (it may be plain text). Scope settled in `runner_plan.md` (R5). | UR #15 | `QJsonDocument::fromJson`, clear "invalid JSON" message (line/column), block save on invalid JSON, in `SkillDialog::ui->outputSchemaTextEdit` (create + edit); empty field stays allowed. Same work item as R5 in `runner_active_action.md` |

## Summary

- **Now:** H2 (JSON validation; scope settled in `runner_plan.md`).
- (C1 — cancel button — is shipped: the Run button toggles into Cancel,
  cooperative cancellation via the runner's cancel flag, row transitions
  to `cancelled`.)
- (L1 — tree context menu on empty area — is shipped: `FolderTreePanel` and
  `ExecutionPanel` both show the menu on empty-area right-clicks with the
  row-scoped actions disabled, mirroring `ContextPanel`.)
- After shipping each item: drop it from the open lists and the Summary
  (the commit is the record).
