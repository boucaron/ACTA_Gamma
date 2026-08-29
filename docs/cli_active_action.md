# Active Actions — `db` surface (`acta db`)

Action plan derived from [`cli_review.md`](cli_review.md), covering all issues
that touch the `db` surface: the `db` command (`acta db version` /
`acta db exec`, i.e. `db.c`) and the global layer where `acta db …`
errors surface (`main.c`, `argparse.c`, `cli_util.h`), ordered by
priority (highest first).

**Round 3** — the `db` surface is closed. What remains is the systemic
follow-ups F1–F3 only.

| # | Action | Issue | Source | Severity | Effort | Notes / dependencies |
|---|--------|-------|--------|----------|--------|----------------------|
| — | *(none — in-scope items for round 3)* | | | | | |

**Systemic follow-ups (db is the reference implementation; not db-closed):**

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| F1 | Adopt `finish_db_error(rc, what)` in the 8 entity files' silent lib-failure paths (`VLOG + return map_rc_to_exit(rc)` with nothing on stderr) | P4 #3 | Helper built and proven in db.c; per-entity plans own the edits |
| F2 | Per-action stdout-schema table in the spec + one shared emit helper; `db` row is already documented in `db_usage()` help | P3 #3 / P4 #8 | Cross-entity; db shapes: `db exec` → `{"status":"ok"}`, `db version` → `{"version":"..."}` |
| F3 | Test the global parse layer: raw `argv` through `parse_globals` + `commands_dispatch` (in-process or spawning the binary) | P5 #3 | Every db argv-layer bug class (global `--stdin` vs `--sql_stdin` shadowing, dead `--from_file`) lives in this untested seam; orphaned `tests_parse_globals.c` is the seed |

## Summary

- **In scope:** none — only F1–F3 remain, all cross-entity/systemic
  (spec, per-entity plans, test seam)
