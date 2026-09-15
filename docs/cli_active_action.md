# CLI — live constraints and residuals

All items from the `acta_cli` review workstream are closed; the record of
each resolution lives in the git history. This file now only tracks what
is still live.

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

## Closed (for the record)

- **Items:** `--result`/`--error` data loss, DEBUG stdin leak, `create
  --json` snippets (S4), global parse layer (S2), `--tools` (T1–T4),
  error-contract unification (T2), parse-layer inconsistencies (S4),
  JSON-layer residue (J1–J3).
- **Structural:** S2 (global-parse test suite, `tests/gparse`), S3 (spec
  table, closed via T1), S4 (parse-layer inconsistencies, docs + dead
  `--live` + `--stdin` usage snippets).
- **`--tools` chain:** T1 (wire-format decisions + spec §11 table in
  [`cli_spec.md`](cli_spec.md)), T2 (error-contract unification, Option A),
  T3 (74-entry schema in
  `src/tools.c`, `--pretty`), T4 (contract suite, `tests/tools`), M4
  (`--all` on `skill list`/`count`), M5 (optional `skill_folder move
  --parent_id`), M6 (per-entry aliases `["execution"]` /
  `["execution_log"]`).
- **Residuals:** J1 (JSON layer cleanup: per-entity acta_db headers break
  the include cycle, `jget_int` range check, negative-id error,
  absent-vs-OOM distinction, error-offset VLOG — pinned in
  `tests/json/json_test_main.c`), J2 (`model get --live` — stale item, no
  code change), J3 (nine `*_usage` made `static`).
- **Accepted as-is (no action taken):** the hand-synchronized
  `tool_table[].flags` / `entity_flag_specs` pair (the 74× raw-argv
  cross-check remains the drift detector; a shared `has_value` header was
  considered in T3 and is not required); `--tools --compact` (second
  renderer over `tool_table` in `src/tools.c`; the T4 schema walk pins
  the full JSON, compact mode exercised manually); the redundant
  `gopts.argc < 2` re-check in `main.c`; the `--from_file` / `db exec
  --file` naming drift; and the `"hash"` → `content_hash` wire-key
  mapping (kept — it is the documented contract in `cli_spec.md`).

The former hard-coded offsets in `parse_globals` (`a[4]`, `a[8]`, `a[6]`,
`a[11]`) are gone: they are now derived via `flag_inline_value()`
(`2 + strlen(name)`), so a flag rename can no longer silently desync a
value offset.
