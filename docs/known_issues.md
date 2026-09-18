# Known issues

Saved list of CLI issues, each verified live against `acta_cli.exe`
(2026-09-18) and automated in the `acta_cli/tests/` suites (test names
marked **KI-<n>**, see "Tests"). All items KI-1…KI-8 are resolved; new
open issues will be appended here. The predecessor of this file (the old
`acta_db` review, issues 1–3) was closed and deleted in commit
`ce024ba`; it is recoverable via `git show a9627fc:docs/known_issues.md`.

Conventions:

- `rc` = exit code. JSON error payloads are on stdout; usage/help/warnings on stderr.
- Repro commands use a scratch copy of the DB: `acta_cli --db ./tmp/ki.db ...`.
- "Pin" tests lock the *current* (buggy) behavior so a future fix is
  caught if the behavior regresses or changes inconsistently; "fail" tests
  assert the *desired* behavior and are red until fixed.

| # | Issue | Status | Type | Repro (expected → actual) |
|---|---|---|---|---|
| KI-1 | `db exec --sql_stdin` **always fails** rc 4 ("no SQL source"). `db.c:144` tests the boolean flag with `cmd_args_flag(ga,"sql_stdin",0) != NULL`, which is NULL by construction. **Fixed**: `use_stdin = cmd_args_has_flag(ga, "sql_stdin")`; `test_exec_sql_stdin_flag` is now a regression pin. | **fixed** | regression | `echo "INSERT INTO contexts(type,content,content_hash) VALUES('t','c','h');" \| acta_cli db exec --sql_stdin` → rc 0, `{"status":"ok"}`. |
| KI-2 | **Unknown JSON keys silently accepted** on `create` — `{"type","content","bogus_key"}` created the row. **Fixed**: `jwalk` (single choke point of all seven `json_parse_*` parsers in `json.c`) now rejects any top-level key not in the entity's cli_spec.md field table → rc 4. Covers every create/update path and every input source (`--json`/`--stdin`/`--from_file`). | **fixed** | regression | `echo '{"type":"x","content":"c","bogus_key":1}' \| acta_cli context create --stdin` → rc 4. |
| KI-3 | `--raw_out` on a **NULL** field → rc 10 `unknown --raw_out field: 'metadata'` while listing `metadata` as supported; help says "null values produce no output". **Fixed**: `context.c`/`execution.c` track `known` separately from the NULL value (`known && v` / `known` / else unknown). | **fixed** | regression | `acta_cli context get <id-without-metadata> --raw_out metadata` → rc 0, empty output. |
| KI-4 | Trailing `help` form **silently executed** the action: `context list help` ran the list ("help" swallowed as an unconsumed positional). **Fixed**: `commands_dispatch` rejects any positional left unconsumed by a successful handler → exit 10 `unexpected argument` (covers all entities/actions, per the documented positional-rejection decision in `docs/cli_help_analysis.md`). | **fixed** | regression | `acta_cli context list help` → rc 10. |
| KI-5 | Help / `--tools` say `db exec` is "no SELECT", but `db exec "SELECT 1;"` **succeeds** (rc 0, `{"status":"ok"}`). **Fixed**: `db.c` rejects any statement whose first keyword is `SELECT` (skipping leading whitespace, stray `;`, and `--` / `/* */` comments) before executing → rc 4; the help text is now true by construction. **Follow-up**: the first fix had an off-by-one — the keyword check matched only `S,E,L,E,C` and tested `p[5]` as the terminator, so real `SELECT ...` statements (where `p[5]` is the `T`) slipped through and executed; the check now covers the full six-letter keyword with the terminator test on `p[6]`. | **fixed** | regression | `acta_cli db exec "SELECT 1;"` → rc 4, `db exec does not support SELECT / query statements`. |
| KI-6 | `--id_only` **silently ignored** on `list` actions (prints full JSON rows). **Fixed**: all nine `list` actions (context, exec, log, model, model_folder, model_revision, skill, skill_folder, skill_revision) reject `--id_only` → rc 4; it is a single-row modifier for `create` / `get`-style actions. | **fixed** | regression | `acta_cli context list --id_only` → rc 4, `context list: --id_only is not supported`. |
| KI-7 | FK-violation error message is `execution create failed: (no detail)` — `sqlite3_errmsg` is not surfaced (`db.c:264` pattern `msg ? msg : "(no detail)"`; same in entity handlers). Note: the error JSON goes to **stderr** (`emit_cli_error`), which the unit harness does not capture, so the test can only assert the rc. **Fixed**: `acta_db` exposes `acta_db_errmsg(db)` (the connection's `sqlite3_errmsg`); `finish_op_error` (the single choke point of every entity handler's failure path) and the `db exec` error path fall back to it when `acta_db_last_error` is NULL — entity mutators fail inside prepared statements and never store `last_error`, which is why the detail was missing (e.g. `execution create failed: FOREIGN KEY constraint failed: executions.context_id` for the SQL-constraint FKs). Additionally, `acta_db_execution_create`'s C-level context pre-check returns `ACTA_DB_ERR_FK` / `ACTA_DB_ERR_NOT_FOUND` without any SQL error ever occurring, so it now records the detail in `last_error` via `db_set_error` (`FOREIGN KEY violation: context <id> does not exist` / `context <id> is soft-deleted`). The test harness also gained stderr capture (`stest_stderr`, in `stest_run_argv` / `stest_run_dispatch`), so the message itself is pinned. **Follow-up (complete):** the remaining `(no detail)` class — every C-level state-check refusal (illegal exec transitions, `delete`-from-`running`, `reset`/`restore` row-class refusals, model/skill/folder NOT_FOUND paths, and the folder-move cycle guard and folder-delete guards) — now records its own reason via `db_set_error` and is pinned by `stest_stderr` message tests in `tests/exec` (lifecycle + deleted), `tests/context` (deleted), `tests/model` (error_contract), `tests/skill` (update + delete/restore), and `tests/model_folder` / `tests/skill_folder` (move + delete/restore); the exec state machine and the refusal exit-code split (INVALID → 4, NOT_FOUND → 1) are documented in `docs/cli_spec.md`. | **fixed** | regression | `echo '{"prompt":"p","context_id":999999,...}' \| acta_cli exec create --stdin` → stderr message contains `FOREIGN KEY` (was: `(no detail)`). |
| KI-8 | `skill list --all` is accepted as a **no-op** (default already lists all folders); identical output with or without the flag. **Resolved (documented, option A)**: the default scope is all folders, so `--all` alone is a documented no-op kept as the explicit form — the help text, the `--tools` descriptions and `cli_spec.md` now state that `--all` is the default scope (no-op alone; wins over `--folder_id` when combined). | **fixed** | regression | `acta_cli skill list --all` → same rc + same output as `skill list` (`test_list_all_flag`); `--all` wins over `--folder_id` (`test_list_folder_with_all_overrides`). |

