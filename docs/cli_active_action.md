# Active Actions — from `cli_review.md`

Action plan derived from [`cli_review.md`](cli_review.md), ordered by
priority (highest first). The earlier `db`-surface round closed the
in-scope items; what remains there (F2, F3) is folded into the plan below
(F3 → S2, F2 → S3). F1 landed as P4 (`ed142cc`).

## Top fixes (by severity)

| # | Action | Source | Severity | Notes / dependencies |
|---|--------|--------|----------|----------------------|

## Cheap wins

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|

## Structural (highest long-term payoff)

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| S1 | **Per-entity field descriptor + shared CRUD drivers** — a field table (name, JSON key, flag, type, required, root-vs-null semantics) plus shared `create`/`get`/`list`/`count`/`update`/`delete`/`restore`/`move` drivers (and a shared `get-latest`-class driver). The copy-paste family (~5.4k lines of mostly-CRUD across 8 files) is where most bugs live; this makes P3–P5 and the P1-class divergences impossible to re-introduce rather than fixing 8 files by hand. Also settle the `get-latest` null-semantics ambiguity: a NULL fetch conflates *parent row missing* with *parent exists but no revisions* — the shared driver (with library support, e.g. `get_latest` reporting parent-missing separately) should distinguish the two so the error message is precise | P4 #10 | Do after P3 or the plan is moot — structural fix supersedes the per-file edits for the patterns it covers |
| S2 | **Test the global parse layer** — raw `argv` through `parse_globals` + `commands_dispatch` (in-process or spawning the binary). Every bug in the P2 class (`--json` blob ignored, `--stdin` eaten, bare `--json` unparseable, dead local `--count`) lives in this untested seam (former F3) | P5 #3 | Orphaned `tests_parse_globals.c` is the seed; the `stest_run_argv` helper (`tests/helpers/test_helpers`) is already the in-process raw-argv path used by the P2 input-source regression cases — a dedicated suite is the missing piece |
| S3 | **Per-action stdout-schema table in the spec + one shared emit helper** — success shapes keep multiplying (`{"id":N}`, `{"id":N,"folder_id":M}`, `{"deleted":true}`, bare `N`, …); `db`'s shapes are already documented in `db_usage()` (former F2). Also settle the root-folder wire representation (`null` in `model_to_json` vs `0` in move success lines) | P3 #3 / P4 #8 | Cross-entity; db row is the template |
| S4 | Resolve the remaining parse-layer inconsistencies: `cmd_args_flag` doc/implementation mismatch (present-vs-absent indistinguishable); `parse_globals` conflates OOM / missing flag value / too-few positionals into one `EXIT_CLI` with the wrong main.c message; bare `--json` (no value) still unparseable after the input-source fix — parser still requires a value (help text no longer documents it) | P1 #1–2 | Small, but the flag API is what W1 and the dead flags keep tripping on |

## Summary

- **Next:** S1 (per-entity field descriptor + shared CRUD drivers).
- **Finally:** S1–S4 structural work; S1 should land before any of P3/W1
  patterns are re-touched per-file (or those per-file fixes become throwaway).
- Done so far: P3 `get` not-found → `EXIT_NOT_FOUND` + JSON error (`e6c86fe`),
  plus `get-latest` follow-up (`f7d2ef5`), W1 dead local `--count` reads in
  all `list` actions removed + test ga-injections reworked to `g.count = 1`
  (`99d17c1`),
  W6 shared test helpers moved to `tests/helpers/test_helpers.*` (`696d4df`),
  P1 `skill update` data loss (`928c66f`), P2 input-source
  resolution (`e58d956`), P4 entity DB failures emit the stderr JSON error
  line (`ed142cc`), W2 empty-name checks in `model update` /
  `model_folder rename` (`341de4b`), W3 `exec create` rejects dead
  `--status` (`cffe8fe`), W4 removed `json_serialize_*` stubs +
  `json_print_table` (`2e34bbc`), W5 stale `json.h` comment
  (`9f19a6a`, done during the P2 round), W8 `$(EXEEXT)` on all CLI
  executable targets + root `.gitignore` `*.o`/`*.exe`/`*.a`
  (`5959ea1`), W7 minimal scope: `../acta_db/libacta_db.a` rebuild rule as
  a prerequisite of all link targets + `test` runs every suite and reports
  (`787b518`; auto-derived test targets and `LDFLAGS`→`LDLIBS` skipped as
  POC hygiene); round 3's db-surface scope is closed,
  F2/F3 are carried as S3 / S2.
