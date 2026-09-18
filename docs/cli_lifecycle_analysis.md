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

## The exec state machine (measured, **not in cli_spec.md**)

| Action | Allowed from | Result status | Required flag |
|---|---|---|---|
| `start` | `pending` only | `running` | — |
| `cancel` | `pending`, `running` | `cancelled` | — |
| `complete` | `running` only | `completed` | `--result` / `--result_file` |
| `fail` | `running` only | `failed` | `--error` |
| `reset` | `failed` only | `pending` | — (clears `error`, `raw_response`, `started_at`, `completed_at`) |
| `set-raw` | any | unchanged (echoes current, incl. `pending`) | `--raw*` / `--raw_file*` |

Every illegal transition → exit 4 `ACTA_DB_ERR_INVALID` +
`"(no detail)"`. Consequences for callers: `complete`/`fail` are only
reachable *through* `running` (start first, from `pending`);
`cancelled` is a dead end (no restart, no complete); `reset` only
un-sticks `failed`. All of this is absent from `cli_spec.md` — the spec
lists actions and success shapes but not the transition rules.

## Findings (priority order)

1. **Document the state machine in `cli_spec.md`.** Add the table
   above (plus "all other transitions → exit 4") next to the exec
   section. An agent that cannot see it will probe transitions by trial
   and mistake exit 4 for a DB fault.
2. **`update` silently snapshots a new revision row.** `model update` /
   `skill update` each created a new `model_revision` / `skill_revision`
   (ids 13/12 in the test, same timestamp). This is important semantics
   (the "current" row is updated *and* a revision is appended) and is
   not mentioned anywhere in `cli_spec.md` or the help text.
3. **"(no detail)" message class covers more than create.** Every
   refused transition, the `delete`-from-`running` refusal, and the
   cycle-guarded `move` refusal print `<action> failed: (no detail)`.
   Same root cause as `cli_creation_analysis.md` finding 1:
   `sqlite3_errmsg()` / the specific refusal reason is not surfaced.
   The cycle guard deserves its own message ("cannot move folder into
   its own subtree") — it is a real protection whose reason is
   invisible to the caller.
   **Partial (KI-7):** the create paths and every SQL-error-based
   failure now surface the detail (`acta_db_errmsg` fallback in
   `finish_op_error` + `db_set_error` at C-level FK checks); the
   C-level state-check refusals (illegal transitions, delete-
   from-`running`, cycle-guarded `move`) still return
   `<action> failed: (no detail)` because they decide the error in C
   code without any SQL error — they need their own `db_set_error`
   messages to close this out.
4. **`move` on a soft-deleted row → exit 1 (live-only), no
   `--include_deleted`.** Consistent with get/list live-only defaults,
   but easy to misread as "row gone" — the error could say "row is
   soft-deleted".

## Notes

- `set-raw` on a `pending` row works and keeps the status (spec says
  exactly this); `raw_response` survives a subsequent `reset` test
  ordering in the session but the spec says `reset` clears it —
  `reset` was only exercised from `failed`, per the table.
- `db exec` remains the only path that surfaces SQLite error text;
  entity paths never do (see `cli_creation_analysis.md`).