Fixed since the analysis docs recorded it (no open item): `--stream --count`
is now correctly rejected with rc 4 (`context list --stream --count`); the
`db exec` positional-after-boolean-flag regression is covered by
`test_exec_positional_after_bool_flag`.

## Tests

- `tests/db/db_test.c`: `test_exec_sql_stdin_flag` (KI-1, regression pin —
  now green), `test_exec_select_behavior_pinned` (KI-5, regression pin —
  now green).
- `tests/context/context_test_misc.c`: `test_get_raw_out_null_field`
  (KI-3, regression — now green), `test_trailing_help_rejected` (KI-4,
  regression, routed via `stest_run_dispatch`),
  `test_create_unknown_json_key` (KI-2, regression — now green),
  `test_list_id_only_pinned` (KI-6, regression pin — now green).
- `tests/exec/execution_test_create.c`: `test_create_fk_error_has_detail`
  (KI-7, regression — now green; the harness captures stderr via
  `stest_stderr`, so the message is asserted to contain `FOREIGN KEY`
  and not `(no detail)`, alongside the rc).
- KI-7 follow-up, refusal-message pins (all green, `stest_stderr`-based):
  `tests/exec/execution_test_lifecycle.c` (`test_refusal_msgs` —
  start/cancel/complete/fail/reset wrong-state and missing-id messages),
  `tests/exec/execution_test_deleted.c` (`test_refusal_msgs` —
  delete-from-running, already-deleted, reset-deleted, restore-live),
  `tests/context/context_test_deleted.c` (`test_refusal_msgs` —
  delete deleted/nonexistent, restore live/nonexistent),
  `tests/model/model_test_error_contract.c` (full pinned stderr line incl.
  the refusal detail for delete/restore),
  `tests/skill/skill_test_update.c` (`test_update_nonexistent_msg`),
  `tests/skill/skill_test_delete_restore.c` (`test_refusal_msgs`),
  `tests/model_folder/model_folder_test_move.c` (`test_move_refusal_msgs` —
  missing folder, missing parent, cycle),
  `tests/model_folder/model_folder_test_delete_restore.c`
  (`test_refusal_msgs` — missing, sub-folder guard, assigned-models
  guard, restore missing),
  `tests/skill_folder/skill_folder_test_move.c`
  (`test_move_refusal_msgs`, `test_move_to_deleted_parent_msg`), and
  `tests/skill_folder/skill_folder_test_delete_restore.c`
  (`test_refusal_msgs`; `test_restore_not_deleted` flipped from the old
  no-op pin to the hard-NOT_FOUND refusal message).
- KI-2 also flips two pre-existing json-layer tests
  (`tests/json/json_test_main.c`): the case-sensitivity and unknown-key
  cases now expect `-1` (struct left fully zeroed, per the json.h
  contract) instead of `0` with the keys ignored.
- KI-8 (resolved by documentation, option A): pinned by
  `test_list_all_flag` and `test_list_folder_with_all_overrides`
  (`tests/skill/skill_test_list_count.c`) — `--all` is asserted identical
  to the default listing, and to win over `--folder_id` when both are
  given.

Run: `make -C acta_cli test`. Currently red: **none** — KI-1…KI-8 are
fixed and pinned as regression tests (KI-8 resolved by documentation:
`--all` is the all-folders default scope, kept as the explicit form).
