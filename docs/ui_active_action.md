# Active Actions — from `ui_review.md`

Action plan derived from [`ui_review.md`](ui_review.md). Item numbers
(`UR #N`) reference that document, which now lists only the open items
(completed items were removed from it; item numbers keep the original
numbering).

## Queued (from `ui_review.md`)

*(none — H2, JSON validation of `output_schema`, is shipped: `SkillDialog::validateFields` blocks save on invalid JSON or non-object JSON, with line/column feedback; model `configuration` and context `content` remain excluded per scope in `runner_plan.md` R5)*

## Summary

- **Now:** *(none — H2 shipped; scope settled in `runner_plan.md`).*
- (C1 — cancel button — is shipped: the Run button toggles into Cancel,
  cooperative cancellation via the runner's cancel flag, row transitions
  to `cancelled`.)
- (L1 — tree context menu on empty area — is shipped: `FolderTreePanel` and
  `ExecutionPanel` both show the menu on empty-area right-clicks with the
  row-scoped actions disabled, mirroring `ContextPanel`.)
- After shipping each item: drop it from the open lists and the Summary
  (the commit is the record).
