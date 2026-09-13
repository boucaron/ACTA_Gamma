# Soft delete for contexts and executions — `acta_db` layer plan

Companion to [`soft_delete_context_execution.md`](soft_delete_context_execution.md), which is
the design spec. This file is the implementation plan for the **DB layer only**
(`acta_db/include/context.h`, `acta_db/src/context.c`, `acta_db/include/execution.h`,
`acta_db/src/execution.c`). Schema changes (`acta_gui/db/schema.sql`) are already
done; CLI/runner/GUI are out of scope here.

## Reference: the skill pattern

Mirrors `acta_db/include/skill.h` / `src/skill.c`:

- Live listers/counts: `WHERE deleted_at IS NULL`.
- `_with_deleted` variants: **no** `deleted_at` filter — they return live + deleted,
  and callers distinguish via the `deleted_at` field.
- `acta_db_skill_soft_delete`: `UPDATE … SET deleted_at = datetime('now')
  WHERE id = ? AND deleted_at IS NULL` → 0 changes = `ACTA_DB_ERR_NOT_FOUND`
  (covers both "missing" and "already deleted").
- `acta_db_skill_restore`: 0 changes → distinguishes already-live (OK no-op)
  from absent (`NOT_FOUND`).
- `get` returns all rows; `get_live` adds `AND deleted_at IS NULL`.

**Doc deviations to follow (from the spec, not the skill pattern):**

- `context restore` / `execution restore` are **strict**: no *deleted* row
  matching the id → `ACTA_DB_ERR_NOT_FOUND` (restore of a live row is an
  error, unlike `skill_restore`).
- Names are `acta_db_context_delete` / `acta_db_execution_delete` (not
  `soft_delete`), per the spec.
- Context listers: separate `_with_deleted` functions (spec: "keep it
  consistent with how `skill.h` does it" — skill uses separate functions,
  not a query flag).
- Execution listers: `include_deleted` flag **inside** `execution_query_t`
  (spec explicit).

## 1. `acta_db/include/context.h`

- `context_t` gains `char *deleted_at;` (NULL if live).
- New declarations:
  - `int acta_db_context_delete(db_t *db, int id);`
    — `ACTA_DB_ERR_NOT_FOUND` if no row (or already deleted); `ACTA_DB_OK`
    on the flag flip.
  - `int acta_db_context_restore(db_t *db, int id);`
    — `ACTA_DB_ERR_NOT_FOUND` if no **deleted** row matches (strict).
  - `context_t *acta_db_context_get_live(db_t *db, int id, int *err);`
    — NULL for deleted rows (same shape as `acta_db_skill_get_live`).
  - `context_t **acta_db_context_query_with_deleted(db_t *db, const context_query_t *q,
     int offset, int limit, int *out_count, int *err);`
    — same signature as `acta_db_context_query`, no `deleted_at` filter.
  - `int acta_db_context_count_with_deleted(db_t *db, const context_query_t *q, int *err);`
- Replace the "Immutability: context rows have no update / soft-delete API"
  paragraph with the soft-delete lifecycle documentation (flag flip, restore,
  live-only default for listers).

## 2. `acta_db/src/context.c`

- `row_to_context`: decode the new column — SELECT list becomes
  `id, type, content, content_hash, metadata, created_at, deleted_at`
  (in both `acta_db_context_get` and the query builder).
  `acta_db_context_get` keeps its contract but now returns deleted rows too,
  with `deleted_at` populated.
- `build_where` / `build_select_sql` / `build_count_sql`: add a `live_only`
  parameter → appends the static clause `AND deleted_at IS NULL` when set.
  No new binds (constant clause); bind order `[type, hash, limit, (offset)]`
  is unchanged.
- `acta_db_context_query` / `acta_db_context_count`: built with
  `live_only = 1` → **default becomes live-only**; existing callers
  (CLI, GUI, runner, tests) keep working unchanged.
- `_with_deleted` variants: same code path with `live_only = 0`.
- New functions:
  - `acta_db_context_delete`:
    `UPDATE contexts SET deleted_at = datetime('now') WHERE id = ? AND deleted_at IS NULL`
    → `changes > 0 ? ACTA_DB_OK : ACTA_DB_ERR_NOT_FOUND`
    (skill shape; the `contexts_soft_delete_only` trigger allows this UPDATE).
  - `acta_db_context_restore`:
    `UPDATE contexts SET deleted_at = NULL WHERE id = ? AND deleted_at IS NOT NULL`
    → `changes > 0 ? ACTA_DB_OK : ACTA_DB_ERR_NOT_FOUND`
    (strict — no "already live" OK fallback, per the spec).
  - `acta_db_context_get_live`: `… WHERE id = ? AND deleted_at IS NULL`.
- `acta_db_context_free`: `DB_FREE_STR(c->deleted_at)`.
- Legacy listers (`list_all` / `list_by_type` / `list_by_hash`): untouched —
  they wrap `query` and inherit the live-only default.

## 3. `acta_db/include/execution.h`

