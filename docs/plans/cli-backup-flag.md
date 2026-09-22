# Plan — `acta_cli` backup action: atomic snapshot via `VACUUM INTO`

Status: **done** (implementation, schema, tests, docs) — but see
**Current state & follow-ups** at the bottom: one open `test_tools`
cross-check failure touches the `db.backup` entry, and the code review's limitations — the Windows backslash
rejection, the naive self-backup guard, and the portability nit are
now fixed; the TOCTOU caveat is deliberately accepted; and one
`test_tools` cross-check failure remains open.

Option 2 was chosen at scheduling: the SQLite backup
C API (`sqlite3_backup_init` / `step` / `finish`) behind the small
`acta_db` helper `acta_db_backup()` (`acta_db/include/db.h`,
`acta_db/src/db.c`) — the target reaches SQLite only as a C API
argument, never as SQL text, and the same consistent-snapshot
semantics hold (WAL state folded in, run against the open
connection). The CLI action is `acta_cli db backup --to <target>` in
`acta_cli/src/commands/db.c` (strict target validation: non-empty,
no quote/semicolon characters — and no backslash on POSIX, while on
Windows the backslash is the native separator and is allowed — not the
same file as the DB path itself (canonicalized absolute-path
comparison) — via the new
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
  as a CLI usage error (exit 10, `ACTA_CLI_ERR`, `code: -10`) — the same
  class as the other validation failures, not a DB error; there is no
  overwrite flag. Rotation is by dated name
  (`acta_backup_YYYYMMDD.db`), so the operator names generations.
- **No stdin input:** the global `--stdin` is a JSON-input flag and
  `db backup` takes no input; passing it is rejected explicitly
  (`ACTA_DB_ERR_INVALID`, exit 4), the same rule as `db exec`.
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
   semicolon / backslash on POSIX — backslash excluded on Windows, the
   native separator there) and the DB path itself → rejected with
   nothing written; uncreatable target → rejected.
4. **Done —** Docs: README durability line re-pointed to
   `acta_cli db backup`; `docs/DBDesign.md` maintenance section gains
   the working path, the manual procedure demoted to reference, and
   the frequency guidance (before destructive operations, end of
   sessions with executions, dated names outside the DB directory,
   restore by opening the backup).

## Current state & follow-ups

1. **Open: `test_tools` cross-check failure.**
   `tests/tools/tools_test_main.c:451` (`TEST(rc != EXIT_CLI)`) fails for
   exactly one of the 75 schema-generated entries, **in-process only**:
   the same argv shapes run out-of-process (subprocess, fresh replica,
   `./tmp` backup target) produce no `rc=10` anywhere. The `db.backup`
   handler paths were reviewed line by line and none of its exit-10
   branches fire for the cross-check argv (`--to ./tmp/acta_tools_backup_<pid>_<n>.db`):
   flag present, allowed characters, ≠ db path, `stat` → ENOENT. The
   remaining exit-10 sources are in the parse layer (`parse_globals` /
   `cmd_args_validate`), which is where investigation continues. Until
   this is fixed the `test_tools` suite is red even though every other
   suite passes against the regenerated `acta_test_ref.db`.
2. **Fixed — Windows backslash targets allowed.** The char validation
   now bans `\` only on POSIX (`#ifndef _WIN32`); on Windows the
   backslash is the native separator and passes. The error message is
   platform-conditional (quote/semicolon on Windows; quote/semicolon/
   backslash on POSIX). `db_test.c`'s `test_backup_invalid_chars`
   excludes the backslash case on `_WIN32` accordingly, and the help
   text (`usage_backup`) + cli_spec.md paragraph now state the
   platform rule.
3. **Fixed — self-backup guard compares files, not spellings.**
   `cmd_db` now canonicalizes both sides (`canon_path`: resolve
   relative paths against the cwd, drop trailing slashes) and compares
   case-insensitively on Windows (`_stricmp`), so `./x`, `x`, an
   absolute spelling, or a case variant of the DB path are all caught
   by the DB-path-itself rule.
4. **Accepted caveat — TOCTOU on "remove on failure".** The existence
   check and the later open-with-`CREATE` are separate
   moments; if another process creates the target in between, the backup
   overwrites it and a failure would `remove()` that racing file rather
   than only its own output. Deliberately accepted for a single-user
   CLI; no code change.
5. **Fixed — portability nit.** `acta_db_backup` now checks
   `st.st_size == 0` (a zero-size backup is a failed copy) instead of
   `st.st_size < 0`, which is ill-typed where `st_size` is unsigned.

## Deliberately out of scope

- No automatic or scheduled backups (cron, in-app timer) — the operator
  runs the command; frequency is guidance, not enforcement.
- No retention/rotation enforcement, no backup catalog table, no
  incremental backups, no split/multi-file backups.
- No restore command (opening the backup with `--db` / the GUI dialog is
  the restore).
