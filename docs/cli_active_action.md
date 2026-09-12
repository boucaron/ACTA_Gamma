# Active Actions — from `cli_review.md`

Action plan derived from [`cli_review.md`](cli_review.md). **All summary
items in `cli_review.md` are closed** — item 1 (`--result`/`--error`
data loss), 2 (DEBUG stdin leak), 3 (`create --json` snippets, S4),
4 (global parse layer, S2), 5 (`--tools`, T1–T4), 6 (error-contract
unification, T2), 7 (parse-layer inconsistencies, S4), and 8 (JSON-layer
residue, J1–J3) — each carrying a *Resolved* note in
[`cli_review.md`](cli_review.md). The record of each resolution (what
changed, where, and why) lives in those notes and in the git history;
this file now only tracks what is still live.

## Live constraints

### Hand-rolled residue (intentional — no `cli_util.h` atom expresses these)

- `skill_folder list`/`count`: the *optional* `<parent_id>` positional with
  its `"all"` sentinel — `parse_id_positional` is required and
  positive-only.
- Required *string* flags (`exec set-raw --raw`, folder `rename --name`):
  `require_flag` has no `required` param, so absence is checked by the
  caller with the canonical `missing required flag: --<name>` text.
- Struct-based create validation (flag *or* JSON-sourced fields) —
  flag-reading atoms can't see it.
- `skill update`'s deliberately divergent `--name must not be empty`
  contract (documented in-code vs the atom's `field 'name' must not be
  empty`).

These blocks are accepted as-is; a future edit that touches one should
keep the canonical message texts.

### `--tools` maintenance note

`tool_table[].flags` and argparse's `entity_flag_specs` remain two
hand-synced sources of truth — `make test`'s 69× raw-argv cross-check is
the drift detector, and for the M4/M5 cells it can only prove acceptance,
not spec-vs-code agreement (see [`t4_analysis.md`](t4_analysis.md) §4). A
shared `has_value` header was considered in T3 and is not required.

`--tools --compact` (plain-text, one line per command) is a second
renderer over the same `tool_table` — no new source of truth and no new
drift surface; both renderers live in `src/tools.c` and the T4 suite's
schema walk still pins the full JSON (the compact mode is exercised
manually, not yet pinned in `make test`).

## Closed (for the record)

- **Structural:** S2 (global-parse test suite, `tests/gparse`), S3 (spec
  table, closed via T1), S4 (parse-layer inconsistencies, docs + dead
  `--live` + `--stdin` usage snippets).
- **`--tools` chain:** T1 (wire-format decisions + spec §11 table in
  [`cli_spec.md`](cli_spec.md)), T2 (error-contract unification, Option A
  from [`t2_analysis.md`](t2_analysis.md)), T3 (69-entry schema in
  `src/tools.c`, `--pretty`), T4 (contract suite, `tests/tools`), M4
  (`--all` on `skill list`/`count`), M5 (optional `skill_folder move
  --parent_id`), M6 (per-entry aliases `["execution"]` /
  `["execution_log"]`).
- **Residuals:** J1 (JSON layer cleanup: per-entity acta_db headers break
  the include cycle, `jget_int` range check, negative-id error,
  absent-vs-OOM distinction, error-offset VLOG — pinned in
  `tests/json/json_test_main.c`), J2 (`model get --live` — stale item, no
  code change), J3 (nine `*_usage` made `static`).

Low-priority nitpicks intentionally left as-is (no action taken):
Makefile libraries in `LDFLAGS` rather than `LDLIBS`; the hard-coded
offsets in `parse_globals` (`a[4]`, `a[6]`, `a[9]`, `a[11]`); the redundant
`gopts.argc < 2` re-check in `main.c`; the `--from_file` / `db exec
--file` naming drift; and the `"hash"` → `content_hash` wire-key mapping
(kept — it is the documented contract in `cli_spec.md`).
