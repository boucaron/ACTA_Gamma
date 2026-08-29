# Active Actions — `db` surface (`acta db`)

Action plan derived from [`cli_review.md`](cli_review.md), covering all issues
that touch the `db` surface: the `db` command (`acta db version` /
`acta db exec`, i.e. `db.c`) and the global layer where `acta db …`
errors surface (`main.c`, `argparse.c`, `cli_util.h`), ordered by
priority (highest first).

**Round 2** — the `db.c` handler is contract-clean (round 1: positional
form, `--sql_stdin`, empty-SQL, trailing `;`, `sqlite3_libversion()`,
JSON error contract via `finish_db_error`, success shapes in help).
What remains is the global error layer plus db-related follow-ups.

| # | Action | Issue | Source | Severity | Effort | Notes / dependencies |
|---|--------|-------|--------|----------|--------|----------------------|
| 1 | ~~Escape `cli_error` messages in `main.c` with `json_str` (`cli_util.h`)~~ — ✅ done (f422717) | P1 #2 — `cli_error` emits unescaped JSON; a `--db` path containing `"`/`\` breaks the single-line JSON contract — the first error a `db` user hits, same class as what was fixed in db.c | P1 #2 | Medium-High — error-contract violation at the db open/parse layer | S | Header dependency direction already OK (`main.c` includes `cli_util.h`) |
| 2 | ~~Fix `cli_error` error-code plumbing: stop ignoring `exit_code`, unify string codes (`"ACTA_CLI_ERR"`) with numeric `c_code` incl. raw lib rc in `ACTA_DB_OPEN_FAIL`; enforce the §7.1 schema once~~ — ✅ done (f422717) | P1 #4 — dead/inconsistent error-code plumbing; the db-open failure path is one of the producers | P1 #4 | Medium — exit codes / `code` fields misleading for db-open failures | M | Do together with #1 (same function); spec §7.1 is the schema |
| 3 | ~~Replace hard-coded `"libacta_db 0.1.0, sqlite 3.x.x"` in `version_print` with `ACTA_DB_CLI_VERSION` + `sqlite3_libversion()`~~ — ✅ done (47e3a63) | P1 nitpick — `actagamma_db version` fakes the sqlite version while `db version` reports the real one | P1 nitpick | Low | S | `main.c`; sqlite already linked |
| 4 | Declare `db_usage` in `commands.h` (and wire `actagamma_db db --help`) or make it static | P4 #12 — `db_usage` is non-static ("the dispatch layer can call this") but undeclared, so the dispatch layer cannot call it | P4 #12 | Low — dead intent | S | Closes the db file; the other 9 entities carry the same issue (out of scope) |
| 5 | Fix `map_rc_to_exit` `default:` mapping unknown rc to `EXIT_SQL` (explicit generic code or complete the rc set) | P1 nitpick — IO-style/unknown rc misreported as SQL; `db exec` is the main consumer | P1 nitpick | Low | S | `cli_util.h`; check the rc set in `acta_db/include/db.h` first |

**Systemic follow-ups (db is the reference implementation; not db-closed):**

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| F1 | Adopt `finish_db_error(rc, what)` in the 8 entity files' silent lib-failure paths (`VLOG + return map_rc_to_exit(rc)` with nothing on stderr) | P4 #6 | Helper built and proven in db.c (round 1); per-entity plans own the edits |
| F2 | Per-action stdout-schema table in the spec + one shared emit helper; `db` row is already documented in `db_usage()` help | P3 #7 / P4 #11 | Cross-entity; db shapes: `db exec` → `{"status":"ok"}`, `db version` → `{"version":"..."}` |
| F3 | Test the global parse layer: raw `argv` through `parse_globals` + `commands_dispatch` (in-process or spawning the binary) | P5 #4 | Every db argv-layer bug class (global `--stdin` vs `--sql_stdin` shadowing, dead `--from_file`) lives in this untested seam; orphaned `tests_parse_globals.c` is the seed |

## Summary

- **Quick wins (1–2 h):** #1, #3, #4, #5
- **Paired fix:** #1 + #2 together (same `cli_error` function, same
  JSON/exit-code contract)
- **Defer / own elsewhere:** F1–F3 are systemic; db-side halves are done
