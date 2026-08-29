# Active Actions — from `cli_review.md`

Action plan derived from [`cli_review.md`](cli_review.md), ordered by
priority (highest first). The earlier `db`-surface round closed the
in-scope items; what remains there (F1–F3) is folded into the plan below
(F1 → P4, F3 → test-seam item; F2 → spec-table item).

## Top fixes (by severity)

| # | Action | Source | Severity | Notes / dependencies |
|---|--------|--------|----------|----------------------|
| P3 | **Silent not-found** — all 8 entities' `get` return `EXIT_OK` with empty stdout when the row doesn't exist. Return `EXIT_NOT_FOUND` (1) + JSON error line via `finish_db_error` | P4 #2 / P3 #1 | High — contract violation | Helper already in `cli_util.h`; `list` → `[]` is the existing correct pattern for empty |
| P4 | **DB failures produce no stderr** — every `if (rc != ACTA_DB_OK)` path outside `db.c` prints nothing but a non-zero exit. Route them through `finish_db_error(rc, what)` (former F1) | P4 #3 (F1) | High — broken error contract | `db.c` is the reference implementation; per-entity edits |

## Cheap wins

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| W1 | Remove the dead local `--count` flag in all 8 `list` actions (`parse_globals` already consumed it into `gopts->count`) | P4 #4 / P3 #2 | Dead branch misleads readers; `gopts->count` already used |
| W4 | Remove unimplemented stubs: `json_serialize_*` (4 + 1 array) and `json_print_table` — the header advertises an API that always fails / no-ops; real output is hand-emitted per command (`tcol`) | P2 #3–4 | Or implement the serializers to share one emit path — decide first |
| W6 | Move the shared test helpers out of `tests/skill/` into `tests/helpers/` (or `tests/common/`) so ownership is visible; every suite currently links `tests/skill/skill_test_helpers.o` and carries a `-Itests/skill` path | P5 #7 | Minor: skill target gets the helper via its own wildcard instead of `HELPERS_OBJ` |
| W7 | Makefile hygiene: auto-derive test targets from `$(wildcard tests/*)`; add the missing `../acta_db/libacta_db.a` dependency (lib consumed with no rebuild rule); move the lib from `LDFLAGS` to `LDLIBS`; make `test` run all suites and report (`for … || fail=1` or `make -k`) instead of stopping at the first failure | P5 #4, #5, #8 | `LDFLAGS` ordering only works today by accident |
| W8 | POSIX/Windows mismatch: targets have no `$(EXEEXT)` but the tree carries `.o`/`.exe` artifacts; document "POSIX only" or add `$(EXEEXT)`. Extend root `.gitignore` with `*.o`, `*.exe`, `*.a` (covers `*.db` only today) | P5 #6 | Tree hygiene, not a commit bug |

## Structural (highest long-term payoff)

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| S1 | **Per-entity field descriptor + shared CRUD drivers** — a field table (name, JSON key, flag, type, required, root-vs-null semantics) plus shared `create`/`get`/`list`/`count`/`update`/`delete`/`restore`/`move` drivers. The copy-paste family (~5.4k lines of mostly-CRUD across 8 files) is where most bugs live; this makes P3–P5 and the P1-class divergences impossible to re-introduce rather than fixing 8 files by hand | P4 #10 | Do after P1–P4 or the plan is moot — structural fix supersedes the per-file edits for the patterns it covers |
| S2 | **Test the global parse layer** — raw `argv` through `parse_globals` + `commands_dispatch` (in-process or spawning the binary). Every bug in the P2 class (`--json` blob ignored, `--stdin` eaten, bare `--json` unparseable, dead local `--count`) lives in this untested seam (former F3) | P5 #3 | Orphaned `tests_parse_globals.c` is the seed; the `stest_run_argv` helper (`tests/skill/skill_test_helpers`) is already the in-process raw-argv path used by the P2 input-source regression cases — a dedicated suite is the missing piece |
| S3 | **Per-action stdout-schema table in the spec + one shared emit helper** — success shapes keep multiplying (`{"id":N}`, `{"id":N,"folder_id":M}`, `{"deleted":true}`, bare `N`, …); `db`'s shapes are already documented in `db_usage()` (former F2). Also settle the root-folder wire representation (`null` in `model_to_json` vs `0` in move success lines) | P3 #3 / P4 #8 | Cross-entity; db row is the template |
| S4 | Resolve the remaining parse-layer inconsistencies: `cmd_args_flag` doc/implementation mismatch (present-vs-absent indistinguishable); `parse_globals` conflates OOM / missing flag value / too-few positionals into one `EXIT_CLI` with the wrong main.c message; bare `--json` (no value) still unparseable after the input-source fix — parser still requires a value (help text no longer documents it) | P1 #1–2 | Small, but the flag API is what W1 and the dead flags keep tripping on |

## Summary

- **Do first:** P3–P4 (silent not-found, error contract) — all have ready
  helpers (`finish_db_error`).
- **Then:** cheap wins W1, W4, W6–W8, mostly mechanical.
- **Finally:** S1–S4 structural work; S1 should land before any of P3/P4/W1
  patterns are re-touched per-file (or those per-file fixes become throwaway).
- Done so far: P1 `skill update` data loss (`928c66f`), P2 input-source
  resolution (`e58d956`), W2 empty-name checks in `model update` /
  `model_folder rename` (`341de4b`), W3 `exec create` rejects dead
  `--status` (`cffe8fe`), W5 stale `json.h` comment
  (`9f19a6a`, done during the P2 round); round 3's db-surface scope is closed,
  F1–F3 are carried as P4 / S2 / S3.
