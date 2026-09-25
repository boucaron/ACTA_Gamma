# Plan: schema migration path

Status: implemented & verified (all `make all` / `make test` suites
PASS, `make gui` green) — follow-up to
[`remove-db-exec.md`](remove-db-exec.md). Committed. See Implementation
notes below.

## Context

`docs/DBDesign.md` states: "No schema versioning or migration framework. …
if the schema changes, there is no built-in migration path. Manual
`ALTER TABLE` recipes … are the supported way to move a file forward."
Once the raw-SQL `db exec` action is removed (see
`remove-db-exec.md`), there is **no** operator path for evolving an
existing, data-carrying database at all. This plan adds a versioned,
static, no-user-SQL migration path.

## Design

- **`PRAGMA user_version` is the schema version.** The value recorded in
  the file *is* the schema version (e.g. `0.1`). A brand-new file from
  `db init` is created directly at the current version; `user_version = 0`
  means "no schema / pre-migration".
- **Migrations are repo-static files, one per version.**
  `acta_db/migrations/<version>.sql` (e.g. `0.1.sql`) containing only
  static DDL. No user-supplied SQL is ever accepted: `db migrate` reads
  the repo files, in ascending version order, and applies only those
  newer than the file's `user_version`.
- **A new `acta_cli db migrate` action** (no arguments, no SQL flags):
  1. open the DB (`--db` / `$ACTA_DB` / config / default resolution),
  2. read `PRAGMA user_version`,
  3. apply each migration `v > current`, in order, inside a
     `BEGIN … COMMIT` transaction per migration (a failing migration
     rolls back itself; the DB keeps its prior `user_version`),
  4. set `PRAGMA user_version = v` after each applied migration,
  5. print `{"status":"ok","schema_version":"<v>"}` (exit 0); any failure
     → exit 4 with the failing migration named on stderr.
- **Version record table.** The canonical schema version ledger lives in
  this section (single source of truth; a future `db schema-version`
  action may print it):

  | VERSION | Change |
  |---|---|
  | 0.1 | Baseline schema: 10 tables (model_folders, models, model_revisions, skill_folders, skills, skill_revisions, contexts, executions, execution_log, …), indexes, and the revision auto-snapshot triggers. Applied by `db init` (fresh file) or `0.1.sql` (migration path). |

  Every future schema change adds exactly one row here and one
  `acta_db/migrations/<version>.sql` file.

## Compatibility with `db init`

- `db init` (fresh file, per `remove-db-exec.md`) creates the 0.1 schema
  and sets `PRAGMA user_version = 0.1` on creation.
- `db migrate` on a fresh-but-schema'd file is a no-op (prints the
  current version). On a file with `user_version = 0` but tables present
  (e.g. an existing pre-migration file), the first check is a
  `PRAGMA table_info` sanity check: if the baseline tables are missing it
  fails closed with "not an ACTA Gamma database", instead of double-
  applying.
- The embedded static schema text shared by `db init`, the GUI first
  launch, and `0.1.sql` is the same content (generated from
  `acta_db/schema.sql` — single source of truth, same rule as
  `remove-db-exec.md`).

## Files & docs

- New: `acta_db/migrations/0.1.sql` (generated from `acta_db/schema.sql`).
- `acta_cli`: add `db migrate` (handler, help section, tools-table entry,
  `--tools` version bump alongside the `db exec` removal).
- `acta_db`: `acta_db_open` reports the effective `PRAGMA user_version`
  alongside the existing journal_mode/foreign_keys open-time notes.
