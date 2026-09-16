# acta_cli — help *text* review (wording & consistency)

Exploration-only review of the human-readable help content in
`acta_cli/` — the global usage (`help_print` in `src/commands.c`), the
per-entity usages and per-action sections in `src/commands/*.c`, and the
`--tools` schema table in `src/tools.c`.

Complements `cli_help_analysis.md` (routing / discoverability / binary
state) and `cli_help_plan.md` (P0/P1/P2). This file covers the **text
itself**, not the routing: all paths verified working in the current
binary (`exec --help`, `exec get --help`, `log --help`,
`model_folder --help`, `skill_revision --help`, unknown action → exit 10).

## What's good

- **Three consistent levels**: global `--help`, entity
  (`<entity> help` / `<entity> --help`), action
  (`<entity> <action> --help` / `<entity> help <action>`).
- **Single source of truth**: per-action section functions are shared
  by the full-entity `X_usage()` compositor and `X_help_for_action()`,
  so the two entry points cannot drift. Verified: `exec --help` and
  `exec help get` print identical sections.
- **Uniform section template**: description → examples (flag form and
  `--stdin` JSON form) → required/optional fields → options →
  stdout/stderr contract (e.g. `db exec` documents `{"status":"ok"}`
  and the stderr error line).
- **Aligned with the machine contract**: unknown action/entity under
  help still emits the canonical JSON error line first (exit 10), then
  a human hint. `--tools` remains the machine-readable reference;
  `help_print` points to it instead of duplicating everything.
- Read-only entities are consistent with their role
  (`skill_revision`: only get/get-latest/list/count).

## Findings / improvable (priority order)

1. **Verbosity-scale inconsistency** — `help_print` documents
   `--verbose, -v … (repeatable, 1-3)`; `db exec`'s section says
   `--verbose <n> … debug level 0-3 (stderr)`. Same flag, two ranges
   (and `db exec` also implies it takes a value while the global help
   says "repeatable"). Pick one canonical description and reuse it.
2. **Uneven depth between entities** — `model` / `exec` sections carry
   field-by-field descriptions, an explicit Required/Optional split and
   an output contract; `context` sections are terser and omit the
   Required/Optional breakdown and the explicit stdout contract.
   Apply the fuller template uniformly.
3. **Repeated global-flag blocks** — `--table`, `--fields`, `--id_only`,
   `--no_nulls`, `--include_deleted` / `--deleted` are re-listed in
   nearly every `get`/`list` section. Intentional (scoped help should be
   self-contained), but a shared "common output options" block could
   DRY it at the cost of standalone readability. Low priority.
4. **`--json` exclusivity phrasing** — `model create` says
   "Read the entry as JSON; --stdin and --from_file <path> are the
   alternative sources"; `db exec` is explicit: "mutually exclusive,
   first wins". Unify to the explicit phrasing since exclusivity is a
   real constraint (combining two sources is an error per `help_print`).
5. Optional: `help_print`'s global flag list and per-entity "Global
   options" footers list partially different flag sets (e.g. `db`'s
   footer only has `--table` / `--verbose`); consider a canonical
   ordering so `--stream` / `--out` / `--raw_out` placement is predictable.

## Out of scope

- No changes to routing, exit codes, stdout schema, or DB behavior
  (that territory belongs to `cli_help_analysis.md` / `cli_spec.md`).
- `--tools` schema content is already covered by the P1 work in
  `cli_help_plan.md`.
