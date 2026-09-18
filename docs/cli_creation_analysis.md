# acta_cli — creation-path analysis

Exploration-only review of the `create` actions (and the error paths around
them), verified against the running binary on a test DB
(`acta_cli/tmp/acta.db`, copy of `acta_test_ref.db`). Companion to
`cli_spec.md` (T1 stdout contract) and `cli_download_analysis.md`
(read path). No source changes; findings only. Checked 2025-07-25.

## Verified working

- `context create` via flags and via `--stdin` JSON both emit `{"id":N}`.
- Default hash rule byte-exact: omitting `--hash` gives
  `content_hash = sha256(content)` lowercase hex (checked against Python
  `hashlib` for `context get 8`).
- Nested folder creation: `model_folder create --parent_id 9` on a
  folder just created works; same for `skill_folder`.
- `model create` with `--folder_id` / `--base_url`, `skill create` via
  `--json`, `exec create` with all revision ids → `{"id":N}`, row
  created `pending` with `parent_execution_id: 0`.
- `log create --execution_id <valid>` → `{"id":N}`, confirmed by
  `log list <execution_id>` (e.g. id 4 under execution 6).
- Missing required flag → exit 4 `ACTA_DB_ERR_INVALID`
  ("missing required field: content") + JSON line on stderr.
- FK violations (`--context_id 999`, `--parent_id 999`) → exit 4
  `ACTA_DB_ERR_FK`.
- T2 invariant holds on every path: `code` = −exit.

## Findings / broken (priority order)

1. **Entity-create error paths drop the SQL detail ("(no detail)") —
   and it hides real causes.** FK violations on `exec create` /
   `model_folder create` / `log create` print
   `<entity> create failed: (no detail)`. The `db exec` path proves the
   detail *is* available: the same FK violation via
   `db exec "INSERT INTO execution_logs ..."` prints
   `"SQL execution failed: FOREIGN KEY constraint failed"`. (The
   initial hypothesis that `log create` was broken in the binary was
   wrong: every failing probe used a nonexistent `execution_id` —
   plain FK violations masked by the empty message.) `sqlite3_errmsg()`
   should be propagated into the stderr JSON `message` on the
   entity-create paths, matching the `db exec` behavior.

2. **Same FK violation, two different exit codes/messages depending on
   path.** Via entity create: exit 4 `ACTA_DB_ERR_FK` + "(no detail)".
   Via `db exec`: exit 2 `ACTA_DB_ERR_SQL` + the full SQLite message.
   Pick one canonical classification (the spec's exit table lists FK
   under exit 4, so the `db exec` mapping is the odd one out) and make
   both paths carry the SQLite detail.

3. **Full entity help dump on usage errors.** A missing required flag
   prints the JSON error line *plus the entire ~100-line entity help*
   on stderr. Fine for humans, noisy for agents. Proposed: print only
   the failing action's section — the per-action section functions
   already exist from P0 (`cli_help_plan.md`) and could be reused in
   the `emit_error` path.

4. **No uniqueness on model identity.** `model create` with an identical
   `(name, backend, model_identifier)` in a different folder succeeds
   (new id, exit 0). Probably intentional (revisions track versions),
   but `cli_spec.md` does not state it; document the expected behavior
   so agents don't assume dedup.

5. **Silent positional-argument swallowing (also visible on create-adjacent
   list commands).** `model list xyz --count` → `8`, exit 0;
   `model_folder list 5 --count` → `8`; `context list 5 --count` → `7`;
   `skill_folder list 1 2` → filters on `1`, ignores `2`. Garbage
   positionals are silently ignored → silently wrong data, exit 0.
   Proposed: argparse rejects unexpected positionals → exit 10
   `ACTA_CLI_ERR` ("unexpected positional"), with tests; document in
   `cli_spec.md`.

   *Resolved* (cf03616): `commands_dispatch` rejects any positional left
   unconsumed by a successful handler with exit 10 `unexpected
   argument: '<tok>'` (all entities/actions). Pinned by
   `test_trailing_help_rejected` via `stest_run_dispatch`; see
   `docs/known_issues.md` KI-4; documented in `cli_spec.md` (exit-code
   paragraph).

6. **`--tools` advertises aliases the binary rejects.** The compact/JSON
   schema lists `aliases:execution` / `aliases:execution_log` for every
   exec/log action, but `execution list` / `execution_log count 1` →
   exit 10 "unknown entity". Either implement the alias in `main.c`
   routing (with tests) or drop it from `tool_table`; add an aliases
   note to `cli_spec.md`.

7. **File-input read-failure messages omit the path (minor).**
   `--content_file` / `--raw_file` / `--result_file` on a missing file →
   exit 4 with a message without the path (`"cannot read content
   file"`), while `--from_file` includes it (`"cannot read file
   '<path>'"`). An agent cannot tell which file failed; add the path
   to the three raw-file messages.

## File-input verification (2025-07-25)

The P3/P4 file inputs were exercised on the test DB; everything works
as documented (T2 invariant holds, messages descriptive):

- `--from_file` JSON input works; malformed file → exit 4
  `"invalid JSON body"`; missing file → exit 4 with the path.
- All mutual exclusions → exit 4 naming the conflicting flags
  (`--json`/`--stdin`/`--from_file` pair; `--content`+`--content_file`;
  `--json`+`--content_file`; `--raw`+`--raw_file`; `--result`+
  `--result_file`).
- `--content_file` stores content byte-exact, no JSON escaping
  (quotes/newlines/braces preserved); a 266 KB file (≈4× the 64 KiB
  `db exec --file` cap) works → the raw-file path is genuinely
  uncapped.
- `--raw_file` (`exec set-raw`) stores raw bytes, status echoed
  unchanged; `--result_file` (`exec complete`) stores the file content
  as the `result` string value (escaped in output, not parsed as
  JSON) — matches the spec.

## Notes

- `execution_logs` seed data contains a row with `event = ''` (empty
  string, not NULL) from an older writer; `row_to_execution_log`
  accepts it (only SQL NULL is rejected). Not a CLI bug; relevant
  context for finding 1.