- `docs/DBDesign.md`: replace "No schema versioning or migration
  framework" with the `user_version` + static-migration design; update
  the durability section ("schema changes via `db exec`" → "via
  `db migrate`").
- `docs/cli_spec.md`: `db migrate` row + wire format.
- `README.md`: one sentence in the "Fresh databases" note: existing
  databases are upgraded with `acta_cli db migrate`.
- `docs/status.md`: Done note when landed.

## Test plan

- `acta_db/tests`: `user_version` set on open of a schema'd file;
  reported in the open-time notes.
- `acta_cli/tests/db`: `db migrate` scenarios —
  - fresh empty file → applies 0.1, `user_version` 0, → `0.1`,
    `{"status":"ok","schema_version":"0.1"}`;
  - already at 0.1 → no-op, exit 0;
  - `user_version` 0 with a foreign file (no ACTA tables) → exit 4,
    "not an ACTA Gamma database";
  - a failing 0.2 migration (test fixture) → rolled back,
    `user_version` unchanged, exit 4, migration named on stderr;
  - ordering: two pending migrations apply in ascending order.
- `tools` suite: entry count and `version` bump; `db.migrate` `success`
  shape (`json`, keys `["status","schema_version"]`).
- GUI smoke: first-launch schema application still sets `user_version = 0.1`
  (so a later `db migrate` on the GUI-created file is a clean no-op).

## Open questions

1. Is `PRAGMA user_version` (INTEGER) enough, or do we want a
   `schema_version TEXT` table row to carry semver-like strings such as
   `0.10`? Integer is simpler and monotonic; semver needs a table.
   Default: `PRAGMA user_version` with a minor-bump convention
   (0.1 → 0.2 → 0.3, no 0.10).
2. Do migrations ever include `UPDATE` (data) statements, or DDL only?
   Default: DDL only for the POC; data migration is explicitly out of
   scope.

## Rollout (single logical change, after `remove-db-exec` lands)

1. Generate `acta_db/migrations/0.1.sql` from `acta_db/schema.sql`.
2. `acta_db_open`: report `PRAGMA user_version`.
3. `acta_cli db migrate`: read-apply-set loop with per-migration
   transactions.
4. `db init` / GUI first launch: set `user_version = 0.1` on creation.
5. Docs: DBDesign, cli_spec, README, status.
6. Tests: `acta_db`, `acta_cli db`, `tools`, GUI smoke.

## Implementation notes (as landed)

- **Versioning primitive:** the integer `PRAGMA user_version` IS the
  recorded schema version, with the 0.1 → 1, 0.2 → 2 mapping (so
  "set `user_version = 0.1`" above means "set `user_version = 1`" —
  the displayed version is `0.<integer>`). `PRAGMA user_version` is a
  no-op inside a transaction, so each migration's version is set AFTER
  its `COMMIT`.
- **Repo-static migrations:** `acta_db/migrations/0.1.sql` is a
  verbatim copy of `acta_db/schema.sql`; embedded in the CLI as
  `include/migrations_sql.h` (`static const acta_migration_t
  ACTA_MIGRATIONS[]`, `{0, NULL}` sentinel), generated by the `acta_db`
  Makefile rule with the same escaping rules as `schema_sql.h`.
- **acta_db:** `acta_db_schema_version` / `acta_db_set_schema_version`
  added; the open-time informational note (`acta_db_last_error` on a
  successful open) now appends "Schema version: 0.N (PRAGMA
  user_version=N). Informational note, not an error." when the version
  is nonzero (combined with the pragma-degradation notes).
- **`db init` versioning:** fresh file → schema applied, then
  `user_version = 1`; already-schema'd file with `user_version = 0`
  (legacy, pre-versioning) → adopted to `user_version = 1`. Partial /
  foreign file → fail closed, exit 4, no version set.
- **`db migrate`:** rejects any SQL input of any kind (positional /
  `--sql` / `--file` / `--sql_stdin` / global `--stdin` → exit 4).
  `user_version = 0` with no user tables → migrate from 0.1; `user_version
  = 0` with user tables present → fail closed ("not an ACTA Gamma
  database", exit 4). Core loop in `db_migrate_apply` (declared in
  `commands.h`, defined in `src/commands/db.c`, callable from tests with
  synthetic fixtures): each pending migration in its own `BEGIN … COMMIT`,
  version set after the commit; a failing migration rolls back, the file
  keeps its prior version, and the error is "migration 0.N failed:
  <detail>" (exit 4). Success: `{"status":"ok","schema_version":"0.N"}`
  (`--table` → `ok`), exit 0.
- **GUI:** `MainWindow::createDatabase` sets `user_version = 1` after the
  schema application (warning on failure, non-fatal); `smoke_test.sh`
  sets `PRAGMA user_version=1` after applying the schema.
- **Tools:** 75 → 76 entries; `--tools` schema `version` 5 → 6; compact
  header "v5"; `db.migrate` row with `input: "none"` and success
  `[{status}, {schema_version}]`; `db` action list 4 → 5.
- **Docs:** `docs/DBDesign.md` (versioning paragraph replaces "No schema
  versioning", version ledger table with the 9 baseline tables,
  sole-callers and durability wording), `docs/cli_spec.md` (Common-shapes
  row, `db migrate` paragraph, `## db` table row), README (fresh-database
  paragraph), `acta_cli/Makefile` comment.
- **Tests:** `acta_db/tests/test_db.c` (set/read cycle + open-note
  reporting); `acta_cli/tests/db/db_test.c` (migrate fresh / no-op /
  foreign / failing-migration fixture / ordering / input-rejection;
  `db help` lists `migrate`); `acta_cli/tests/tools/tools_test_main.c`
  (76 entries, 5 `db` actions, `db.migrate` in the input-"none"
  invariant).
- **Verification (done):** `make all` / `make test` (every suite PASS,
  including the new `db`-suite migrate scenarios and the `acta_db`
  `user_version` tests) and `make gui` from the repo root. Two
  post-edit fixes landed during verification: the `acta_db_open`
  note-combining use-after-free (fixed by taking ownership of the
  version note) and `db_migrate_apply` setting `*out_version` on the
  failure path (last successfully applied version); the generated
  header's comment was also rewritten so no `*/` sequence occurs inside
  it (the original `migrations/*.sql` glob terminated the comment
  early). Manual first-launch GUI smoke and `db backup --to` +
  `db migrate` on the backed-up file (the backup path from the design
  notes) remain available as operator checks.
