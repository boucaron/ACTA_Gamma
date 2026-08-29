# Active Actions — `db` command (`acta db`)

Action plan derived from [`cli_review.md`](cli_review.md), covering all issues
that touch the `db` command (`acta db version` / `acta db exec`, i.e. `db.c`),
ordered by priority (highest first).

| # | Action | Issue | Source | Severity | Effort | Notes / dependencies |
|---|--------|-------|--------|----------|--------|----------------------|
| 1 | Switch `db exec` / unknown-action errors to the single-line JSON error contract (`{"error":...}` on stderr) instead of human text | Error output contract violation — scripts parsing stderr JSON choke | P3 #6 | Medium-High — breaks the CLI's stdout/stderr JSON contract | S | Should be part of the centralized `finish_db_error(rc, what)` helper from P4 #6 (one fix covers db.c + 8 entity files) |
| 2 | Document `db` action success shapes (`db exec` → `{"status":"ok"}`, `db version` → `{"version":"..."}`) in the per-action stdout-schema table | Success shapes inconsistent/undocumented across entities | P3 #7 (shared with P4 #11) | Low — spec/doc gap | S | Same spec table serves all entities; db is just one row |

**Done:**

| Item | Action | Source | Status |
|------|--------|--------|--------|
| — | `db version` prints literal `"test"` → call `sqlite3_libversion()` | former P3 #1, Summary #4 | ✅ fixed (db.c already uses `sqlite3_libversion()`) |
| #4 | Reject empty SQL (`--sql ""`, 0-byte `--file`, empty `--stdin`) with a clear error before the lib call | former #4, P3 nitpick | ✅ fixed (`acta_db_cli/src/commands/db.c`; tests: `test_exec_empty_sql_flag`, `test_exec_empty_file`) |
| #2 | Fix dead `db exec --stdin` flag: entity flag renamed to `--sql_stdin`; global `--stdin` (`gopts->from_stdin`) now explicitly rejected with a pointer to `--sql_stdin` | former #2, P3 #2 / P2 #2 | ✅ fixed (`acta_db_cli/src/commands/db.c`; test: `test_exec_global_stdin_rejected`) |
| #3 | Verify lib accepts trailing `;` for single-statement exec — verified against `sqlite3_exec` (accepts `;` + trailing whitespace); help example already correct, no change needed | former #3, P3 nitpick | ✅ verified; locked in (`test_exec_trailing_semicolon`) |
| #1 | Implement the documented positional form `acta db exec "INSERT ..."` — resolver resolves positional via `cmd_args_next_positional`; precedence positional > `--sql` > `--file` > `--sql_stdin` (help order) | former #1, P3 #1, Summary #4 | ✅ fixed (`acta_db_cli/src/commands/db.c`; tests: `test_exec_positional_real`, `test_exec_positional_after_bool_flag`) |
| #4 | Fix `cmd_args_next_positional` boolean-flag misparse — name→has_value table (`entity_flag_specs[]`) instead of the unconditional next-token heuristic; unknown flags keep legacy value-taking; bonus: `--name=value` inline form no longer swallows the next token | former #4, P1 #1 | ✅ fixed (`acta_db_cli/src/argparse.c`; no call-site changes) |

## Summary

- **Quick wins (1–2 h):** #2
- **Do as part of a systemic fix:** #1 via the P4 #6 centralized error
  emitter; #2 via the shared spec table (P3 #7 / P4 #11)

None of the db-command issues involve data loss or memory leaks — the db
command's problems are all about behavior/documentation mismatches
(version placeholder, dead `--stdin`, missing positional form) and error
contract violations.
