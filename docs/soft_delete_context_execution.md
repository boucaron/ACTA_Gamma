# Soft delete for contexts and executions

Decision: **add a `deleted_at` soft-delete lifecycle to `contexts` and
`executions`, mirroring the existing skill/model/folder pattern.**
(owner decision, 2026-09-13)

This replaces the "Things deliberately missing" note in `DBDesign.md`:
deletion of contexts/executions, when needed, **is** a `deleted_at` soft
delete — and it is now needed.

Implementation plan for the DB layer: `soft_delete_context_execution_acta_db.md`.

## Why

- Folders, models and skills already have `delete` / `restore` /
  `--include_deleted`. Contexts and executions are the only entities
  without a lifecycle, which is inconsistent for a tool whose job is
  running and tidying executions.
- Executions accumulate (failed runs, discarded experiments); hiding them
  keeps `exec list` usable. Contexts accumulate the same way.
- Soft delete is **restorable and lossless**: the content stays physically
  in the DB, the audit trail is intact. It is compatible with the
  PointOfView "executions are historical artifacts" stance — an artifact
  can be hidden, but it is never destroyed.
- Hard delete remains out of scope (owner decision, 2026-07-10).

## Semantics (apply to both entities)

- Soft delete is a **flag flip**: set `deleted_at = datetime('now')`;
  restore (aka undelete) sets it back to `NULL`. No data is removed, no
  space is reclaimed, nothing cascades — restoring a row makes it
  visible and usable again exactly as before, with the same id, data and
  status.
- Foreign keys are never touched (a soft delete is an UPDATE, not a
  physical DELETE), so the existing `ON DELETE RESTRICT` /
  `ON DELETE SET NULL` constraints need no change.
- Default visibility: listers, counts, pickers and transitions operate on
  **live rows only** (`deleted_at IS NULL`) unless an explicit
  `--include_deleted` / `--deleted` filter says otherwise — the exact
  model/skill pattern.

## Contexts

### Schema

Add the column and replace the immutability trigger so that *only*
`deleted_at` may change (content immutability is otherwise preserved):

```sql
ALTER TABLE contexts ADD COLUMN deleted_at TEXT;

DROP TRIGGER IF EXISTS contexts_immutable;
CREATE TRIGGER contexts_soft_delete_only
BEFORE UPDATE ON contexts
WHEN (
    NEW.type IS NOT OLD.type
    OR NEW.content IS NOT OLD.content
    OR NEW.content_hash IS NOT OLD.content_hash
    OR NEW.metadata IS NOT OLD.metadata
)
BEGIN
  SELECT RAISE(ABORT, 'contexts are immutable: only deleted_at may change');
END;
```

No revision snapshot trigger is needed (contexts have no revisions).

### Rules

- Deleting a context that has executions **is allowed**: executions stay
  fully inspectable because the content physically remains; restore makes
  it risk-free.
- A deleted context is **unselectable for new work**: `exec create` (CLI,
  GUI dialog, runner resolution) must require a live context and refuse
  a deleted one with `ACTA_DB_ERR_NOT_FOUND`.

### DB layer (`acta_db/include/context.h`, `src/context.c`)

Mirror `skill.h`:

- `context_t` gains `char *deleted_at;` (NULL if live).
- `int acta_db_context_delete(db_t *db, int id);`
  — `ACTA_DB_ERR_NOT_FOUND` if no row matches id; `ACTA_DB_OK` on
  success.
- `int acta_db_context_restore(db_t *db, int id);`
  — `ACTA_DB_ERR_NOT_FOUND` if no **deleted** row matches id.
- `acta_db_context_get` keeps its contract but now returns deleted rows
  too (their `deleted_at` is populated); add
  `context_t *acta_db_context_get_live(db_t *db, int id, int *err);`
  which returns NULL for deleted rows (same shape as
  `acta_db_skill_get_live`).
- `context_query_t` gains `int include_deleted;` (0 = live only — the
  default, matching existing queries); alternatively a separate
  `_with_deleted` lister/count per the skill pattern. Pick one and keep
  it consistent with how `skill.h` does it.

## Executions

### Schema

```sql
ALTER TABLE executions ADD COLUMN deleted_at TEXT;
```

No trigger needed: status transitions are enforced in the C layer, and
`deleted_at` is the only extra mutable column.

### State rules

The **one real decision** of this doc:

- `delete` is allowed from `pending`, `completed`, `failed`,
  `cancelled` — and **not from `running`** (the runner owns the row;
  deleting mid-run would race `start()/complete()/fail()` and confuse
  `sweep`). Attempt → `ACTA_DB_ERR_INVALID`.
- `restore` just clears the flag; **status is untouched** (a deleted
  `failed` row restores to `failed`).

### Inert rules for deleted executions

- `acta_db_execution_reset` (and the GUI Retry button) refuses deleted
  rows → `ACTA_DB_ERR_NOT_FOUND`: a deleted execution must be **restored
  first** before it can be reset/retried. Restore only clears the flag
  and preserves status, so a restored `failed` row is resettable as
  usual.
