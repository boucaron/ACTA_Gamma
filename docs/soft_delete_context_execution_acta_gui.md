# Soft delete for contexts and executions — `acta_gui` plan

Companion to
[`soft_delete_context_execution.md`](soft_delete_context_execution.md)
(design spec),
[`soft_delete_context_execution_acta_db.md`](soft_delete_context_execution_acta_db.md)
(DB layer, **done and tested**) and
[`soft_delete_context_execution_acta_cli_runner.md`](soft_delete_context_execution_acta_cli_runner.md)
(CLI + runner, **done and tested**). This is the implementation plan for the
last consumer: the GUI, per the spec's GUI section:

- Execution list: hide deleted rows by default; per-row restore action
  mirroring the model/skill rows; Retry button disabled for deleted rows.
- Context picker (ExecutionCreateDialog): list live contexts only.
- No delete button for a `running` execution (matches the C-layer rule).

**Status:** implemented (code in place, mirrors the skill/model panel
pattern; not yet compile-verified). `acta_gui` has no unit-test harness —
verification is a manual smoke run (`acta_gui/db/smoke_test.sh`) against a
migrated DB.

No `acta_db` or schema work is needed here: `acta_gui/db/schema.sql`
already carries `contexts.deleted_at` / `executions.deleted_at` and the
`contexts_soft_delete_only` trigger, and the DB-layer API
(`acta_db_execution_delete` / `restore`,
`acta_db_execution_query` with `execution_query_t.include_deleted`,
`acta_db_context_query` live-only default) is complete.

## 1. `src/widgets/executionPanel.h` / `executionPanel.cpp` — **done**

### List: hide deleted rows by default

- `reload()` builds `execution_query_t q = ACTA_EXEC_QUERY_ANY;
  q.include_deleted = <"Show trash" checked>` and calls
  `acta_db_execution_query(m_db, &q, …)`.
  - Unchecked (default): live-only — the DB-layer default, so deleted rows
    simply do not appear (no separate code path).
  - Checked: live + deleted rows; the panel distinguishes them via
    `execution_t.deleted_at`.
- New "Show trash" `QCheckBox` above the list (same label, tooltip and
  toggle-→`reload()` wiring as the skill/model `FolderTreePanel`);
  unchecked by default.

### Per-row marking and actions (mirrors `FolderTreePanel`)

- New row role `RoleExecutionDeleted` (`Qt::UserRole + 2`); deleted rows get
  the trash icon (`SP_TrashIcon`, cached in `m_deletedIcon`), a gray date
  column and a "Deleted <iso>" tooltip — exactly the skill/model row
  styling.
- Toolbar row: new icon-only buttons next to Run (accelerators
  Alt+D / Alt+T, letters unique in the panel):
  - **Delete** — `acta_db_execution_delete` after a confirmation prompt
    (same question style as `FolderTreePanel::onEntityDeleteClicked`).
    Error surfacing: `ACTA_DB_ERR_INVALID` → "Cannot delete a running
    execution."; `ACTA_DB_ERR_NOT_FOUND` → "already deleted?"; otherwise the
    raw `acta_db_strerror` message.
  - **Restore** — `acta_db_execution_restore`; status is untouched (a
    restored `failed` row stays `failed` and is retryable with Run), an
    "Execution N restored." info box, then `reload()`.
- Context menu: "Delete" / "Restore" entries between Run/Cancel and Show,
  enabled exactly like the toolbar buttons via `updateActionBtnStates()`.
- `updateActionBtnStates()` (called from the selection handler, `reload()`
  and `onWorkerFinished`):
  - Delete enabled for a **live, non-running** row;
  - Restore enabled only for a **deleted** row.
  "Running" is matched by prefix because the status cell shows
  "running…" (the progress indicator) while a run is in flight.
- `Delete` key accelerator while the list has focus (same gating as the
  buttons, mirroring the skill/model panel): soft-deletes a live
  non-running row, restores a deleted one.

### Retry (Run) button: deleted rows inert

- `updateRunBtnState()`: `canRun` now also requires
  `!cur->data(0, RoleExecutionDeleted).toBool()` — Run/Retry is disabled for
  deleted rows.
- `onRunBtnClicked()`: after `acta_db_execution_get`, a deleted row
  (`exec->deleted_at != nullptr`) returns before the failed→pending reset
  and before starting the worker. The button is the primary guard; this
  covers the context-menu / stale-selection paths. (Backstop:
  `acta_db_execution_reset` refuses deleted rows with `NOT_FOUND` anyway.)
- **No delete for `running` rows**: Delete is disabled while the row is
  running (the runner owns the row; deleting mid-run would race
  `start()/complete()/fail()` and `sweep`) — the same rule the DB layer
  enforces with `ACTA_DB_ERR_INVALID`.

### Unchanged on purpose

- The log list, filters, empty-state labels, keyboard Enter behavior, the
  in-process runner worker (`runnerWorker` / polling) and
  `sweep`-related refresh paths are untouched.
- `ExecutionDialog` (read-only details) still opens for deleted rows:
  `acta_db_execution_get` returns them, and the dialog only reads.

## 2. `src/widgets/executionCreateDialog.cpp` — **done**

### Context picker: live contexts only

- `loadContexts()` keeps calling `acta_db_context_query(m_db, nullptr, …)`;
  that lister is **live-only by default** in the DB layer, so deleted
  contexts are not offered in the combo — no code change, a comment
  documents why (a deleted context is refused at `exec create` with
  `ACTA_DB_ERR_NOT_FOUND`).

### Parent-execution picker: keep offering deleted parents

- `loadParentExecutions()` switches from the plain (now live-only) query to
  `execution_query_t q = ACTA_EXEC_QUERY_ANY; q.include_deleted = 1`, and
  appends " (deleted)" to those labels.
- Why: the plain query used to return every row; the new live-only default
  would silently hide deleted parents, breaking replay of a deleted parent
  from the GUI. The design spec says replay is unaffected — a new live row
  is created and the parent reference is audit data that physically
  remains — so the picker must keep offering deleted parents.
  `acta_db_execution_create` only FK-checks the parent, and a soft-deleted
  row still exists physically, so the create succeeds.

## 3. Explicitly out of scope / follow-ups

- **`ContextPanel` / `ContextDialog`**: no delete/restore UI was added.
  `acta_db_context_query` is live-only by default, so soft-deleted contexts
  are hidden from the panel automatically (the intended semantic); there is
  currently no GUI path to restore them — that remains a possible follow-up
  (a "Show trash" + Delete/Restore pair exactly like §1). Context deletion
  itself is available via `acta_cli context delete/restore`.
- **`ExecutionDialog`**: no `deleted_at` display field (the spec does not
  require it; the list row already marks deleted rows).
- No hard delete, no cascades, no space reclamation — per the owner
  decision of 2026-09-13, same as the rest of the phase.
- No DB migration code in the GUI: existing DB files are migrated via
  `acta_cli db exec` per the design spec; `acta_gui/db/schema.sql`
  already carries the columns and the
  `contexts_soft_delete_only` trigger.
