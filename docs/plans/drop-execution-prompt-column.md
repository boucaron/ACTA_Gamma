# Plan — fully drop the `executions.prompt` column

Status: **in progress** (follow-up to the completed
[`drop-execution-prompt.md`](drop-execution-prompt.md); owner decision,
dev phase — no users, so full removal is acceptable). Items 2
(`acta_db`, `75142db`), 3 (`acta_cli`) and 5 (`acta_runner`) are
implemented and all CLI/DB/runner suites pass; the migration script
exists and all DB files in the repo have already been migrated.

## Rationale

The previous change removed `execution.prompt` as a *contract* input:
no `--prompt` flag, no `prompt` create key, runner user message is always
`context.content`. The `executions.prompt` column was then kept as a
legacy never-written field, still readable by the CLI and GUI. The
project has no users and is still in development, so the dead column is
pure weight: schema surface, read paths, tests and docs that all exist
only to describe something current code never writes. This change
removes the column and **every** remaining read path. Old prompt values
become unrecoverable in the app (a pre-migration DB file can still be
read with raw `sqlite3`) — accepted.

## Migration

One-off script **`acta_gui/db/drop_execution_prompt.sh`** (no migration
framework by design — run it manually, once, on each pre-removal DB
file):

- backs up the DB first (`<db>.bak-<timestamp>`), idempotent (column
  already gone → exit 0), verifies the drop
- fast path: `ALTER TABLE executions DROP COLUMN prompt;` (SQLite
  ≥ 3.35.0)
- fallback for older SQLite: table rebuild preserving rows, FKs and the
  five `idx_executions_*` indexes

Existing DBs keep working *without* the migration (SQLite tolerates an
extra column the schema no longer declares; the new code never reads
it) — the script is for hygiene, not for correctness.

## Target contract

- `executions` DDL: **no `prompt` column** (`acta_gui/db/schema.sql`).
- `execution_t`: no `prompt` member; main and light projections updated.
- CLI: `exec get` JSON has no `prompt` key; `--fields prompt` and
  `--raw_out prompt` are rejected as unknown fields; `--table` has no
  PROMPT column; `exec create` still rejects a `prompt` key with exit 4.
- GUI: no read-only **Prompt** tab in the execution dialog.
- Runner: behaviour unchanged; comment/test references to the column
  disappear.
- Wire schema (`--tools`): `prompt` drops out of the `exec get` output
  keys.

## Work items

### 1. `acta_gui/db/`

- `schema.sql` — delete the `prompt TEXT, -- legacy…` line from the
  `executions` DDL. *(pending — the DB files are already migrated, so
  the source schema is now ahead of nothing but itself)*
- `drop_execution_prompt.sh` — added (`3e468d8`) and tested.
- `engine.db` — migrated (all repo DB files migrated: `acta_cli/
  acta_test_ref.db`, `acta_cli/tmp/acta.db`, `acta_db/test/*.db`, `acta_gui/
  db/engine.db`, `acta_gui/src/release/acta.db`).

### 2. `acta_db/` — done

- `include/execution.h` — removed `char *prompt;` from `execution_t`;
  rewritten the `acta_db_execution_create` doc (no legacy column
  anymore), the main-projection and light-projection notes (blob list
  minus `prompt`).
- `src/execution.c` — main projection: dropped `prompt` from the SELECT
  and the `db_col_text(COL_PROMPT)` assignment (column indexes shift);
  light projection: dropped `prompt` from the column list; create:
  dropped `prompt` from the INSERT column list and the NULL bind
  (5 binds); destructor: dropped `free(e->prompt)`.
- `tests/` — dropped every `->prompt` assert: `test_execution_common.h`
  helper comment, `test_execution_create.c` (three asserts plus the
  whole `test_exec_create_ignores_prompt` case and its registration —
  the concept no longer exists), `test_light_queries.c` (four asserts).

### 3. `acta_cli/` — done (all suites pass)

- `src/commands/execution.c`
  - `exec get` JSON: the `prompt` key is gone.
  - `--raw_out`: the `"prompt"` case is gone → unknown-field error;
    `--fields prompt` can no longer select anything; the light-projection
    `--fields` warning drops `prompt`.
  - `--table`: PROMPT column dropped from header and row print.
  - vlog line: the `prompt` field is gone.
  - help texts (`--full` blob list, `--raw_out` field list) updated.
- `src/json.c` — create-key comment: `prompt` is not a key because the
  column does not exist (KI-2 rejection unchanged).
- `src/tools.c` — `exec list` light-projection note drops `prompt`;
  schema version bumped 3 → 4.
- `tests/exec/execution_test_get.c` — `test_get_with_prompt` replaced by
  `test_get_no_prompt_key` (pins the *absence* of the `prompt` key on ref
  row 4).
- `tests/json/json_test_main.c` — `free(e->prompt)` and the two
  `TNULL(e.prompt)` asserts dropped.
- `tests/exec/execution_test_list_count.c`, `tests/tools/tools_test_main.c`
  — expected schema version 4; comments updated.
- `acta_test_ref.sql` / `acta_test_ref.db` — ref DB rebuilt from the
  columnless seed (seed `executions` rows lose their prompt values).

### 4. `acta_gui/`

- `ui/executionDialog.ui` — remove the `promptTab` / `promptTextEdit`
  from the dialog's tab widget.
- `src/widgets/executionDialog.cpp` — drop the prompt-tab population
  (including the legacy placeholder logic).
- `src/widgets/executionCreateDialog.cpp`, `executionPanel.cpp` — blob
  column comments: drop `prompt` from the list.
- Regenerate `build/ui/ui_executionDialog.h` at build time.

### 5. `acta_runner/` — done

- `src/run.c` — comment near the user-message block now states the
  executions table has no prompt column.
- `tests/run/test_run.c` — the `e.prompt = "USER-PROMPT"` legacy-ignore
  pin is gone (the field does not exist).

### 6. Docs

- `README.md` — drop the legacy-column notes (replayable bullet,
  execution binding, how a run is assembled).
- `docs/DBDesign.md` — `executions` DDL without the column; core-model
  tree; drop the "Why store the prompt?" section.
- `docs/cli_spec.md` — `exec get` output keys and `--fields`/`--raw_out`
  vocabulary lose `prompt`; note the unknown-field rejection.
- `docs/runner_contract.md` — drop references to the legacy column.
- `docs/status.md` — pointer to this plan.

### 7. Verification

1. `make all` (CLI + runner + DB libs compile).
2. `make test` — all three C suites green (DB, CLI, runner).
3. `make -C acta_runner test-e2e` (dead-runner suite).
4. GUI: rebuild, manual smoke — execution dialog has no Prompt tab;
   `exec get` JSON has no `prompt` key.
5. Migration: copy a pre-removal DB file, run
   `drop_execution_prompt.sh`, verify `PRAGMA table_info(executions)`
   has no `prompt` and the row count is unchanged; run it twice
   (idempotent exit 0).
6. Wire check: `acta_cli exec get <id>` JSON → no `prompt` key;
   `exec create --json '{"prompt":"x",…}'` → still exit 4.

## Compatibility notes

- **Breaking, intentionally**: `exec get` output loses the `prompt`
  key; `--fields prompt` / `--raw_out prompt` become usage errors; the
  GUI execution dialog loses the Prompt tab.
- Pre-removal DB files still open and work without migration (extra
  column ignored); the one-off script exists to clean them.
- Legacy prompt values are audit-only data and become unrecoverable via
  the app after migration — acceptable in the dev phase.
