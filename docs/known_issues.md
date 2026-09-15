# Known issues and residual notes

Findings from the `acta_db` review. Issue 1 is mostly resolved (light
projection listers; GUI switch outstanding); issues 2–3 are low-priority
residual notes.

---

## Issue 1: listers materialize unbounded blob columns

Status: **mostly resolved** (Option A implemented; GUI switch outstanding).

### Problem

The two highest-volume listers in `acta_db` return **full blob columns**
for every row, and callers routinely ask for "all rows" (`limit 0`,
clamped to `ACTA_DB_MAX_PAGE` = 10 000). One call therefore allocates a
heap array of up to 10 000 complete rows, each carrying multi-KB-to-MB
text. There is no per-row or per-page byte cap anywhere in `acta_db`.

Worst case per page:

| Lister | Blob columns returned | Realistic per row | 10 000-row page |
|---|---|---|---|
| `acta_db_context_query` / `_with_deleted` | `content`, `metadata` | KB → MB (full document) | ~GB-scale |
| `acta_db_execution_query` | `prompt`, `raw_response`, `result`, `error` | LLM response: tens of KB → MB | ~GB-scale (realistic execution counts are much lower) |

Everything else is bounded by construction and **not** a problem:

- `execution_log` listers: `message`/`metadata`, but fan-out per execution
  is only ~7–10 phase rows.
- model/skill/revision/folder listers: only small TEXT columns
  (`description`, `configuration`, `prompt_template`, `output_schema`).
- `COUNT(*)` queries and single-row getters (`*_get`): no bulk text.

### Where it is actually exercised

All `acta_db` listers clamp `limit 0` to 10 000 (`db_clamp_limit` in
`acta_db/include/internal.h`, ceiling `ACTA_DB_MAX_PAGE` in
`acta_db/include/db.h`), so "no cap" always means "10 000 full rows".

Callers that trigger a full-blob page:

- **CLI**: `acta_cli/src/commands/context.c:465` (`context list`) and
  `acta_cli/src/commands/execution.c:983` (`exec list`) pass the user's
  `--limit` through; `limit 0` → 10 000 full rows.
- **GUI**:
  - `acta_gui/src/widgets/contextPanel.cpp:217` — `ContextPanel::reload()`
    calls `acta_db_context_query(_with_deleted)` with `limit 0` on every
    reload; every row carries the full `content`.
  - `acta_gui/src/widgets/executionPanel.cpp:555` — execution list reload
    calls `acta_db_execution_query` with `limit 0`; every row carries
    `prompt`, `raw_response`, `result`.

The in-flight run poll is **not** affected: it uses targeted single-row
fetches (`acta_db_execution_get`, `acta_db_execution_log_list_by_execution`
— ~10 rows).

### Why the current design causes it

1. Row decoders read **all** columns into heap strings
   (`row_to_context` in `acta_db/src/context.c`, `row_to_execution` in
   `acta_db/src/execution.c`); there is no projection option.
2. The pagination contract (`db.h`) is a hard cap on *row count* only;
   the comment in `ACTA_DB_MAX_PAGE` acknowledges wide rows but sets no
   byte ceiling.
3. List views (CLI table output, GUI panels) only need a small handful of
   columns per row (id, name/type, status, timestamps) — the blobs are
   fetched, heap-copied, displayed as truncated text, then freed.

### Fix options

#### Option A — lightweight projections (recommended, PoC-sized)

Add projection variants that omit the blob columns:

- `context_query_light`: same signature, but selects
  `id, type, content_hash, metadata, created_at, deleted_at`
  (no `content`). `content` stays `NULL` in the returned `context_t`.
  (Optionally keep `metadata` out too if it can be wide.)
- `execution_query_light`: omits `prompt`, `raw_response`, `result`,
  `error` (keeps ids, status, timestamps, parent).

Implementation notes (both modules follow an identical pattern):

- The row decoder must tolerate a "light" SQL: either a second SELECT
  string + a flag, or build the column list like `build_select_sql`
  already does. Keep `row_to_*` alloc-error handling unchanged.
- The struct shape does not change (blob fields simply stay `NULL`);
  free functions are already NULL-safe.
- New public functions in `acta_db/include/context.h` /
  `acta_db/include/execution.h`, implemented in
  `acta_db/src/context.c` / `acta_db/src/execution.c`.
