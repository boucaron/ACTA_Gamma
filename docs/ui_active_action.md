# Active Actions — from `ui_review.md`

Action plan derived from [`ui_review.md`](ui_review.md). Item numbers
(`UR #N`) reference that document, which now lists only the open items
(completed items were removed from it; item numbers keep the original
numbering).

## In progress

| # | Action | Source | Status / remaining |
|---|--------|--------|--------------------|
| C1 | **Cancel button**: the Run button toggles into Cancel while an execution is in flight (cooperative cancellation; the row transitions pending\|running → `cancelled`, not `failed`) | New (2026-07-11) | **Code written, not committed (build/test pending).** Runner: `backend.h`/`backend.c` — process-global `backend_cancel_request/reset/requested` flag + curl `XFERINFOFUNCTION` abort → new `BACKEND_ERR_CANCELED` (-4); `run.c` — resources hoisted + `CANCEL()` macro + `cancel_execution()` (logs `execution_cancelled`, `acta_db_execution_cancel()`) with checks before claim, after claim, after resolve/config/schema, after each HTTP call, before close; `runner.h` — `EXIT_CANCELED` (14); `main.c` help line ("UI cancel only"). CLI behavior unchanged (it never sets the flag). UI: `runnerWorker.{h,cpp}` — `requestCancel()`/`resetCancel()` (flag reset before/after each run); `executionPanel.{h,cpp}` — Run↔Cancel button + context-menu toggle, error dialog suppressed on `EXIT_CANCELED`. Remaining: build + smoke (run, cancel mid-run, cancelled row renders gray), commit, then a README/doc note. |

## Queued (from `ui_review.md`)

### High

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| H2 | **JSON validation** for `output_schema` and model `configuration` — scope settled in `runner_plan.md` (R5): both must be JSON *objects*; context `content` is **excluded** (it may be plain text) | UR #15 | `QJsonDocument::fromJson`, clear "invalid JSON" message (line/column), block save on invalid JSON, in `ModelDialog::ui->configurationTextEdit` and `SkillDialog::ui->outputSchemaTextEdit` (create + edit); empty fields stay allowed. Same work item as R5 in `runner_active_action.md` |

## Summary

- **Now:** H2 (JSON validation; scope settled in `runner_plan.md`).
- **C1 (cancel button)** is in progress: code written, awaiting build/test + commit.
- (L1 — tree context menu on empty area — is shipped: `FolderTreePanel` and
  `ExecutionPanel` both show the menu on empty-area right-clicks with the
  row-scoped actions disabled, mirroring `ContextPanel`.)
- After shipping each item: drop it from the open lists and the Summary
  (the commit is the record).
