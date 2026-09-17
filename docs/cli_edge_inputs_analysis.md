# acta_cli — conflicts, edge inputs & `db exec` analysis

Exploration-only review of flag-conflict rules, edge inputs, and the
`db exec` input forms, verified against the running binary on a test DB
(`acta_cli/tmp/acta.db`). Companion to `cli_spec.md` and
`cli_download_analysis.md`. Findings only, checked 2025-07-25.

## Verified working

- `--stream` conflicts: `--stream --table` and `--stream --id_only` →
  exit 4 `"conflicting output modes: --stream is incompatible with
  --count, --table and --id_only"` (descriptive, T2 holds).
- Malformed `--json` → exit 4 `"invalid JSON body"`.
- `--offset -5` → exit 4 `"--offset must be a non-negative integer"`;
  `--limit 0` = unlimited (matches help); `--offset 100` → `[]`.
- `skill list --folder_id N` filters correctly (1 and 4 rows per folder
  in the test); `skill count --all` → total.
- `db exec --file` works (UPDATE applied, `{"status":"ok"}`); the
  64 KiB cap is enforced with a clear message: `"SQL file
  'tmp/big2.sql' too large (93382 bytes, max 65536): split the file or
  use --sql_stdin for large inputs"` → exit 4.

## Bugs / mismatches (priority order)

1. **`db exec --sql_stdin` is completely broken (always exit 4).**
   Piped SQL + `--sql_stdin` → `"no SQL source: provide SQL as a
   positional argument, --sql, --file, or --sql_stdin"` even though
   stdin carries the statement. Root cause in `src/commands/db.c:144`:
   `cmd_args_flag(ga, "sql_stdin", 0)` — for a boolean flag
   (`has_value = 0`) that function returns NULL by construction (it
   only returns the *next token* when `has_value` is 1), so
   `use_stdin` is always 0. The flag passes `cmd_args_validate`
   (known flag), so the failure masquerades as "you gave no source".
   One-line fix: `cmd_args_has_flag(ga, "sql_stdin")` — every other
   boolean flag in the codebase already uses that helper (grep: only
   this one call uses `cmd_args_flag(…, 0)`). Add a regression test
   (piped UPDATE, expect `{"status":"ok"}`).

2. **`--stream --count` is not rejected.** `model list --stream --count`
   → bare `10`, exit 0: `--count` silently wins and `--stream` is
   ignored. The spec and the conflict message both name `--count` as
   incompatible; only the `--table` / `--id_only` pairings are actually
   enforced. The stream conflict check must include `--count` (or the
   check is missing the flag it is supposed to catch).

3. **Help text lies about SELECT.** `db exec "SELECT 1"` (and
   `SELECT id, name FROM models LIMIT 2`) → `{"status":"ok"}`, exit 0:
   SELECT statements *are* executed; there is no guard in
   `src/commands/db.c`, only the prose "no SELECT / query support"
   (help + `--tools` description at line 114). Two ways out: add a
   real query-statement guard, or fix the text ("SELECT is accepted
   but returns no rows; use the list actions to query").

## Notes

- Unknown JSON keys are silently accepted (`--json '{"type":"t",
  "content":"c","bogus":1}' context create` → created, exit 0).
  Lenient is defensible, but strict-reject would give agents better
  feedback on key typos; at minimum document the leniency in
  `cli_spec.md`.
- `skill list` without `--folder_id` already returns all folders (8
  in the test), so `--all` is observably a no-op; either drop the flag
  or document that the default is already all-folders.