- Single-row getters stay as-is (one row is fine).
- Callers to switch to the light variants:
  - `acta_gui/src/widgets/contextPanel.cpp` reload;
  - `acta_gui/src/widgets/executionPanel.cpp` list reload (keep
    `acta_db_execution_get` for the detail dialog, which legitimately
    needs the blobs);
  - CLI: add a `--light` flag, or default to light on `list` and add
    `--full` for blob output (decide in CLI spec update,
    `docs/cli_spec.md`).
- Tests: `acta_db/tests/` — one new test per light lister (blob fields
  are NULL, ids/status correct, pagination identical); follow the
  existing `test_skill_pagination.c` style.

#### Option B — cap bytes, not rows

Keep the full lister but refuse/trim when a page would exceed a byte
budget (e.g. 16 MB): `SUM(length(col))` pre-check, or early-terminate the
step loop with `ACTA_DB_ERR_INVALID` + `last_error` explaining the page
is too wide. Ugly: callers cannot distinguish "page too big" from other
failures, and a single 50 MB context would make any page containing it
fail. **Not recommended alone.**

#### Option C — document the ceiling only

Cheapest: state in `docs/DBDesign.md` and the lister doc comments that a
page is at most 10 000 full rows of full-blob content, and that callers
should paginate or use light variants. Does not fix the GUI reload path.

### Suggested scope (do A)

### Status (Option A, in progress)

Done:

- `acta_db`: `acta_db_context_query_light` / `_with_deleted_light` and
  `acta_db_execution_query_light` (+ doc comments in the headers; NOTEs in
  the full listers' doc comments). Blob fields are `NULL` in returned rows;
  struct shapes, count queries, and pagination unchanged.
- `acta_cli`: `context list` / `exec list` default to the light variants;
  `--full` fetches the blobs (`--full` registered in the argparse flag
  table). Documented in `docs/cli_spec.md`.
- Tests: `acta_db/tests/test_light_queries.c` (blob fields NULL, ids /
  status correct, pagination identical, `limit 0` clamp, include_deleted,
  count parity).

Outstanding:

- `acta_gui`: `contextPanel.cpp` reload and `executionPanel.cpp` list
  reload still call the full-blob listers with `limit 0` — switch them to
  the light variants (keep `acta_db_execution_get` for the detail dialog).

---

## Issue 2: pragma failures silently ignored at open

`acta_db_open` (`acta_db/src/db.c`) fires two pragmas and ignores the
results:

```c
sqlite3_exec(handle, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
sqlite3_exec(handle, "PRAGMA foreign_keys=ON;",  NULL, NULL, NULL);
```

Consequences:

- On filesystems without WAL support (and `:memory:`), the DB opens in
  rollback-journal mode with no warning — degraded concurrency/
  durability profile that nobody sees.
- If `PRAGMA foreign_keys=ON` fails, FK enforcement is off on that
  connection and the layer maps FK violations to `ACTA_DB_ERR_FK`
  based on extended result codes that never fire.

Fix (cheap): capture both results; on failure set `last_error` and at
least log/warn. Whether to fail the open is a design choice — the code
comment today says "best-effort", so the minimal honest change is
check-and-report, not fail.

## Issue 3: `acta_db_exec` is a public raw-SQL escape hatch

`acta_db_exec(db, sql)` runs arbitrary SQL on the connection. Correctly
used today only for DDL / schema application (`schema.sql` at first GUI
launch, `acta_cli db exec` / `--file`). Risk: any future caller that
composes `sql` from user-supplied strings reintroduces SQL injection;
the rest of the library is fully parameterized, so this is the only
unparameterized path.

Fix (cheap): keep the API (DDL genuinely needs it) and add a doc comment
stating that `sql` must be static/developer-supplied, never composed
from user input. No code change required beyond the comment.

1. `acta_db`: `acta_db_context_query_light` / `_with_deleted_light` and
   `acta_db_execution_query_light` (+ matching doc comments in the headers).
2. `acta_gui`: context/execution panels reload via the light variants.
3. `acta_cli`: `list` subcommands default to light, `--full` for blobs.
4. Tests in `acta_db/tests/`.
5. Update `docs/cli_spec.md` and `docs/status.md`.
