# Active Actions — `db` command (`acta db`)

Action plan derived from [`cli_review.md`](cli_review.md), covering all issues
that touch the `db` command (`acta db version` / `acta db exec`, i.e. `db.c`),
ordered by priority (highest first).

*No open actions — both items below are resolved.*

**Done:**

| Item | Action | Source | Status |
|------|--------|--------|--------|
| #1 | Switch `db exec` / unknown-action errors to the single-line JSON error contract (`{"error":"ACTA_DB_ERR_*","code":<rc>,"message":"..."}` on stderr) — new centralized `finish_db_error(rc, what)` helper in `cli_util.h` (P4 #6), used by all 9 `db exec` error paths; unknown-action emits the JSON line first, then the shared human suggestion block | P3 #6 | ✅ fixed (`acta_db_cli/include/cli_util.h`, `acta_db_cli/src/commands/db.c`); the other 8 entity files adopt the same helper via their own action plans |
| #2 | Document `db` action success shapes (`db exec` → `{"status":"ok"}`, `db version` → `{"version":"<version>"}`, plus the JSON error line) in `db_usage()` help — the cross-entity per-action stdout-schema table (P3 #7 / P4 #11) is shared spec work, out of db scope | P3 #7 (shared with P4 #11) | ✅ fixed (`acta_db_cli/src/commands/db.c`, `db_usage`) |
| #4 | Reject empty SQL (`--sql ""`, 0-byte `--file`, empty `--stdin`) with a clear error before the lib call | former #4, P3 nitpick | ✅ fixed (`acta_db_cli/src/commands/db.c`; tests: `test_exec_empty_sql_flag`, `test_exec_empty_file`) |
| #2 | Fix dead `db exec --stdin` flag: entity flag renamed to `--sql_stdin`; global `--stdin` (`gopts->from_stdin`) now explicitly rejected with a pointer to `--sql_stdin` | former #2, P3 #2 / P2 #2 | ✅ fixed (`acta_db_cli/src/commands/db.c`; test: `test_exec_global_stdin_rejected`) |
| #3 | Verify lib accepts trailing `;` for single-statement exec — verified against `sqlite3_exec` (accepts `;` + trailing whitespace); help example already correct, no change needed | former #3, P3 nitpick | ✅ verified; locked in (`test_exec_trailing_semicolon`) |
| #1 | Implement the documented positional form `acta db exec "INSERT ..."` — resolver resolves positional via `cmd_args_next_positional`; precedence positional > `--sql` > `--file` > `--sql_stdin` (help order) | former #1, P3 #1, Summary #4 | ✅ fixed (`acta_db_cli/src/commands/db.c`; tests: `test_exec_positional_real`, `test_exec_positional_after_bool_flag`) |
| #4 | Fix `cmd_args_next_positional` boolean-flag misparse — name→has_value table (`entity_flag_specs[]`) instead of the unconditional next-token heuristic; unknown flags keep legacy value-taking; bonus: `--name=value` inline form no longer swallows the next token | former #4, P1 #1 | ✅ fixed (`acta_db_cli/src/argparse.c`; no call-site changes) |

## Summary

- All actions done. #1 lands the db half of the P4 #6 centralized
  `finish_db_error` emitter; #2 documents the db success shapes in the
  `db` help text (the shared spec table remains P3 #7 / P4 #11).

None of the db-command issues involve data loss or memory leaks — the db
command's problems are all about behavior/documentation mismatches
(version placeholder, dead `--stdin`, missing positional form) and error
contract violations.
