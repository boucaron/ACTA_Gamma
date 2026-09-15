# acta_cli — help & discoverability review

Exploration-only review of the human/machine help surfaces of
`acta_cli` (`--help`, `<entity> help`, `--tools`, `--tools --compact`,
error paths). No real commands were run against the database.
Companion to `cli_spec.md` (the T1 stdout contract) and
`status.md` (task list).

## Verdict

**Sound, agent-ready.** The help system is internally consistent, the
machine-readable schema exists and matches the spec, and error semantics
are clean (`code` = −exit everywhere). The main ergonomic gap is the
absence of per-action help.

## Findings

| Area | Finding | Good / gap |
|---|---|---|
| Top-level `--help` | Concise; lists entities, global flags, input-source exclusivity (`--json`/`--stdin`/`--from_file`); points to `--tools` | Good |
| `<entity> help` | Per-action sections with examples, required/optional fields, aliases, defaults (`--offset 0`, `--limit 0 = unlimited`) | Good, but dumps the **entire entity** — no single-action help (`model list --help` falls back to the global help text) |
| `--tools` JSON (~32 KB) | Per command: `description`, typed `positionals`, `flags`, `input` mode, `aliases`, `success` (string), plus top-level `exit_codes` and `error` contract | Good; `success` is a prose string, not structured |
| `--tools --compact` (~7 KB) | One line per command, `*` = required, exits + error line in the header | Good for LLM in-context use; minor wart: `skill.update` renders `json: / name,…` (empty required-list slot) |
| Error paths | Unknown entity/action/option → exit 10, single JSON line on stderr (`{"error":"ACTA_CLI_ERR","code":-10,"message":…}`) + human hint "Run `acta_cli <entity> help`" | Good; T2 invariant holds |
| `nope --help` | Prints global help, **exit 0** (help wins), whereas `nope` alone is exit 10 | Minor: hides typos if an agent always passes `-h` |
| `--db` default | Help only says "database file"; the default path is not documented | Gap |
| Duplication | Prose `X help` and `--tools` JSON are two renderings of the command table; prose-only details (defaults, notes) are not in the schema | Drift risk between prose and schema |
| `--version` | `acta_cli 0.1.0 (<commit> \| libacta_db, sqlite <ver>)` | Fine |

## Missing / improvable (priority order)

1. ~~**Per-action help**~~ — **done** (commit `108e7ce`): `X help
   <action>` and `X <action> --help` print a single action's section for
   every entity; plan in `cli_help_plan.md` (P0).
2. **Structured `success` in `--tools`** — e.g.
   `{"type":"json","keys":["id"]}` / `{"type":"bare_int"}` /
   `{"type":"json_array"}` instead of a prose string.
3. **Document the default DB path** in the `--db` help line.
4. ~~**Unknown entity + `--help` should still exit 10**~~ — **done**
   (commit `108e7ce`, side effect of the P0 `entity_help` routing).
5. Optional: **per-command exit-code notes** in the schema (e.g.
   delete-refused → 4); currently only the global table.
