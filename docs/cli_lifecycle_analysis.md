# acta_cli — update / lifecycle / move analysis

Exploration-only review of `update`, `delete`/`restore`, exec state
transitions, `move` and `rename`, verified against the running binary on
a throwaway test DB (`acta_cli/tmp/acta.db`). Companion to
`cli_creation_analysis.md` and `cli_spec.md`. Findings only, checked
2025-07-25.

## Verified working

- `model update` / `skill update` (flags and `--json` forms) →
  `{"id":N}`; zero fields → exit 4
  `"at least one field required for update"` (clear message).
- Soft delete → `{"deleted":true}`; `get` of a deleted row → exit 1;
  `get --include_deleted` returns the row with `deleted_at`; `count`
  live vs `--include_deleted` differs correctly (8 vs 9 in the test);
  `restore` → `{"id":N,"restored":true}`; double restore → exit 1;
  `exec restore` leaves the status untouched (spec-compliant).
- `exec delete` from `running` → refused, exit 4 (spec-compliant).
- `move` wire convention verified: `{"id":N,"folder_id":null|M}` for
  entity moves, `{"id":N,"parent_id":null|M}` for folder moves; flag
  `0` → wire `null` (root).
- `rename` → `{"id":N}`.
- Folder moves into their own subtree are rejected (cycle guard exists
  in `acta_db/src/model_folder.c`, `ancestor_reaches` →
  `ACTA_DB_ERR_INVALID`, exit 4) — no tree corruption possible via the
  CLI.

## The exec state machine (measured; now documented in `cli_spec.md`)

| Action | Allowed from | Result status | Required flag |
|---|---|---|---|
| `start` | `pending` only | `running` | — |
| `cancel` | `pending`, `running` | `cancelled` | — |
| `complete` | `running` only | `completed` | `--result` / `--result_file` |
| `fail` | `running` only | `failed` | `--error` |
| `reset` | `failed` only | `pending` | — (clears `error`, `raw_response`, `started_at`, `completed_at`) |
| `set-raw` | any | unchanged (echoes current, incl. `pending`) | `--raw*` / `--raw_file*` |

Every illegal transition → exit 4 `ACTA_DB_ERR_INVALID` with the
refusal reason in the error message (the "(no detail)" degeneration is
gone — see finding 3). Consequences for callers: `complete`/`fail`
are only reachable *through* `running` (start first, from `pending`);
`cancelled` is a dead end (no restart, no complete); `reset` only
un-sticks `failed`.

## Findings (priority order)

1. ~~Document the state machine in `cli_spec.md`.~~ **Resolved.**
   `cli_spec.md` now carries the state-machine table (allowed source
   states, result status, plus the `delete` / `restore` row-class
   refusals) in the exec notes, so agents see the transition rules
   instead of probing by trial and mistaking exit 4 for a DB fault.
2. ~~`update` silently snapshots a new revision row.~~ **Resolved.**
   `model update` / `skill update` each created a new
   `model_revision` / `skill_revision` (ids 13/12 in the test, same
   timestamp). That semantics (the "current" row is updated *and* a
   revision is appended) is now documented in `cli_spec.md` (the
   "Versioning side effect" note: `update` and `move` — `folder_id`
   being a tracked field — append `revision = max(revision) + 1` via
   the schema triggers whenever a tracked value changes; `create`
   snapshots the initial revision, `delete` a final deleted one, no-op
   updates snapshot nothing). The trigger design itself was already in
   `docs/DBDesign.md`; only the CLI-visible consequence was missing.
3. **"(no detail)" message class covers more than create.** Every
   refused transition, the `delete`-from-`running` refusal, and the
   cycle-guarded `move` refusal print `<action> failed: (no detail)`.
   Same root cause as `cli_creation_analysis.md` finding 1:
   `sqlite3_errmsg()` / the specific refusal reason is not surfaced.
   The cycle guard deserves its own message ("cannot move folder into
   its own subtree") — it is a real protection whose reason is
   invisible to the caller.
   **Resolved.** The create paths and every SQL-error-based failure
   surface the detail (`acta_db_errmsg` fallback in `finish_op_error` +
   `db_set_error` at C-level FK checks), and the remaining class — the
   C-level state-check refusals that decide the error without any SQL
   error (illegal exec transitions, delete-from-`running`,
   reset/restore row-class refusals, model/skill/folder NOT_FOUND
   paths, the folder-move cycle guard, and the folder-delete guards
   for live sub-folders / live assigned models) — now records its own
   `db_set_error` message (`execution start failed: execution 3 is
   'completed'; start requires status 'pending'`, `cannot move
   model_folder 6 into its own subtree: parent 8 is a descendant of 6`,
   …). Refusal-message pins via `stest_stderr` live in the `tests/exec`
   (lifecycle + deleted), `tests/context` (deleted), `tests/model`
   (error_contract), `tests/skill` (update + delete/restore),
   `tests/model_folder` and `tests/skill_folder` (move +
   delete/restore) suites; the exit-code split (refused operation on
   an existing row → exit 4 `INVALID`; missing / already-deleted row →
   exit 1 `NOT_FOUND`) is documented in `cli_spec.md`.
4. ~~`move` on a soft-deleted row → exit 1 (live-only), no
   `--include_deleted`.~~ **Resolved.** The refusal message now says
   so: `model/skill/model_folder/skill_folder move failed: <row> N
   does not exist or is soft-deleted` — the `db_set_error` detail added
   by the KI-7 follow-up (finding 3), pinned by `test_move_refusal_msgs`
   in `tests/model_folder` and `tests/skill_folder`. Live-only
   semantics with no `--include_deleted` is unchanged and is the
   consistent get/list default.

## Notes

- `set-raw` on a `pending` row works and keeps the status (spec says
  exactly this); `raw_response` survives a subsequent `reset` test
  ordering in the session but the spec says `reset` clears it —
  `reset` was only exercised from `failed`, per the table.
- Superseded by the KI-7 work: entity paths now surface error detail
  too — `finish_op_error` falls back to `acta_db_errmsg` (the
  connection's `sqlite3_errmsg`) when the library stored no
  `last_error`, and every C-level refusal records its own reason via
  `db_set_error` (see finding 3, resolved).
