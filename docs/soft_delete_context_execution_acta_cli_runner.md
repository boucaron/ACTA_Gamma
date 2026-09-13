# Soft delete for contexts and executions — `acta_cli` + `acta_runner` plan

Companion to [`soft_delete_context_execution.md`](soft_delete_context_execution.md)
(design spec) and [`soft_delete_context_execution_acta_db.md`](soft_delete_context_execution_acta_db.md)
(DB layer, **done and tested**). This is the implementation analysis for the two
remaining consumers:

- `acta_cli`: `context delete/restore`, `exec delete/restore`,
  `--include_deleted` / `--deleted` flags on the listers/counts (and `get`),
  `cli_spec.md` rows, `tools.c` (T3), tests.
- `acta_runner`: the `run --pending` claim and the single-id claim path.

**Status:** all sections done. `context.c` and `execution.c` have the
`delete` / `restore` actions, `--include_deleted` / `--deleted` on
`get` / `list` / `count`, and `deleted_at` in JSON/table/vlog output and
the usage text (sections 1, 2); `docs/cli_spec.md` rows in place
(section 4); `tools.c` (T3) is complete — 74-entry table with
`context.delete` / `context.restore` / `exec.delete` / `exec.restore`
and `include_deleted` on the context and exec `get` / `list` /
`count` rows, `tools_test_main.c` updated to the 74-entry table with
the exec action set of 12 (section 3); the `acta_runner` single-claim
guard and the live-only `run --pending` comment are in, with the
runner soft-delete claim suite wired into `make test` (section 5);
the CLI deleted suites are in and green (section 6).

All open questions in section 7 are resolved. The test-ref fixture
migration included replacing the old `contexts_immutable` trigger with
`contexts_soft_delete_only` in `acta_test_ref.sql` (and the regenerated
`acta_test_ref.db`) —
without that, every `contexts` UPDATE (including the `deleted_at`
flag flip) was aborted by the trigger and the context soft-delete
suite failed with `ACTA_DB_ERR_SQL`.

## Reference: the model/skill CLI pattern

Everything below mirrors what `acta_cli/src/commands/model.c` and `skill.c`
already do, so no new CLI machinery is needed:

- `delete <id>` / `restore <id>` actions: `parse_id_positional` → one
  `acta_db_*_delete` / `acta_db_*_restore` call →
  `emit_deleted()` / `emit_ok_restored(gopts, id)` on success,
  `finish_op_error(db, rc, …)` on failure. Both atoms already exist in
  `acta_cli/include/cli_util.h`.
- `--include_deleted` flag: read via `cmd_args_has_flag(ga, "include_deleted")`;
  the `--deleted` **alias already exists globally** — `argparse.c`
  normalizes `--deleted` → `--include_deleted` before dispatch
  (line ~304), so no new argparse work.
- `get` with `--include_deleted`: flag set → `acta_db_*_get` (returns
  deleted rows too); unset → `acta_db_*_get_live` (NULL for deleted →
  `load_row_or_notfound` → exit 1).
- Exit codes come free from `finish_op_error`: `ACTA_DB_ERR_NOT_FOUND` →
  exit 1, `ACTA_DB_ERR_INVALID` → exit 4 — exactly the spec's table.
- JSON create paths are table-driven in `json.c` with **unknown keys
  ignored**, so a stray `deleted_at` key in a create body is silently
  dropped — no parser changes required (see Open questions).

---

## 1. `acta_cli/src/commands/context.c` — **done**

### New actions

- `delete`: `acta_db_context_delete(db, id)` → `emit_deleted()` on OK;
  `finish_op_error(db, rc, "context delete")` on failure (NOT_FOUND →
  exit 1; delete-of-deleted is NOT_FOUND at the DB layer, so no extra
  branch).
- `restore`: `acta_db_context_restore(db, id)` →
  `emit_ok_restored(gopts, id)` on OK; strict restore (live row →
  NOT_FOUND → exit 1) is enforced by the DB layer, no extra branch.

