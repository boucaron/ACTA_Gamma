# acta_cli — help & discoverability review

Exploration-only review of the human/machine help surfaces of
`acta_cli` (`--help`, `<entity> help`, `--tools`, `--tools --compact`,
error paths). No real commands were run against the database.
Companion to `cli_spec.md` (the T1 stdout contract) and
`status.md` (task list).

## Verdict

**Source is sound, agent-ready. Running binary is stale.** The help
system in the source tree (commit `0d17c710`) is internally consistent,
the machine-readable schema exists and matches the spec, and error
semantics are clean (`code` = −exit everywhere). Per-action help (P0,
commit `108e7ce`) is implemented in source but **not in the running
binary** (`acta_cli.exe`, built from `bcafe9e6-dirty`). All top-level
and entity-level help paths (`--help`, `-h`, `<entity> help`,
`<entity> --help`, `<entity> help <action>`) fail in the current
binary; only `<entity> <action> --help` works.

## Findings

| Area | Finding | Good / gap |
|---|---|---|
| Top-level `--help` / `-h` / bare `help` | **Broken in running binary**: exit 10 `ACTA_CLI_ERR` "missing entity and/or action"; does not print global usage. Source has the correct routing (`parse_globals` exempts `show_help`, `main.c` calls `help_print`). | Gap — stale binary |
| `<entity> help` (e.g. `model help`) | **Broken in running binary**: exit 11 `ACTA_DB_ERR_INVALID_DB` — tries to open DB before routing to help. Source routes through `entity_help()`. | Gap — stale binary |
| `<entity> --help` (e.g. `model --help`) | **Broken in running binary**: exit 10 "missing entity and/or action". Source routes through `entity_help()`. | Gap — stale binary |
| `<entity> help <action>` (e.g. `model help create`) | **Broken in running binary**: exit 11 DB open error. Source routes through `entity_help()`. | Gap — stale binary |
| `<entity> <action> --help` (e.g. `model list --help`) | **Works**: prints single-action section, exit 0. Confirmed for `model list`, `model create`, `exec list`, `db exec`. | Good |
| `<entity> help nope` / `<entity> nope --help` | `nope --help` → exit 10 `ACTA_CLI_ERR` "unknown action: nope" + human hint. Correct. | Good |
| `--tools` JSON (~32 KB) | Per command: `description`, typed `positionals`, `flags`, `input` mode, `aliases`, `success` (string), plus top-level `exit_codes` and `error` contract. Works, exit 0. | Good; `success` is structured in the rebuilt binary (P1, commit `1bb0466`) |
| `--tools --compact` (~7 KB) | One line per command, `*` = required, exits + error line in the header. Works, exit 0. | Good for LLM in-context use; minor wart: `skill.update` renders `json: / name,…` (empty required-list slot) |
| Error paths | Unknown entity/action/option → exit 10, single JSON line on stderr (`{"error":"ACTA_CLI_ERR","code":-10,"message":…}`) + human hint "Run `acta_cli <entity> help`". T2 invariant holds. | Good |
| `--db` default | `--db` without value → exit 10 "missing value for --db". Default path (`./acta.db`, or `$ACTA_DB`) is documented in the `--db` help line (P2). | Resolved |
| `--version` | `acta_cli 0.1.0 (bcafe9e6-dirty \| libacta_db, sqlite 3.53.4)` — reflects stale binary. | Fine (format) |

## Missing / improvable (priority order)

0. **Rebuild `acta_cli.exe`** from the current source (commit `0d17c710`).
   The running binary is from `bcafe9e6` and predates the P0 help-routing
   changes. Until rebuilt, 10 help entry points are broken (see table above).
1. ~~**Per-action help**~~ — **done in source** (commit `108e7ce`): `X help
   <action>` and `X <action> --help` print a single action's section for
   every entity; plan in `cli_help_plan.md` (P0).
2. ~~**Structured `success` in `--tools`**~~ — **done in source**
   (commit `1bb0466`): `success` is a JSON object
   `{"kind":"json"|"json_object"|"json_array"|"bare_int"|"plain_text"}`
   (`"keys"` for kind `"json"`, optional `"note"`), schema `version`
   bumped 1 → 2; verified in the rebuilt binary (`test_tools` passes).
3. ~~**Document the default DB path**~~ — **done** (P2): the `--db`
   help line now reads `database file (default: $ACTA_DB, else
   ./acta.db)`.
4. ~~**Unknown entity + `--help` should still exit 10**~~ — **done in source**
   (commit `108e7ce`, side effect of the P0 `entity_help` routing).
5. Optional: **per-command exit-code notes** in the schema (e.g.
   delete-refused → 4); currently only the global table.

## Live re-verification of help edge paths (2025-07-25)

Re-ran the help edge cases against the current binary on a test DB
(`acta_cli/tmp/acta.db`); no source changes.

Verified working (exit 0, single-action section):

- `X help <action>` (`model help list`, `exec help set-raw` — dash
  action names work).
- `X help help` → full entity help (matches the documented behavior).
- `X help nope` / `X nope --help` → exit 10
  `{"error":"ACTA_CLI_ERR","code":-10,"message":"unknown action: nope"}`.
- bare `acta_cli help` → exit 10
  `"missing entity and/or action. See --help."`.

New finding:

- **`X <action> help` (help as trailing positional) is not a valid
  form — and it does not error.** `model list help` silently executes
  `model list` and returns the full list, exit 0: the extra `help`
  token is swallowed as an unexpected positional. This is the same
  positional-swallowing bug as `cli_creation_analysis.md` finding 5,
  in a particularly misleading spot (an agent typing the "obvious"
  help form gets data, not help). The documented forms remain
  `X help <action>` and `X <action> --help`; the positional-rejection
  fix (unexpected positional → exit 10) would also turn this case
  into a proper error.
