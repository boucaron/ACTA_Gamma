# Plan — acta_cli help improvements

Follow-up to `cli_help_analysis.md`. All work is in `acta_cli/`
(mainly `src/main.c`, `src/argparse.c`, `src/commands/`, per the T1/T3
split in `cli_spec.md`). No DB behavior changes; only help/discoverability.

Status: **P0 done** (commit `108e7ce`), P1 and P2 pending.

## P0 — Per-action help (done, commit `108e7ce`)

- Accept `<entity> help <action>` (and `<entity> <action> --help`),
  printing only that action's help section.
- Single source of truth: both entry points call the per-entity
  `X_help_for_action` lookups, which print the same section functions
  used by the full `<entity> help` dump. `db` and `context` were
  refactored into header + section functions + footer so their sections
  live exactly once; the other eight entities already had per-action
  section functions.
- `X help` / `X help help` → full entity help; `X --help` → full entity
  help; bare `--help` → global usage (unchanged).
- `parse_globals` no longer short-circuits on `--help` (entity/action
  tokens are collected for routing; the "missing entity and/or action"
  check exempts `--help`); `main.c` routes through the shared
  `entity_help()` in `commands.c`.
- Unknown entity or action with `help`/`--help` → standard exit-10 JSON
  error (this also resolves P2 item 4 below as a side effect).
- Tests: new `tests/help` suite (13 (entity, action) cases across all
  10 entities, full help, `help help`, unknown → EXIT_CLI) plus
  `tests/gparse` help-routing seam tests (`parse_globals` keeps the
  entity/action after `--help`, `entity_help` paths). Regression fix:
  `tests/skill/skill_test_misc.c` passed a NULL `cmd_args_t` to the
  `help` action (segfault under the new branch); it now builds a real
  empty one.

## P1 — Structured `success` in `--tools`

- In `src/tools.c`, replace the prose `success` string with a structured
  object, e.g.:
  - `{"kind":"json","keys":["id"]}` for create/update
  - `{"kind":"json","keys":["id","folder_id"]}` for moves
  - `{"kind":"json_object"}` for get/get-latest (full entity)
  - `{"kind":"json_array"}` for list
  - `{"kind":"bare_int"}` for count / `--count`
  - `{"kind":"plain_text"}` for help / `--table`
- Keep the human `--tools --compact` rendering derived from the same
  fields; keep the spec table in `cli_spec.md` updated to match.
- Bump the schema `version` field; test with `test_tools.exe`.

## P2 — Small fixes

- Document the default DB path in the `--db` help line.
- ~~`nope --help` (unknown entity + help): emit the exit-10 JSON error
  instead of plain help with exit 0.~~ — already resolved as a side
  effect of P0 (`entity_help` routing in `main.c`).
- Fix the `skill.update` compact line: empty required-JSON list renders
  as `json: / name,…`; render as `json:name,prompt_template,…` or
  `json:<optional>`.

## Out of scope

- No changes to stdout data schema, exit codes, or DB behavior.
- No change to `<entity> help` content itself (already good).

## Acceptance

- `acta_cli model list help` prints only the list section (exit 0).
- `acta_cli --tools` contains structured `success` for every command;
  `test_tools` passes.
- `acta_cli nope --help` exits 10 with the JSON error line.
- `--tools --compact` renders all commands without the stray `/` wart.
- `cli_spec.md` updated where the schema version/shape changed.
