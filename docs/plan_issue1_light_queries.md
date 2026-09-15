# Plan: light projection variants for high-volume listers (Issue 1)

**Status: complete.** All scope items are done: DB light listers, GUI
panel reloads (plus `ExecutionCreateDialog::loadParentExecutions`), CLI
`--full`, tests, and all doc updates (`docs/known_issues.md` Issue 1 is
marked resolved). Note the one UI trade-off: the context panel filter no
longer searches row content (the blob is not materialized in the list); it
matches the visible columns (Type + Date). The create-dialog context
toOLTIP still shows content (truncated to 400 chars), so `loadContexts()`
deliberately keeps the full lister.

Resolves Issue 1 from `docs/known_issues.md`: the highest-volume
`acta_db` listers return full blob columns for every row, and `limit 0`
clamps to `ACTA_DB_MAX_PAGE` = 10 000, so one call can materialize a
GB-scale heap page. Chosen fix: **Option A — lightweight projections**.

## Goal

Add lister variants that omit the blob columns so list views (CLI
tables, GUI panels) stop fetching, heap-copying, and freeing multi-KB
to MB text per row that they only display truncated.

## Scope

| # | Deliverable | Files |
|---|---|---|
| 1 | `acta_db_context_query_light`, `acta_db_context_query_with_deleted_light` | `acta_db/include/context.h`, `acta_db/src/context.c` |
| 2 | `acta_db_execution_query_light` | `acta_db/include/execution.h`, `acta_db/src/execution.c` |
| 3 | GUI panels reload via light variants | `acta_gui/src/widgets/contextPanel.cpp`, `acta_gui/src/widgets/executionPanel.cpp` |
| 4 | CLI `list` defaults to light, `--full` for blobs | `acta_cli/src/commands/context.c`, `acta_cli/src/commands/execution.c` |
| 5 | Tests | `acta_db/tests/` |
| 6 | Docs | `docs/cli_spec.md`, `docs/status.md` (and `docs/known_issues.md` status) |

## Design

### 1. `acta_db_context_query_light` / `_with_deleted_light`

- Same signature as `acta_db_context_query` / `_with_deleted`:

  ```c
  context_t **acta_db_context_query_light(db_t *db,
                                          const context_query_t *q,
                                          int offset, int limit,
                                          int *out_count, int *err);
  ```

- SELECT columns: `id, type, content_hash, metadata, created_at,
  deleted_at` — **no `content`** (keep `metadata`; revisit only if it
  proves wide in practice).
- `context_t.content` is left `NULL` in returned rows. Struct shape is
  unchanged; `acta_db_context_list_free` is already NULL-safe.
- `q`, `offset`, `limit`, count semantics are identical to the full
  lister; pagination must not change.

Implementation (`acta_db/src/context.c`):

- `build_select_sql` currently hard-codes the full column list. Add a
  `light` flag parameter (or a second `build_select_sql_light`) that
  swaps only the column list; WHERE/ORDER/LIMIT/OFFSET construction and
  `bind_where` stay shared and unchanged.
- `row_to_context` must tolerate the light SQL: either take a
  `light` flag so it skips `sqlite3_column_text` on `content`, or
  detect by column count. Keep its existing alloc-error handling
  unchanged.
- New public functions are thin wrappers over the existing query path
  with `light = 1` (and `live_only = 0` for `_with_deleted_light`).

### 2. `acta_db_execution_query_light`

- Same signature as `acta_db_execution_query`:

  ```c
  execution_t **acta_db_execution_query_light(db_t *db,
                                              const execution_query_t *q,
                                              int offset, int limit,
                                              int *out_count, int *err);
  ```

- Omits `prompt`, `raw_response`, `result`, `error` (those fields stay
  `NULL` in `execution_t`); keeps ids, status, timestamps, parent,
  include_deleted behavior.
- `EXEC_SELECT` (`execution.c:19`) is a `#define` shared with
  `row_to_execution`; introduce a light variant of the SELECT string and
  a matching decode path with identical alloc-error handling.
- `acta_db_execution_count` and the in-flight poll path
  (`acta_db_execution_get`, `acta_db_execution_log_list_by_execution`)
  are **not** touched.

### 3. Doc comments

- Header doc comments for the three new functions: same contract as the
  full lister, with an explicit note "blob fields are NULL; use
  `acta_db_context_get` / `acta_db_execution_get` to fetch them".
- Note in the full listers' doc comments that list views should prefer
  the light variants.

### 4. GUI callers

- `contextPanel.cpp` `ContextPanel::reload()` (~line 217): switch
  `acta_db_context_query(_with_deleted)` → `..._light`.
- `executionPanel.cpp` list reload (~line 555): switch
  `acta_db_execution_query` → `acta_db_execution_query_light`.
- Keep `acta_db_execution_get` for the detail dialog — it legitimately
  needs the blobs. Verify no list-row code path dereferences the blob
  fields expecting non-NULL (display falls back to empty/truncated as
  today).

### 5. CLI callers

- `context list` (`commands/context.c`) and `exec list`
  (`commands/execution.c`): default to the light variants; add a
  `--full` flag that switches to the blob-returning lister.
- Help text: document `--full` ("include blob columns
  (content / prompt / raw_response / result / error)").

### 6. Tests (`acta_db/tests/`)

Follow the existing `test_skill_pagination.c` style; one new test per
light lister:

- blob fields (`content` / `prompt`, `raw_response`, `result`, `error`)
  are `NULL` in every returned row;
- ids, status/type, timestamps, parent, deleted flags are correct and
  match the full lister's non-blob fields for the same query;
- pagination is identical: same `count` from the count API, same page
  contents at a given `offset`/`limit`, including `limit 0` → 10 000
  clamp behavior on a small fixture.

### 7. Docs

- `docs/cli_spec.md`: `--full` flag on `context list` / `exec list`.
- `docs/status.md`: mark Issue 1 done (or note residual scope).
- `docs/known_issues.md`: move Issue 1 status from *open problem* to
  resolved (keep issues 2–3 as-is).

## Non-goals

- No byte-based page cap (Option B) — callers cannot distinguish
  "page too wide" from other failures.
- No change to single-row getters (`*_get`) — one row is fine.
- No struct shape changes; no new error codes.

## Verification

1. Build `acta_db`, `acta_cli`, `acta_gui`.
2. Run `acta_db` test suite incl. the new light-lister tests.
3. Manual: `acta context list` / `acta exec list` on a DB with
   multi-KB rows — confirm truncated output, no blob fetch (spot-check
   with `--full` diff), and `--full` returns the blobs.
4. GUI: reload both panels on a DB with large rows; confirm no
   regression in display and the detail dialog still shows full blobs.
