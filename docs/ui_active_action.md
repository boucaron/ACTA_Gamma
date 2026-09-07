# Active Actions — from `ui_review.md`

Action plan derived from [`ui_review.md`](ui_review.md). Item numbers
(`UR #N`) reference that document, which now lists only the open items
(completed items were removed from it; item numbers keep the original
numbering).

## Queued (from `ui_review.md`)

### Medium

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| M1 | **In-process runner: drop QProcess from the UI** — link the runner's pipeline into the app and call it directly | UR #45 | First analysis done (see below). Plan: (a) compile `acta_runner/src/run.c` + `backend.c` into `ACTA_Gamma` (`src.pro`: add to `SOURCES`, `LIBS += -lcurl -lcjson` plus platform libs, `extern "C"` prototype for `run_execution`); (b) run `run_execution()` on a worker `QThread` that opens its **own** `db_t` inside the thread (SQLite connections are not thread-shareable); the GUI thread keeps its `DbHandle` and the existing 1500 ms polling (WAL allows concurrent reader); (c) remove `QProcess`/`m_runner`, `findRunnerExe()`, `parseRunnerError()`, `onRunnerFinished`/`onRunnerError` from `ExecutionPanel`, replacing them with a worker-finished signal carrying the exit code + message (worker reads the structured result, no stderr parsing); (d) `setDb()` stops the worker instead of killing a process. The standalone `acta_runner` CLI stays as-is. Open design questions: keep or refactor the runner's stderr error-emission (`finish_db_error`/`emit_runner_error`) into a result out-parameter; api_key resolution (`$OPENAI_API_KEY` → `configuration.api_key`) reproduced in the UI wrapper; cancel-on-db-switch semantics. |

### High

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| H2 | **JSON validation** for `output_schema` and model `configuration` — scope settled in `runner_plan.md` (R5): both must be JSON *objects*; context `content` is **excluded** (it may be plain text) | UR #15 | `QJsonDocument::fromJson`, clear "invalid JSON" message (line/column), block save on invalid JSON, in `ModelDialog::ui->configurationTextEdit` and `SkillDialog::ui->outputSchemaTextEdit` (create + edit); empty fields stay allowed. Same work item as R5 in `runner_active_action.md` |

## Summary

- **Now:** H2 (JSON validation; scope settled in `runner_plan.md`), then M1 (in-process runner, no QProcess).
- (L1 — tree context menu on empty area — is shipped: `FolderTreePanel` and
  `ExecutionPanel` both show the menu on empty-area right-clicks with the
  row-scoped actions disabled, mirroring `ContextPanel`.)
- After shipping each item: drop it from the open lists and the Summary
  (the commit is the record).
