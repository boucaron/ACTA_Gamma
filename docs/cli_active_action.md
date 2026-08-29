# Active Actions — `db` command (`acta db`)

Action plan derived from [`cli_review.md`](cli_review.md), covering all issues
that touch the `db` command (`acta db version` / `acta db exec`, i.e. `db.c`),
ordered by priority (highest first).

| # | Action | Issue | Source | Severity | Effort | Notes / dependencies |
|---|--------|-------|--------|----------|--------|----------------------|
| 1 | Implement the documented positional form `acta db exec "INSERT ..."` — resolver only checks `--sql`/`--file`/`--stdin`, never calls `cmd_args_next_positional` | Documented form fails with "no SQL source provided" | P3 #1, Summary #4 | High — documented API broken | M | Fix P1 #1 first (boolean-flag heuristic eats positionals) or the new positional path inherits the bug |
| 2 | Switch `db exec` / unknown-action errors to the single-line JSON error contract (`{"error":...}` on stderr) instead of human text | Error output contract violation — scripts parsing stderr JSON choke | P3 #6 | Medium-High — breaks the CLI's stdout/stderr JSON contract | S | Should be part of the centralized `finish_db_error(rc, what)` helper from P4 #6 (one fix covers db.c + 8 entity files) |
| 3 | Verify lib accepts trailing `;` for single-statement exec; fix `db exec` help example (`VALUES ('Ada');`) if not | First-run users will hit the example as written | P3 nitpick | Low-Medium — doc/example correctness | S | Test: `acta db exec "INSERT INTO t VALUES (1);"` |
| 4 | Document `db` action success shapes (`db exec` → `{"status":"ok"}`, `db version` → `{"version":"..."}`) in the per-action stdout-schema table | Success shapes inconsistent/undocumented across entities | P3 #7 (shared with P4 #11) | Low — spec/doc gap | S | Same spec table serves all entities; db is just one row |
| 5 | (Dep) Fix `cmd_args_next_positional` boolean-flag misparse before item 1 | Parser heuristic treats next non-flag token as a value unconditionally — `--draft --title X`-style sequences eat positionals | P1 #1 | Enabler for #1 | M | Name→has_value table instead of positional heuristic |

**Done:**

| Item | Action | Source | Status |
|------|--------|--------|--------|
| — | `db version` prints literal `"test"` → call `sqlite3_libversion()` | former P3 #1, Summary #4 | ✅ fixed (db.c already uses `sqlite3_libversion()`) |
| #4 | Reject empty SQL (`--sql ""`, 0-byte `--file`, empty `--stdin`) with a clear error before the lib call | former #4, P3 nitpick | ✅ fixed (`acta_db_cli/src/commands/db.c`; tests: `test_exec_empty_sql_flag`, `test_exec_empty_file`) |
| #2 | Fix dead `db exec --stdin` flag: entity flag renamed to `--sql_stdin`; global `--stdin` (`gopts->from_stdin`) now explicitly rejected with a pointer to `--sql_stdin` | former #2, P3 #2 / P2 #2 | ✅ fixed (`acta_db_cli/src/commands/db.c`; test: `test_exec_global_stdin_rejected`) |

## Summary

- **Quick wins (1–2 h):** #3, #4
- **Wiring work (needs care):** #1 (positional SQL) — must land #5 first
- **Do as part of a systemic fix:** #2 via the P4 #6 centralized error
  emitter; #4 via the shared spec table (P3 #7 / P4 #11)

None of the db-command issues involve data loss or memory leaks — the db
command's problems are all about behavior/documentation mismatches
(version placeholder, dead `--stdin`, missing positional form) and error
contract violations.