### Existing actions

- `get`: add `int include_deleted = cmd_args_has_flag(ga, "include_deleted")`;
  `include_deleted ? acta_db_context_get(db, id, &err)
  : acta_db_context_get_live(db, id, &err)`; rest of the handler
  unchanged (the existing `load_row_or_notfound` already maps a NULL
  live row to exit 1).
- `list`: `q` unchanged; call
  `include_deleted ? acta_db_context_query_with_deleted(…)
  : acta_db_context_query(…)`. Note: the plain query is now **live-only by
  default** in the DB layer, so `context list` output *does change* for
  DBs that contain deleted contexts — that is the intended semantic, no
  code needed beyond the flag.
- `count`: same switch to `acta_db_context_count_with_deleted`.

### Output & plumbing

- `ctx_to_json`: add the `"deleted_at"` key (model_to_json pattern:
  `fields_has(fl, "deleted_at")`, `no_nulls` aware).
- `ctx_table`: add a `DELETED_AT` column (19-wide, same as `model_table`).
- `vlog_ctx_fields`: add `deleted_at=%s`.
- `ctx_usage` / per-action `usage_*`: add `delete` / `restore` sections and
  `--include_deleted` / `--deleted` notes on `get` / `list` / `count`
  (mirror the model.c usage strings).
- `context_actions[]` array: add the two new action_def rows (drives
  `unknown_action` suggestions and help).

## 2. `acta_cli/src/commands/execution.c` — **done**

### New actions

