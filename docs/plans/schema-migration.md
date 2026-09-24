# Plan: schema migration path

Status: proposal (not started) — follow-up to
[`remove-db-exec.md`](remove-db-exec.md)

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