- `execution_t` gains `char *deleted_at;` (NULL if live).
- `execution_query_t` gains `int include_deleted;` (0 = live only, the default).
- Update the `ACTA_EXEC_QUERY_ANY` macro: add `.include_deleted = 0`.
- New declarations:
  - `int acta_db_execution_delete(db_t *db, int id);`
    — `ACTA_DB_ERR_NOT_FOUND` if no row; `ACTA_DB_ERR_INVALID` if the row is
    `running` or already deleted; `ACTA_DB_OK` otherwise.
  - `int acta_db_execution_restore(db_t *db, int id);`
    — `ACTA_DB_ERR_NOT_FOUND` if no deleted row matches; status untouched.
- Rewrite the "Permanence" paragraph of the state-machine block: execution
  rows have a soft-delete lifecycle now; `delete` is forbidden from
  `running`; `reset` refuses deleted rows.

## 4. `acta_db/src/execution.c`

- `COL_*` enum + `EXEC_SELECT`: append `deleted_at` as the **last** column
  (keeps existing column indices stable); decode in `row_to_execution`.
  `acta_db_execution_get` keeps its contract; now returns deleted rows too.
- `exec_build_where`: when `q->include_deleted == 0`, append the static
  clause `deleted_at IS NULL` (no new binding; `exec_bind_where` unchanged).
  Applies to both `acta_db_execution_query` and `acta_db_execution_count`.
- `acta_db_execution_delete`:
  1. `SELECT status, deleted_at FROM executions WHERE id = ?`
  2. no row → `ACTA_DB_ERR_NOT_FOUND`; `running` → `ACTA_DB_ERR_INVALID`;
     `deleted_at` NOT NULL → `ACTA_DB_ERR_INVALID`
  3. `UPDATE … SET deleted_at = datetime('now')
       WHERE id = ? AND status IN ('pending','completed','failed','cancelled')
       AND deleted_at IS NULL`
     → `changes > 0 ? ACTA_DB_OK : ACTA_DB_ERR_INVALID` (race guard, mirrors
     the transition-function style).
- `acta_db_execution_restore` (aka undelete):
  `UPDATE … SET deleted_at = NULL WHERE id = ? AND deleted_at IS NOT NULL`
  → `changes > 0 ? ACTA_DB_OK : ACTA_DB_ERR_NOT_FOUND`.
  Status untouched: a deleted `failed` row restores to `failed` and is
  then resettable.
- `acta_db_execution_create`: pre-INSERT validation of the referenced
  context: `SELECT deleted_at FROM contexts WHERE id = ?`
  → no row → `ACTA_DB_ERR_FK` (unchanged FK semantics);
  → deleted → `ACTA_DB_ERR_NOT_FOUND` (per the spec);
  → live → proceed.
- `acta_db_execution_reset`: extend the pre-check — missing →
  `ACTA_DB_ERR_NOT_FOUND` (unchanged); **deleted → `ACTA_DB_ERR_NOT_FOUND`**;
  then the existing `failed → pending` transition with its `changes` guard.
  (A deleted execution must be restored before it can be reset/retried —
  see "Inert rules for deleted executions" in the spec.)
- `acta_db_execution_free`: `free(e->deleted_at)`.
- **Deliberately not guarded (per the spec, flagged):**
  `start` / `cancel` / `complete` / `fail` / `set_raw_response` on a deleted
  row — the spec only names `reset`. Implemented as written unless the owner
  decides all transitions should refuse deleted rows.

## 5. No other `acta_db` changes

- `acta_db.h` (umbrella) already includes `context.h` and `execution.h` —
  nothing to do.
- `db.h` error codes already cover everything (`NOT_FOUND`, `INVALID`,
  `FK`, `SQL`, `ALLOC`) — no new codes.
- `execution_log` untouched (its FK `CASCADE` only concerns out-of-scope
  hard delete).
- Existing `acta_db` unit tests keep passing: all their rows are live, and
  lister defaults stay live-only.

## 6. New DB-layer tests (mirrors the spec's "Tests" section)

- `acta_db/tests/test_context_deleted.c` (+ `TEST_MODULES` entry in
  `acta_db/Makefile`):
  - delete → restore round trip; delete a deleted context → `NOT_FOUND`;
    restore a live context → `NOT_FOUND`.
  - `get` on a deleted row → row with `deleted_at` set; `get_live` → NULL.
  - `query` / `count` default live-only; `_with_deleted` variants include
    deleted rows.
  - trigger via `acta_db_exec`: `UPDATE contexts SET content = …` → fails;
    `UPDATE contexts SET deleted_at = …` → OK.
- `acta_db/tests/test_execution_deleted.c`:
  - `delete` from `pending` / `completed` / `failed` / `cancelled` → `OK`;
    from `running` → `INVALID`; already deleted → `INVALID`.
  - `restore` preserves status (delete a `failed` row, restore, status still
    `failed`).
  - `reset` on a deleted row → `NOT_FOUND`.
  - `create` with a deleted `context_id` → `NOT_FOUND`; with a missing
    `context_id` → `ERR_FK` (unchanged).
  - `query` default live-only; `include_deleted = 1` → includes deleted rows.

## 7. Out of scope for this phase (follow-up work)

- `acta_cli`: `context delete/restore`, `exec delete/restore`,
  `--include_deleted` / `--deleted` flags, `cli_spec.md` rows, `tools.c` (T3).
- `acta_runner`: `run --pending` claim query + `AND deleted_at IS NULL`.
- `acta_gui`: execution/context panels, context pickers, Retry disable.