- `delete`: `acta_db_execution_delete(db, id)` → `emit_deleted()` on OK;
  `finish_op_error` on failure. Exit mapping is automatic:
  - no row / already deleted → `NOT_FOUND` → **exit 1**;
  - `running` → `INVALID` → **exit 4** (per the spec's table).
- `restore`: `acta_db_execution_restore(db, id)` →
  `emit_ok_restored(gopts, id)`; status untouched (DB-layer contract).

### Existing actions

- `list` / `count`: add
  `q.include_deleted = cmd_args_has_flag(ga, "include_deleted");`
  to the `execution_query_t` initializer. Default (flag unset) is
  live-only — the DB-layer default — which is the intended behavior
  change for existing DBs.
- `reset`: **no change** — `acta_db_execution_reset` already refuses
  deleted rows with `NOT_FOUND`, which surfaces as exit 1 via
  `finish_op_error` (the DB test suite already covers the C side; the
  CLI test covers the wire).
- `get`: **decision point** — see Open questions below. The spec's table
  adds flags to `exec list` / `exec count` but not to `exec get`; the
  model/skill pattern has the flag on `get` too.
- `create`: no change — `acta_db_execution_create` now returns
  `ACTA_DB_ERR_NOT_FOUND` for a deleted `--context_id` (DB layer, done);
  the existing `finish_op_error` maps it to exit 1.

### Output & plumbing

- `exec_to_json`: add `"deleted_at"` key (model_to_json pattern).
- `exec_table`: add a `DELETED_AT` column.
- `vlog_exec_fields`: add `deleted_at=%s`.
- `exec_usage` / `usage_list` / `usage_count` (+ `usage_get` if the flag
  is added): add `delete` / `restore` sections and the flag notes.
- `exec_actions[]`: add the two new rows.

## 3. `acta_cli/src/tools.c` (T3) — **done**

Generated data table — **no logic change**, only new rows/flags. The
`context.delete` / `context.restore` entries, `include_deleted` on
context `get` / `list` / `count` (reusing `f_inc_del`), the `context
get` description, the `exec.delete` / `exec.restore` entries,
`include_deleted` on `f_exec_list` / `f_exec_count` / the `exec get`
row (`f_inc_del`), and the `exec get` description are all in place;
`tools_test_main.c` carries the 74-entry table (64 actions + 10
help) with the exec action set of 12.

- New entries (mirror the model/skill delete/restore entries, lines
  ~321/389/466/536):
  - `context.delete` — pos `p_id`, no flags, input `positional`,
    success `{"deleted":true}`.
  - `context.restore` — pos `p_id`, no flags, success
    `{"id":N,"restored":true}`.
  - `exec.delete`, `exec.restore` — same shape (with the existing
    `alias_execution` alias list like the other exec entries).
- Flag arrays:
  - `f_ctx_get`: does not exist yet — `context.get` currently has no
    flags; add `f_inc_del` (reuse the existing `static const tool_flag_t
    f_inc_del[]` already used by model/skill).
  - `f_ctx_list`, `f_ctx_count`: add `include_deleted` (reuse `f_inc_del`
    or extend the arrays).
  - `f_exec_list`, `f_exec_count`: add `include_deleted`.
  - `f_exec_get`: add only if Open question 1 resolves to "yes".
- Descriptions on `get` entries: mirror the model/skill wording
  ("--include_deleted, alias --deleted, returns …").

The compact one-liner output and the JSON schema both derive from
`tool_table`, so both update automatically.

## 4. `docs/cli_spec.md` — **done**

Source of truth for T3 — add:

- `context` table:
  - `context get <id>`: flags `--include_deleted` / `--deleted`.
  - `context delete <id>`: — | `{"deleted":true}`.
  - `context restore <id>`: — | `{"id":N,"restored":true}`.
  - `context list` / `context count`: add `--include_deleted` /
    `--deleted` to the flags column.
- `exec` table:
  - `exec delete <id>`: — | `{"deleted":true}` (exit 4 if `running`).
  - `exec restore <id>`: — | `{"id":N,"restored":true}`.
  - `exec list` / `exec count`: add `--include_deleted` / `--deleted`.
  - (`exec get` row only if Open question 1 resolves to "yes".)
- The wire-format preamble already covers the atoms
  (`emit_deleted`, `emit_ok_restored`) — one line extending their list of
  users to context/exec is enough.

## 5. `acta_runner` — **done**

### `run --pending` (batch claim)

**No code change required.** `run.c` builds the claim query as
`execution_query_t q = ACTA_EXEC_QUERY_ANY; q.status = PENDING;`, and
`ACTA_EXEC_QUERY_ANY` now carries `.include_deleted = 0`; the DB layer
appends the static `deleted_at IS NULL` clause for that default. A
deleted pending row is therefore already skipped by the batch claim.
Optionally add a comment (or set `q.include_deleted = 0` explicitly) so
the "deleted rows are skipped" contract is visible at the call site.

### `run <execution-id>` (single claim)

**One guard needed.** `run_execution` fetches the row with
`acta_db_execution_get` (run.c:361), which now returns deleted rows too.
The DB layer deliberately does **not** guard `start()` on deleted rows
(acta_db plan §4 "Deliberately not guarded"), so without a guard a
`run <deleted-pending-id>` would claim and run the row — violating the
spec ("a `run <execution-id>` on a deleted execution fails with the
standard not-found path").

Implementation: after the successful `get`, check
`if (e->deleted_at) { acta_db_execution_free(e); <emit standard
not-found line, return EXIT_NOT_FOUND (1)> }` — before the status check
and before any `start()`. Nothing else in the single-execution path
changes:

- Context resolution uses `acta_db_context_get`, which returns deleted
  contexts (content physically remains) — per the spec, executing an
  existing execution whose context was later soft-deleted is fine
  ("deletion of a skill or context only matters at `exec create` time").
  No change.
- `sweep.c`: no change. Deleted rows cannot be `running` (delete is
  forbidden from `running`), and its query is live-only by default
  anyway.

### Runner tests (`acta_runner/tests/run/`) — **done**

`tests/run/test_deleted.c` (wired into `make test` as
`tests/run/test_deleted[.exe]`, same harness as `test_pending.c` /
`test_run.c`: scratch `:memory:` DB seeded from
`acta_gui/db/schema.sql` + in-process stub server, `cmd_run` called
directly):

- `run <id>` on a deleted `pending` execution → exit 1, row still
  `pending` (not claimed), no runner error other than not-found.
- `run --pending` with one live + one deleted pending row → runs only
  the live one.

## 6. `acta_cli` tests — **done**

All suites green on the regenerated fixture DB (14/14 suites, no
assertion failures). Following the per-module structure of
`acta_cli/tests/` (each module `*_test_<name>.c` registered in its
directory's `*_test_main.c`):

- `tests/context/`: new `context_test_deleted.c` (+ entry in
  `context_test_main.c`):
  - delete → restore round trip; delete a deleted context → exit 1;
    restore a live context → exit 1.
  - `context get` on deleted → exit 1; with `--deleted` → JSON with
    `deleted_at` set.
  - `list` / `count` default live-only vs `--include_deleted` (and the
    `--deleted` alias).
  - `delete` stdout exactly `{"deleted":true}`; `restore` stdout
    exactly `{"id":N,"restored":true}`.
- `tests/exec/`: `execution_test_deleted.c` (+ entry in
  `execution_test_main.c`):
  - `exec delete` from `pending` / `completed` / `failed` /
    `cancelled` → `{"deleted":true}`; from `running` → exit 4
    (`ACTA_DB_ERR_INVALID`); on a deleted row → exit 1.
  - `exec restore` preserves status (delete a `failed` row, restore,
    `get` shows `failed`, then `exec reset` succeeds).
  - `exec reset` on a deleted row → exit 1.
  - `exec create --context_id <deleted>` → exit 1.
  - `list` / `count` default live-only vs `--include_deleted` parity.
- `tests/tools/tools_test_main.c`: update expectations for the new
  `context.delete` / `context.restore` / `exec.delete` / `exec.restore`
  entries and the new `include_deleted` flags (entry count, compact
  rendering) — done: the table carries 74 entries (64 actions + 10
  help), context action set 7, exec action set 12.

## 7. Open questions (all resolved)

1. **`exec get` flag.** — **Resolved: added.** The flag is on `exec get`
   (consistency with `context` / `model` / `skill get`). The execution DB
   layer has no `get_live`, so the live-only default is enforced in the
   CLI: without the flag, a deleted row is freed and mapped to the
   standard not-found path (exit 1); with the flag, deleted rows are
   returned. `docs/cli_spec.md` carries the row.
2. **`deleted_at` in create JSON bodies.** — **Resolved: leave it.**
   `json.c` is table-driven and silently ignores unknown keys, so
   `{"deleted_at": …}` in a `context create` / `exec create` body is
   dropped. Decision: keep the silent drop (no explicit rejection).
3. **`--table` column set.** — **Resolved: done.** `DELETED_AT` is a
   column in `ctx_table` / `exec_table`; the plain-text layout change is
   noted in the usage/help text (the `get` / `list` sections document
   `--fields` / `--no_nulls`, which suppress the column), and `deleted_at`
   is present in JSON/table/vlog output.

## 8. Explicitly out of scope (per the design spec)

- GUI changes (context picker, Retry disable, per-row restore actions) —
  `acta_gui` is its own follow-up.
- **Hard delete is excluded everywhere in this phase** — contexts and
  executions are soft-deleted only (`deleted_at` flag flip); there is no
  hard-delete command in `acta_db`, `acta_cli` or `acta_runner`, no
  cascades, and no space reclamation.
- No new `acta_db` functions needed — the DB layer API is already
  complete; this phase only consumes it.
- Migration of existing DB files is already done via
  `acta_cli db exec` per the design spec; the CLI/runner changes assume
  the schema is in place (guarding: `PRAGMA table_info` checks belong to
  the migration script, not to these components).
