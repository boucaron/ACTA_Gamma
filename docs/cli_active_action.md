# Active Actions — from `cli_review.md`

Action plan derived from [`cli_review.md`](cli_review.md), ordered by
priority (highest first). The earlier `db`-surface round closed the
in-scope items; what remains there (F1–F3) is folded into the plan below
(F1 → P4, F3 → test-seam item; F2 → spec-table item).

## Top fixes (by severity)

| # | Action | Source | Severity | Notes / dependencies |
|---|--------|--------|----------|----------------------|
| ~~P1~~ | ~~**`skill update` data loss**~~ — done (`928c66f`): fetch-and-merge (live row → override only what the caller passed) in both flag and JSON modes, `EXIT_NOT_FOUND` from the live-row fetch, regression tests added. Residuals folded into P2/S3: `--json <blob>` input source still unread (P2), and the JSON-body `folder_id` cannot distinguish `0`/absent from root (wire representation — S3) | P4 #1 | High — silent data loss | Fix pattern already exists in `model.c`; flag and JSON modes both affected |
| ~~P2~~ | ~~**`--json <blob>` ignored**~~ — done (`e58d956`): `resolve_input_source()` in `commands.h` (`--json` blob → `--from_file` → `--stdin`; no source → flag mode; conflicting/empty/unreadable → canonical JSON error + `EXIT_INVALID`), routed through all 8 sites (7 create + `skill update`); help texts updated. Actual site count was 8, not 9. Per-suite input-source regression cases now done: raw argv through `parse_globals` + handler via the shared `stest_run_argv` helper (`tests/skill/skill_test_helpers`) in every JSON-input suite — create (context, model, model_folder, skill, skill_folder, exec, execution_log) + skill update — covering `--json <blob>` (space + `=` form), `--from_file` (present + missing), `--stdin`, and the three conflicting-source pairs (see `p2_input_source_review.md`). Remaining: bare `--json` still unparseable — parser unchanged by design, stays with P3 #4 | P2 #1–2 | High — wrong input source, hangs | P3 #4 root cause only partly resolved: help no longer documents a broken invocation, but the parser still requires a value |
| P3 | **Silent not-found** — all 8 entities' `get` return `EXIT_OK` with empty stdout when the row doesn't exist. Return `EXIT_NOT_FOUND` (1) + JSON error line via `finish_db_error` | P4 #2 / P3 #1 | High — contract violation | Helper already in `cli_util.h`; `list` → `[]` is the existing correct pattern for empty |
| P4 | **DB failures produce no stderr** — every `if (rc != ACTA_DB_OK)` path outside `db.c` prints nothing but a non-zero exit. Route them through `finish_db_error(rc, what)` (former F1) | P4 #3 (F1) | High — broken error contract | `db.c` is the reference implementation; per-entity edits |

## Cheap wins

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| W1 | Remove the dead local `--count` flag in all 8 `list` actions (`parse_globals` already consumed it into `gopts->count`) | P4 #4 / P3 #2 | Dead branch misleads readers; `gopts->count` already used |
| W2 | Add empty-string checks to `update`/`rename` paths (`model create` rejects `name == ""`; `model update --name ""`, `model_folder rename 7 --name ""` accept it) | P4 #5 | Apply the same check create has |
| W3 | Reject or mark `(ignored)` the dead `--status` flag on `exec create` — the lib forces `pending`, so `--status completed` looks like it worked and didn't | P5 #1 | Lib already handles it correctly (Part 2 #9 resolved); CLI just shouldn't advertise the flag |
| W4 | Remove unimplemented stubs: `json_serialize_*` (4 + 1 array) and `json_print_table` — the header advertises an API that always fails / no-ops; real output is hand-emitted per command (`tcol`) | P2 #3–4 | Or implement the serializers to share one emit path — decide first |
| W5 | Fix stale `json.h` header comment ("stubs … placeholders" — the file *is* the cJSON implementation now) | P2 #12 | One-line comment fix |
| W6 | Move the shared test helpers out of `tests/skill/` into `tests/helpers/` (or `tests/common/`) so ownership is visible; every suite currently links `tests/skill/skill_test_helpers.o` and carries a `-Itests/skill` path | P5 #7 | Minor: skill target gets the helper via its own wildcard instead of `HELPERS_OBJ` |
| W7 | Makefile hygiene: auto-derive test targets from `$(wildcard tests/*)`; add the missing `../acta_db/libacta_db.a` dependency (lib consumed with no rebuild rule); move the lib from `LDFLAGS` to `LDLIBS`; make `test` run all suites and report (`for … || fail=1` or `make -k`) instead of stopping at the first failure | P5 #4, #5, #8 | `LDFLAGS` ordering only works today by accident |
| W8 | POSIX/Windows mismatch: targets have no `$(EXEEXT)` but the tree carries `.o`/`.exe` artifacts; document "POSIX only" or add `$(EXEEXT)`. Extend root `.gitignore` with `*.o`, `*.exe`, `*.a` (covers `*.db` only today) | P5 #6 | Tree hygiene, not a commit bug |

## Structural (highest long-term payoff)

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| S1 | **Per-entity field descriptor + shared CRUD drivers** — a field table (name, JSON key, flag, type, required, root-vs-null semantics) plus shared `create`/`get`/`list`/`count`/`update`/`delete`/`restore`/`move` drivers. The copy-paste family (~5.4k lines of mostly-CRUD across 8 files) is where most bugs live; this makes P3–P5 and the P1-class divergences impossible to re-introduce rather than fixing 8 files by hand | P4 #10 | Do after P1–P4 or the plan is moot — structural fix supersedes the per-file edits for the patterns it covers |
| S2 | **Test the global parse layer** — raw `argv` through `parse_globals` + `commands_dispatch` (in-process or spawning the binary). Every bug in the P2 class (`--json` blob ignored, `--stdin` eaten, bare `--json` unparseable, dead local `--count`) lives in this untested seam (former F3) | P5 #3 | Orphaned `tests_parse_globals.c` is the seed; the `stest_run_argv` helper (`tests/skill/skill_test_helpers`) is already the in-process raw-argv path used by the P2 input-source regression cases — a dedicated suite is the missing piece |
| S3 | **Per-action stdout-schema table in the spec + one shared emit helper** — success shapes keep multiplying (`{"id":N}`, `{"id":N,"folder_id":M}`, `{"deleted":true}`, bare `N`, …); `db`'s shapes are already documented in `db_usage()` (former F2). Also settle the root-folder wire representation (`null` in `model_to_json` vs `0` in move success lines) | P3 #3 / P4 #8 | Cross-entity; db row is the template |
| S4 | Resolve the remaining parse-layer inconsistencies: `cmd_args_flag` doc/implementation mismatch (present-vs-absent indistinguishable); `parse_globals` conflates OOM / missing flag value / too-few positionals into one `EXIT_CLI` with the wrong main.c message | P1 #1–2 | Small, but the flag API is what W1 and the dead flags keep tripping on |

## Summary

- **Do first:** P1 (`skill update` data loss) — ~~done~~ (`928c66f`).
- **Then:** P3–P4 (silent not-found, error contract) —
  all have ready helpers (`finish_db_error`, `model.c` merge pattern). P2 done (`e58d956`).
- **Then:** cheap wins W1–W8, mostly mechanical.
- **Finally:** S1–S4 structural work; S1 should land before any of P3/P4/W1
  patterns are re-touched per-file (or those per-file fixes become throwaway).
- Round 3's db-surface scope is closed; F1–F3 are carried as P4 / S2 / S3.
