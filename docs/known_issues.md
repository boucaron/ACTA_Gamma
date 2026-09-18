# Known issues

Small tracking file for CLI issues found by read-only testing against a
real DB (`acta_cli/tmp/acta.db`, 2026-09-18). Status: **open** until fixed;
fixes are regression-pinned in `acta_cli/tests` when done.

| # | Issue | Status | Repro |
|---|---|---|---|
| 1 | Trailing positional → rc 10 **but the payload is still emitted on stdout**: the handler runs and prints, then `commands_dispatch` rejects the leftover token. Stdout carries data while the exit code says failure (ambiguous T1 contract). | **fixed**: dispatch now rejects surplus positionals (tools-table count via `tool_entry_positionals`; `help` takes 1) BEFORE the handler runs — rc 10, stderr error line only, no stdout payload. Pinned by `test_trailing_help_rejected` (`stest_run_dispatch`) | `acta_cli context list 5` → rc 10, stdout empty |
| 2 | `model_revision get-latest` **ignores soft-delete** — returns the latest row by id even when `deleted_at` is set (latest *live* revision of model 1 in the test DB is id 2, get-latest returns deleted id 11). | open | `acta_cli model_revision get-latest 1` |
| 3 | `model_revision get <deleted>` returns rc 0 with the deleted row — inconsistent with `context get` / `exec get` (deleted → rc 1). | open | `acta_cli model_revision get 3` |
| 4 | Revision listers include deleted rows **by default** and have no `--include_deleted` machinery (unlike context/exec/model/skill); `cli_spec.md` lists no flags for `model_revision count` yet `--include_deleted` is accepted as a no-op. Spec/code mismatch. Root cause of #2/#3: the revision queries in `acta_db` have no `deleted_at` filtering at all. | open | `acta_cli model_revision list 1` → 4 rows, 2 deleted |
| 5 | `--verbose` help wording inconsistent: global says "repeatable, 1-3"; `db exec` help says "debug level 0-3" (implies a value). Doc-only. | open | `db exec --help` vs `--help` |
| 6 | `log list` shows seed rows with `event: ""` (empty string, not NULL) — data quirk, not a CLI bug. | open (cosmetic) | `acta_cli log list 1` |
