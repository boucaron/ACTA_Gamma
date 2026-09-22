# Plan — `acta_cli` backup action: atomic snapshot via `VACUUM INTO`

Status: **done.** Option 2 was chosen at scheduling: the SQLite backup
C API (`sqlite3_backup_init` / `step` / `finish`) behind the small
`acta_db` helper `acta_db_backup()` (`acta_db/include/db.h`,
`acta_db/src/db.c`) — the target reaches SQLite only as a C API
argument, never as SQL text, and the same consistent-snapshot
semantics hold (WAL state folded in, run against the open
connection). The CLI action is `acta_cli db backup --to <target>` in
`acta_cli/src/commands/db.c` (strict target validation: non-empty,
no quote/semicolon/backslash, not the DB path itself — via the new
`acta_db_main_path()` accessor, which stores the opened path in the
`db_t` at `acta_db_open` — and not already existing; the `stat`
probe doubles as the "path the process cannot create" check), payload
`{"target": …, "bytes": …, "quick_check": "ok"}`; the built-in
verification (reopen the backup on its own connection, `PRAGMA
quick_check`, a failed check is deleted and reported as failure) lives
in `acta_db_backup` itself; nothing is left behind on any failure.
The `--tools` schema gained the `db.backup` entry (75 entries now,
`to` added to `entity_flag_specs`), the tests are the `backup` section
of `acta_cli/tests/db/db_test.c` (success with equal row counts while
the live connection and its WAL are open, `--table` mode, missing
`--to`, existing target rejected with the file untouched, invalid
characters / DB-path target rejected with nothing written,
uncreatable target rejected), and the docs (cli_spec.md `db` table row
+ contract paragraph, DBDesign.md maintenance section with the working
path and the frequency guidance, README durability line re-pointed)
landed with work item 4.

## Context

- The README's durability line says "copy the file before destructive
  operations" — but a raw file copy of `acta.db` **while the database is
  open is not a consistent snapshot**: in WAL mode committed data may
  still sit in `acta.db-wal`. The manual procedure in `docs/DBDesign.md`
  works only if the operator remembers to close every consumer first, or
  to run `VACUUM INTO` from a separate `sqlite3` process.
- `VACUUM INTO '<target>'` is the atomic copy: it writes a complete,
  self-contained, consistent database file (defragmented, WAL state
  folded in) and can be run against the open connection — no need to
  close the GUI/CLI/runner first.

## Target contract

- **New action:** `acta_cli db backup --to <target>` (`db` is the
  file-level escape-hatch entity; backup is a file-level operation, so it
  lives there). Success: a JSON payload
  `{"target": "<path>", "bytes": <size>, "quick_check": "ok"}`.
- **Implementation decision (pick one when scheduled):**
  1. `VACUUM INTO '<target>'` through `acta_db_exec` — the existing
     static-SQL path; requires strict validation of `<target>` because it
     is user-supplied and lands in SQL text: reject empty, reject quote/
     semicolon/backslash characters, reject a target equal to the DB
     path, reject paths the process cannot create.
  2. `sqlite3_backup_init` (C API) from a small `acta_db` helper — no SQL
     interpolation at all, same consistent-snapshot semantics, busy-safe.
     Preferred if the `acta_db` surface is touched anyway.
- **No silent overwrite:** if `<target>` already exists the command fails
  (standard error JSON, rc 1); there is no overwrite flag. Rotation is by
  dated name (`acta_backup_YYYYMMDD.db`), so the operator names
  generations.
- **Verification built in:** after the copy, open the backup on its own
  connection and run `PRAGMA quick_check;`; the result goes in the payload.
  A backup that does not check is reported as failed, not written-and-
  ignored.
- **Error contract:** uncreatable target / validation failure → the usual
  CLI usage error (exit 10); snapshot failure → standard error JSON with a
  non-zero rc. Nothing is written on failure.
- **Contract docs:** `docs/cli_spec.md` (new `db backup` action row,
  flags, payload shape), `docs/DBDesign.md` maintenance section (the
  manual procedure stays as the reference; the CLI action is the working
  path), README durability line re-pointed to `acta_cli db backup`.

## Documented backup frequency (minimum, guidance only)

- **Before any destructive operation** — schema changes via `db exec`,
  manual file operations, migrating to a new file: take a backup first.
- **At the end of any session that produced executions** — executions and
  their logs are the high-value records (soft delete means they stay in
  the file forever and are never recoverable from the live file alone if
  the file is lost).
- Keep backups **outside the DB directory**; name them
  `acta_backup_YYYYMMDD.db`; keep the last ~7 by deleting older ones
  manually (no retention enforcement — see out of scope).
- **Restoring** = opening the backup, not a special command:
  `acta_cli --db acta_backup_20260920.db …` / the GUI's *Choose database
  file* dialog pointed at the backup file. Verify a restored file with
  `PRAGMA integrity_check` (full) before relying on it.

## Work items

1. **Done —** Option 2 implemented: `acta_db_backup()` +
   `acta_db_main_path()` in `acta_db` (backup C API, no SQL
   interpolation; built-in `quick_check` verification; partial target
   removed on any failure) and the `db backup` action in
   `acta_cli/src/commands/db.c` (strict target validation, no silent
   overwrite, payload `{"target","bytes","quick_check"}`).
2. **Done —** `--tools` schema entry `db.backup` (75-entry table),
   `--to` added to `entity_flag_specs`, per-action help section
   (`usage_backup`), `cli_spec.md` row + contract paragraph.
3. **Done —** Tests in `acta_cli/tests/db/db_test.c`: backup while the
   live connection (and its WAL) is open → the backup opens as a
   complete database (same context row counts), `quick_check` ok in
   the payload; `--table` mode; missing `--to` → exit 10; existing
   target → rejected, file untouched; invalid characters (quote /
   semicolon / backslash) and the DB path itself → rejected with
   nothing written; uncreatable target → rejected.
4. **Done —** Docs: README durability line re-pointed to
   `acta_cli db backup`; `docs/DBDesign.md` maintenance section gains
   the working path, the manual procedure demoted to reference, and
   the frequency guidance (before destructive operations, end of
   sessions with executions, dated names outside the DB directory,
   restore by opening the backup).

## Deliberately out of scope

- No automatic or scheduled backups (cron, in-app timer) — the operator
  runs the command; frequency is guidance, not enforcement.
- No retention/rotation enforcement, no backup catalog table, no
  incremental backups, no split/multi-file backups.
- No restore command (opening the backup with `--db` / the GUI dialog is
  the restore).
