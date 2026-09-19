# Session handoff (2026-09-19)

**Resume prompt:**
> Verify the 2026-09-19 fixes by running the suites — `make test` at top level (at minimum `acta_cli` incl. gparse G9, `acta_runner` incl. the new sweep scenario 11, and `make gui` + a manual relaunch check for the GUI "Show trash" persistence) — since this session was static-review only (no compile, no test runs). If green, all 11 tracked known issues are done: retire or delete this handoff.

## Done — this session (2026-09-19, static review only)

| Item | Fix | Commits |
|---|---|---|
| KI #11 | `acta_gui/src/mainwindow.cpp`: `MainWindow` now saves the context/execution "Show trash" checkbox states to QSettings (`window/showDeletedContext`, `window/showDeletedExecution`) in `closeEvent` and restores them in the constructor, mirroring the skill/model panels (same no-op-on-unchanged `setChecked(true)` idiom); no new members/includes | `494e988` + `71cc7e7` (hash) |
| KI #10 | `acta_runner/src/sweep.c`: `last_activity()` counts the total first (`acta_db_execution_log_count`) and fetches only the LAST page (`offset = total - limit`, `limit = min(total, ACTA_DB_MAX_PAGE)`), so `rows[n-1]` is the overall newest row for any row count; count failure falls back to the existing `started_at`/`created_at` chain. Regression pinned: new scenario 11 in `acta_runner/tests/run/test_sweep.c` (> 10,000 log rows, newest row bumped to now beyond the first page → row kept; exact log-count check) | `c3d37b8` + `712cfca` (hash) |

Also verified statically (no changes): gparse G9 (KI #9, from `cebd993`) matches the `cmd_args_flag`/`cmd_args_validate` implementation — both G9 cases (`--name --deleted`, `--name` last token) reach `missing value for --name` → EXIT_CLI.

## Open / pending

| # | Item | Priority |
|---|---|---|
| — | **Verification pending:** nothing was compiled or run this session — full `make test` + `make gui` + GUI relaunch check outstanding for #10/#11 (and the first-ever run of G9) | next session, first |
| — | No open known issues: all 11 tracked items (#1–#11) are fixed and regression-pinned per `docs/known_issues.md` | — |

## Working state & notes

- Working tree clean; this handoff is committed as the last change of the session.
- House commit flow used for #11 and #10: fix commit (code + `known_issues.md` row marked `**fixed**` without hash + `status.md` "Known issues" paragraph synced) → follow-up commit annotating the row with the fix commit's hash.
- MSYS2 environment quirk: in some shells `make` does not import environment variables, so `$(OS)`/`EXEEXT` can be empty and platform detection in `acta_runner/Makefile` silently degrades; pass `OS=Windows_NT EXEEXT=.exe` explicitly when in doubt.
- Fix conventions: regressions pinned in the relevant test suite (`acta_cli/tests`, `acta_db/tests`, `acta_runner/tests`); doc rows keep a short `**fixed** (<sha>)` summary.

## Earlier context

- 2026-09-19 (previous session): KI #7 `c327a49`, #8 `c64b74c`, #9 `cebd993` (+ stale-test `1dbb5a7`, link-error `6a28dbf`), each with hash-annotation follow-ups.
- 2026-09-18: KI #1–#6 fixed and regression-pinned: `ce14a22` (#1), `d678a12` (#2), `4451244` (#3), `c437b3e` (#4), `1e376a7` (#5), #6 data fix (`6009d35`).
- Tracker: `docs/known_issues.md`.
