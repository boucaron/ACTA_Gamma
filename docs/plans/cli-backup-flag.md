# Plan — `acta_cli` backup action: atomic snapshot via `VACUUM INTO`

Status: **open — not started.** Small feature; complements the
"Data durability and maintenance" section in `docs/DBDesign.md`, which
today documents the manual `sqlite3 acta.db "VACUUM INTO …"` procedure but
gives the tool no working backup path of its own.

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

## Work items (TBD — none started)

1. Choose implementation (option 1 vs 2 above); implement the command in
   `acta_cli` (and the `acta_db` helper if option 2).
2. `--tools` schema entry + per-action help + `cli_spec.md` row.
3. Tests: fresh DB with rows → backup while the CLI connection (and its
   WAL) is open → the backup opens as a complete database (same row
   counts), `quick_check` ok in the payload; existing target → rejected;
   invalid target (quotes / DB path itself) → rejected; nothing written
   on failure.
4. Docs: README durability line → `acta_cli db backup` (+ frequency
   pointer); `docs/DBDesign.md` maintenance section gains the working
   path and the frequency guidance.

## Deliberately out of scope

- No automatic or scheduled backups (cron, in-app timer) — the operator
  runs the command; frequency is guidance, not enforcement.
- No retention/rotation enforcement, no backup catalog table, no
  incremental backups, no split/multi-file backups.
- No restore command (opening the backup with `--db` / the GUI dialog is
  the restore).
