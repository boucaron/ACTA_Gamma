# Known issues (open)

Saved list of currently **open** CLI issues, each verified live against
`acta_cli.exe` (2026-09-18) and automated in the `acta_cli/tests/` suites
(test names marked **KI-<n>**, see "Tests"). The predecessor of this file
(the old `acta_db` review, issues 1–3) was closed and deleted in commit
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
| KI-5 | Help / `--tools` say `db exec` is "no SELECT", but `db exec "SELECT 1;"` **succeeds** (rc 0, `{"status":"ok"}`). Either guard non-mutating statements in `db.c` or fix the help text. | open | **pin** | `acta_cli db exec "SELECT 1;"` → currently rc 0; test pins this until a decision is made. |
| KI-6 | `--id_only` **silently ignored** on `list` actions (prints full JSON rows). | open | **pin** | `acta_cli context list --id_only` → currently full JSON; test pins until reject-or-implement. |
| KI-7 | FK-violation error message is `execution create failed: (no detail)` — `sqlite3_errmsg` is not surfaced (`db.c:264` pattern `msg ? msg : "(no detail)"`; same in entity handlers). Note: the error JSON goes to **stderr** (`emit_cli_error`), which the unit harness does not capture, so the test can only assert the rc. | open | **rc-pinned** | `echo '{"prompt":"p","context_id":999999,...}' \| acta_cli exec create --stdin` → stderr message contains `FOREIGN KEY` (actual: `(no detail)`). |
| KI-8 | `skill list --all` is accepted as a **no-op** (default already lists all folders); identical output with or without the flag. | open | **pin** | `acta_cli skill list --all` → same rc + same output as `skill list`; test pins until documented or rejected. |

Fixed since the analysis docs recorded it (no open item): `--stream --count`
is now correctly rejected with rc 4 (`context list --stream --count`); the
`db exec` positional-after-boolean-flag regression is covered by
`test_exec_positional_after_bool_flag`.

## Tests

- `tests/db/db_test.c`: `test_exec_sql_stdin_flag` (KI-1, regression pin —
  now green), `test_exec_select_behavior_pinned` (KI-5, pin).
- `tests/context/context_test_misc.c`: `test_get_raw_out_null_field`
  (KI-3, regression — now green), `test_trailing_help_rejected` (KI-4,
  regression, routed via `stest_run_dispatch`),
  `test_create_unknown_json_key` (KI-2, regression — now green),
  `test_list_id_only_pinned` (KI-6, pin).
- `tests/exec/execution_test_create.c`: `test_create_fk_error_has_detail`
  (KI-7, rc-pinned; the message check needs stderr capture, which the
  harness lacks).
- KI-2 also flips two pre-existing json-layer tests
  (`tests/json/json_test_main.c`): the case-sensitivity and unknown-key
  cases now expect `-1` (struct left fully zeroed, per the json.h
  contract) instead of `0` with the keys ignored.
- KI-8 is already pinned by the pre-existing `test_list_all_flag`
  (`tests/skill/skill_test_list_count.c`): `--all` is asserted to be
  identical to the default listing.

Run: `make -C acta_cli test`. Currently red: **none** — KI-1…KI-4 are
fixed and pinned as regression tests; KI-5/6/7/8 are open *decisions*
behind passing pins.