- Runner claim (`acta_runner run --pending`) must skip deleted rows: the
  pending query gets a `deleted_at IS NULL` guard — the same restore-first
  rule applies to a deleted `pending` row.
- Replay (`exec create` with `--parent_execution_id`) is unaffected —
  replay creates a *new* live row, so the work can be re-run **without
  restoring** the deleted parent.
- No cascade: children of a deleted parent stay live and readable
  (`parent_execution_id` is audit data, `ON DELETE SET NULL` never fires).
- `execution_logs` are untouched by soft delete (their FK `CASCADE` only
  matters for hard delete, which stays out of scope).

### DB layer (`acta_db/include/execution.h`, `src/execution.c`)

- `execution_t` gains `char *deleted_at;` (NULL if live).
- `int acta_db_execution_delete(db_t *db, int id);`
  — `ACTA_DB_ERR_NOT_FOUND` if no row; `ACTA_DB_ERR_INVALID` if the row
  is `running` or already deleted; `ACTA_DB_OK` otherwise.
- `int acta_db_execution_restore(db_t *db, int id);`
  — `ACTA_DB_ERR_NOT_FOUND` if no deleted row matches id.
- `execution_query_t` gains `int include_deleted;` (0 = live only, the
  default).
- `acta_db_execution_create` must validate that the referenced context is
  live (deleted context → `ACTA_DB_ERR_NOT_FOUND`).
- Update the "Permanence" comment in the state-machine block: execution
  rows have a soft-delete lifecycle now.

## CLI (`acta_cli`)

Follow the existing wire-format atoms from `cli_spec.md`:

| Command | stdout on success | Notes |
|---|---|---|
| `context delete <id>` | `{"deleted":true}` | `emit_deleted`; not found → exit 1 |
| `context restore <id>` | `{"id":N,"restored":true}` | `emit_ok_restored`; live/not-found → exit 1 |
| `context get <id>` | context JSON | add `--include_deleted` / `--deleted` (default: live only → deleted → exit 1) |
| `context list` / `context count` | as today | add `--include_deleted` / `--deleted` |
| `exec delete <id>` | `{"deleted":true}` | not found → exit 1; `running` → exit 4 (`ACTA_DB_ERR_INVALID`) |
| `exec restore <id>` | `{"id":N,"restored":true}` | live/not-found → exit 1 |
| `exec list` / `exec count` | as today | add `--include_deleted` / `--deleted` |

`cli_spec.md` is the source of truth — add these rows to the `context`
and `exec` tables, and to the `--tools` schema (T3, generated from
`src/tools.c`, never hand-written).

## Runner (`acta_runner`)

- `run --pending` claim query: add `AND deleted_at IS NULL`. A `run
  <execution-id>` on a deleted execution fails with the standard
  not-found path.
- Single-execution claim is otherwise unchanged: it resolves a specific
  `skill_revision_id` / `model_revision_id` / `context_id`, so a live
  execution's inputs are always live rows anyway (deletion of a skill or
  context only matters at `exec create` time, which is already guarded).

## GUI (`acta_gui`)

- Execution list: hide deleted rows by default; per-row restore action
  mirroring the model/skill rows; Retry button disabled for deleted rows.
- Context picker (ExecutionCreateDialog and Run dialog): list live
  contexts only.
- No delete button for a `running` execution (matches the C-layer rule).

## Migration

Existing DB files:

```sql
ALTER TABLE contexts   ADD COLUMN deleted_at TEXT;
ALTER TABLE executions ADD COLUMN deleted_at TEXT;
-- contexts trigger replacement, see above
```

Existing rows have `deleted_at = NULL`, i.e. they are live. Apply via
`acta_cli db exec` (or `--file`); idempotent check: `PRAGMA table_info`
before applying.

## Tests

Mirror the existing delete/restore tool tests (`acta_cli/tests/`,
`tools_test_main.c`) plus:

- `context delete` / `restore` round trip; delete a deleted context →
  not found; restore a live context → not found.
- `context get` default (deleted → exit 1) and `--deleted`.
- `exec delete` on `running` → exit 4; on `pending`/`failed`/
  `completed`/`cancelled` → `{"deleted":true}`.
- `exec restore` preserves status (delete a `failed` row, restore,
  status still `failed`); `exec reset` on a deleted row → not found.
- `exec create` with a deleted `--context_id` → not found.
- Runner: `run --pending` skips a deleted pending execution.
- `contexts_soft_delete_only` trigger: UPDATE of `content` → abort;
  UPDATE of `deleted_at` → ok.

## Out of scope

- Hard delete (owner decision, 2026-07-10).
- Cascading delete to child executions or executions of a context.
- Space reclamation (deleted rows still occupy storage).
- Deletion of model/skill **revisions** themselves (they already carry
  `deleted_at` via the snapshot trigger).
