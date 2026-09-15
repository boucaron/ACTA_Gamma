# Plan — acta_cli help improvements

Follow-up to `cli_help_analysis.md`. All work is in `acta_cli/`
(mainly `src/main.c` and `src/tools.c`, per the T1/T3 split in
`cli_spec.md`). No DB behavior changes; only help/discoverability.

## P0 — Per-action help

- Accept `<entity> <action> help` (and map `<entity> <action> --help`
  to the same), printing only that action's help section.
- Source the sections from one shared table (same table that renders
  `<entity> help` and `--tools`), so there is a single source of truth —
  no hand-maintained per-action strings that can drift.
- Keep `<entity> help` as the full dump; top-level `--help` unchanged.
- Unknown action + `help`/`--help` → standard exit 10 path.
- Tests: extend the CLI test harness with a few assertions
  (`model list help` prints only the `list` section and not `create`;
  `model list --help` ≡ `model list help`; exit codes).

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
- `nope --help` (unknown entity + help): emit the exit-10 JSON error
  instead of plain help with exit 0 (help still readable; message
  carries the usage hint).
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
