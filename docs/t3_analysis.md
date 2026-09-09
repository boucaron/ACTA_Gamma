# T3 — Implement `--tools`: analysis

Analysis for the `--tools` machine-readable tool-schema item in
[`cli_active_action.md`](cli_active_action.md). Blockers T1 (spec table in
[`cli_spec.md`](cli_spec.md)) and T2 (error contract) are done, so T3 is
unblocked. Source: Agentic #1 in [`cli_review.md`](cli_review.md).

Goal (from the plan): a static per-(entity, action) data table in one file
(suggested `src/tools.c`) that is the **single source of truth**;
`tools_print` renders it with the existing JSON emitter. After this the
help line "see --tools for full command reference" is true.

## 0. Status (post-implementation, audited)

T3 is **implemented**: `src/tools.c` holds the 69-entry table and the
renderer; `tools_print(FILE*, int pretty)` moved out of `commands.c`;
`main.c` passes `gopts.pretty`; help line updated per D3. M1–M3 spec
table fixes are applied in `cli_spec.md`. Verified against a live
`--tools` run:

- valid JSON (compact default, one line, trailing newline);
- **69 entries** (59 actions + 10 help); per-entity counts: db 3,
  context 5, model 9, model_folder 9, model_revision 5, skill 9,
  skill_folder 9, skill_revision 5, exec 10, log 5;
- exactly the 8 JSON-capable actions carry `json_keys` (incl.
  `skill.update` per M2, `hash` key per M1);
- per-action flag lists match `cli_spec.md` (spot-checked, e.g.
  `exec count` = `exec list` minus `--count/--table/--fields/--no_nulls`).

**Bug found during the audit and fixed:** the `--pretty` rendering was
invalid JSON — `jsep`'s pretty path used `if (i + 1 < n) fputc(',', f)`,
which emitted a leading comma **before the first field** of every
object. Fixed to `if (i > 0)` (leading-comma scheme; no trailing comma
after the last field). Compact mode was already valid. **T4 must test
both modes** — `--tools --pretty` before the fix failed any JSON parse.

**Two further `--pretty` defects found during T4 development (both
fixed):**

- `jsep`'s pretty path emitted a leading `\n` unconditionally, so the
  first field landed on line 3 with a blank line after the opening
  `{` (still valid JSON, but broke the shape contract). The newline is
  now emitted only for `i > 0`.
- `jf_entity_aliases`' pretty path emitted a trailing comma inside the
  `exec` array (`"execution", ]`) — invalid JSON. Removed.

Deviations to carry into T4:

- Per-entry `aliases` arrays are all empty — alias info lives in the
  global `entity_aliases` map plus the repeated "canonical entity is
  `exec`/`log`" description text. T4 must not assert non-empty
  `aliases` per entry.
- M4/M5 remain deferred: the table follows `cli_spec.md`, not the code,
  for `skill list`/`count` (`--all` absent) and `skill_folder move`
  (`--parent_id` still required in the spec).

## 1. Current state (audited)

| Item | State |
|------|-------|
| `parse_globals` | `--tools` → `g->show_tools = 1`, early exit in `main.c:72` **before** DB open — no DB needed, `tools_print(stdout)` |
| `tools_print` (`commands.c:127`) | stub: prints `[]`, returns `EXIT_OK` |
| `--pretty` | parsed (`argparse.c:98`, `g->pretty`), advertised in `help_print` ("2-space indent JSON"), **honored by no emitter** |
| `commands.h:21` | `int tools_print(FILE *out);` — single-arg signature; `cli_gopts` is not yet set on the tools path (it is set inside `commands_dispatch`, after the early exit), so `tools_print` cannot read the pretty flag via `cli_gopts` |
| JSON emitter | CLI output is hand-rolled (`json_str` from `cli_util.h` + `fprintf`); there is no serialize API (`json.h` is parse-only) |
| Flag vocabulary (parse side) | `entity_flag_specs[]` in `argparse.c` (name → `has_value`), static to that file; `cmd_args_validate` uses it, so the tools table's per-action flag lists must be a *subset* of this vocabulary or commands will fail validation |
| Makefile | `SRCS := $(wildcard src/*.c)` — a new `src/tools.c` is picked up with no Makefile change |

## 2. The data inventory (what the table must cover)

Action counts per entity, from [`cli_spec.md`](cli_spec.md):

| Entity | Actions | + `help` |
|--------|---------|----------|
| db | `exec`, `version` (2) | 1 |
| context | `create`, `get`, `list`, `count` (4) | 1 |
| model | `create`, `get`, `update`, `delete`, `restore`, `move`, `list`, `count` (8) | 1 |
| model_folder | `create`, `get`, `list`, `count`, `rename`, `delete`, `restore`, `move` (8) | 1 |
| model_revision | `get`, `get-latest`, `list`, `count` (4) | 1 |
| skill | `create`, `get`, `update`, `delete`, `restore`, `move`, `list`, `count` (8) | 1 |
| skill_folder | `create`, `get`, `list`, `count`, `rename`, `delete`, `restore`, `move` (8) | 1 |
| skill_revision | `get`, `get-latest`, `list`, `count` (4) | 1 |
| exec | `create`, `get`, `start`, `cancel`, `complete`, `fail`, `set-raw`, `list`, `count` (9) | 1 |
| log | `create`, `get`, `list`, `count` (4) | 1 |
| **Total** | **59**¹ | **10 → 69 entries** |

¹ *Correction (found during T3 implementation):* the per-entity counts in
this table sum to **59**, not 57 — the 57/67 figures carried through
earlier drafts were an arithmetic error. The implemented table has
**69 entries** (verified against both this table and the
`action_def_t` arrays in all 10 handlers). **T4's count check must
assert 69.**

Doc aliases that agents will actually emit (dispatch only accepts the
canonical names — `lookup_entity` in `commands.c`): `execution` for
`exec`, `execution_log` for `log`. These must appear as `aliases` on the
entries so an agent reading `--tools` knows `execution create` is *not*
a command, but the schema for `exec create` applies.

JSON-capable actions (code-audited via `resolve_input_source`):
`context create`, `model create`, `model_folder create`,
`skill_folder create`, `skill create`, **`skill update`**, `exec create`,
`log create`. `model update` is **flags-only** (no `resolve_input_source`
in that handler). JSON bodies are parsed by `json.c`; keys per parser:

- model: `name, description, backend, base_url, model_identifier, configuration` (+ `folder_id`)
- skill: `name, description, prompt_template, output_schema` (+ `folder_id`)
- context: `type, content, **hash**, metadata` ← see mismatch M1
- folders: `name, parent_id`
- exec: `context_id, skill_revision_id, model_revision_id, prompt, parent_execution_id` (`status`/`id` are read by the parser but create ignores them — row is always created `pending`)
- log: `execution_id, level, event, message, metadata`

Input sources are globally mutually exclusive (`resolve_input_source`,
`commands.h:134`): `--json <blob>` / `--stdin` / `--from_file <p>`;
combining two is an error (exit 4). `db exec` never takes JSON; it takes
positional `sql` or `--sql` / `--file` (≤ 64 KiB) / `--sql_stdin`
(boolean flag, ≤ 64 KiB) — mutually exclusive, first wins; global
`--stdin` is explicitly rejected in `db.c`.

## 3. Spec-vs-code mismatches found during the audit

These must be settled **before** the data table is written, because the
table claims single-source-of-truth status. All are spec-table
errors (code is the behavior to document):

| # | Mismatch | Code fact | Fix |
|---|----------|-----------|-----|
| M1 | `cli_spec.md` context create JSON keys: `{type*, content*, content_hash*, metadata}` | `json.c:104` — `c->content_hash = jget_str(root, "hash")` | spec table key → `hash*` |
| M2 | `cli_spec.md` skill update input column: `—` | `skill.c:682` — `resolve_input_source` + `json_parse_skill`; usage: "Read the skill patch as JSON"; ≥ 1 recognised field required in JSON mode too | spec input → `flags or JSON {name, prompt_template, folder_id, description, output_schema} (≥ 1)` |
| M3 | `cli_spec.md` skill_folder `list`/`count`: positionals `—`, flags include `--parent_id` | `skill_folder.c:491,593` — the handler reads an **optional positional** `<parent_id \| all>`; no `--parent_id` flag is read for list/count (only `move` uses the flag) | spec: positional `parent_id` (optional; `all` sentinel), drop `--parent_id` from list/count flags |

Related quirk (out of T3 scope, note for S4): because `--parent_id` is in
`entity_flag_specs`, `cmd_args_validate` **accepts**
`skill_folder list --parent_id 3` and the handler silently ignores it.
The tools table must document the positional (per M3), not the flag.

**M4/M5 — found during T3 implementation, deferred follow-ups.**
Two further spec-vs-code mismatches surfaced while building the table.
Per the D2 rule ("flags exactly as `cli_spec.md` lists them") the
table follows the spec for both, so the spec table is still incomplete
with respect to "full command reference"; T4's cross-check passes
either way (`--all` is in `entity_flag_specs`, and an optional flag
documented as required is never rejected):

| # | Mismatch | Code fact | Follow-up |
|---|----------|-----------|-----------|
| M4 | `cli_spec.md` skill `list`/`count` flags omit `--all` | `skill.c:895,1002` — a real boolean `--all` flag ("skills from all folders"); in `entity_flag_specs`; documented in the per-action usage text | add `--all` to the spec flag cells, then to `tool_table` |
| M5 | `cli_spec.md` `skill_folder move`: `--parent_id*` (required) | `skill_folder.c:676` — `parse_nonneg_int_flag(..., 0, ...)`: optional, defaults to root (0) | spec: `--parent_id` (0 = root), drop the `*`; `model_folder move` stays required (`model_folder.c:716`) |

## 4. Wire-format decisions

### D1 — Top-level shape

**Decision: top-level object with a `tools` array plus a `global`
section** (replacing the `[]` stub; nothing consumes the stub today, so
changing the top level is free). The common contract is stated **once**,
not repeated 69 times:

```json
{
  "name": "acta_cli",
  "version": 1,
  "usage": "acta_cli [global flags] <entity> <action> [args]",
  "global_flags": [ {"name":"db","has_value":true}, ... ],
  "entity_aliases": {"exec":["execution"], "log":["execution_log"]},
  "input_sources": ["json","stdin","from_file"],
  "exit_codes": {"0":"ok","1":"not found","2":"sql error","3":"OOM",
                   "4":"invalid argument / missing flag / missing required field / duplicate / FK / invalid DB file",
                   "10":"CLI usage error","11":"DB open failed"},
  "error": {"stream":"stderr","line1":"{\"error\":\"ACTA_DB_ERR_*|ACTA_CLI_ERR\",\"code\":-<exit>,\"message\":\"...\"}",
            "invariant":"code == -exit"},
  "tools": [ <69 entries> ]
}
```

Rationale: flat arrays of per-action objects can't hold the exit-code /
error-line contract without 69-fold repetition; the wrapper costs one
level of parsing and self-documents. `--tools` needs no DB
(`main.c:72`), so the whole object is static data.

### D2 — Per-entry fields

```json
{
  "command": "model.create",
  "entity": "model",
  "action": "create",
  "aliases": [],
  "description": "Create a new model",
  "positionals": [ {"name":"id","required":true,"type":"positive-int"} ],
  "flags": [ {"name":"name","has_value":true,"required":true},
             {"name":"include_deleted","has_value":false} ],
  "input": "flags" | "flags|json" | "positional|flags",
  "json_keys": {"required":["name","backend","model_identifier"],
                 "optional":["folder_id","description","base_url","configuration"]},
  "success": "{\"id\":N}"
}
```

Rules:

- `flags` = the entity flags for that action **exactly as
  [`cli_spec.md`](cli_spec.md) lists them** (which already include the
  per-action global modifiers `--id_only` / `--table` / `--fields` /
  `--no_nulls` / `--count`). All names are a subset of
  `entity_flag_specs` so `cmd_args_validate` accepts them. Global
  flags (`--db`, `--verbose`, `--json`, `--stdin`, `--from_file`,
  `--help`, `--version`, `--tools`, `--pretty`) live in `global_flags`
  only.
- `input`: `"flags"` (model update, all get/delete/restore/move/rename/
  transitions), `"flags|json"` (the 8 JSON-capable actions, with
  `json_keys` present), `"positional|flags"` (db exec), `"positional"`
  for the pure-positional reads. `json_keys` uses the **code's** keys
  (post-M1/M2 fixes).
- `success` = the `cli_spec.md` "stdout on success" cell verbatim, plus
  the `--id_only` / `--table` modifier notes where applicable (spec
  common-shapes table).
- `help` actions: 10 extra entries with `"input":"none"`,
  `"success":"usage text (plain, not JSON)"`, exit `0`. Included so the
  "full action set" is literally complete (T4 count check: 69).
- No per-entry exit codes — the global section covers all; an action
  that can only fail 0/1/4 gets nothing extra (agents should not be told
  an exit code that can't happen).

### D3 — `--pretty`

**Decision: implement it, scoped to `--tools`.** Options were:
(a) honor `--pretty` as 2-space-indent for the tools JSON; (b) drop it
from help and parsing. (b) changes argv behavior — a bare `--pretty`
currently passes through `parse_globals`; removing it would make
`cmd_args_validate` reject it, a silent behavior change. (a) is a few
lines in the renderer and makes the existing help line true.

Implementation consequence: `cli_gopts` is unset on the tools path, so
the signature changes to `int tools_print(FILE *out, int pretty);`
(`commands.h:21`, call site `main.c:72` → `tools_print(stdout,
gopts.pretty)`). Help line becomes:
`--pretty  2-space indent JSON (applies to --tools output)`. Entity
JSON output stays single-line (documented scope; a global pretty for
entity output is a separate, larger change).

Default (no `--pretty`): **single-line compact** — script-friendly and
parseable as one JSON value; trailing newline in both modes.

### D4 — Rendering

Hand-rolled, matching the existing emitter style: `fprintf` +
`json_str()` from `cli_util.h` for every string (descriptions contain
`"` and `\n`-safe content only after escaping). No new serialization
layer; cJSON is parse-only by design (`json.h`).

## 5. Implementation plan

1. **`docs/cli_spec.md`** — apply M1, M2, M3 fixes (three table cells).
2. **`acta_cli/src/tools.c`** (new; Makefile wildcard picks it up):

   ```c
   typedef struct { const char *name; int required; const char *type; } tool_pos_t;
   typedef struct { const char *name; int has_value; int required; } tool_flag_t;
   typedef struct {
       const char *command, *entity, *action;
       const char *const *aliases; size_t n_aliases;
       const char *description;
       const tool_pos_t *positionals; size_t n_pos;
       const tool_flag_t *flags; size_t n_flags;
       const char *input;
       const char *const *json_req; size_t n_json_req;
       const char *const *json_opt; size_t n_json_opt;
       const char *success;
   } tool_entry_t;

   static const tool_entry_t tool_table[] = { /* 69 entries */ };
   ```

   - NULL-terminated string arrays for `aliases`/`json_keys` (or count
     fields as above) — plain static data, C11.
   - `const global section` data (global flags, exit codes, error line)
     in the same file.
   - Move the `tools_print` body out of `commands.c` into `tools.c`
     (delete the `[]` stub); keep the `commands.h` declaration, updated
     for the `pretty` parameter.
3. **`acta_cli/src/main.c`** — pass `gopts.pretty`.
4. **`acta_cli/src/commands.c`** — help line for `--pretty` (D3);
   `--tools` line stays "JSON tool schema".

Size estimate: ~670–800 LOC in `tools.c` (data-dominated), < 30 LOC of
other changes. No new test files in T3 (that is T4).

## 6. Risks / watch-outs

- **Dual flag vocabulary.** `entity_flag_specs` (argparse.c) is the parse
  truth; `tool_table[].flags` is the documentation truth. They must be
  kept in sync by review + T4's cross-check (generate a command per
  entry, run it through `stest_run_argv`, assert it is not rejected by
  `cmd_args_validate`). A shared header for the `has_value` table is a
  possible follow-up but is *not* required for T3 (data-only table is
  the agreed scope).
- **M3 quirk**: `skill_folder list --parent_id 3` is accepted by the
  validator and silently ignored by the handler. The table documents the
  positional; the quirk belongs to S4 (parse-layer strictness).
- **Aliases are informational only**: dispatch rejects `execution` /
  `execution_log` (`unknown entity`, exit 10). The table must make that
  clear ("canonical command is `exec`"), otherwise agents emit the
  alias and hit the error path.
- **Keep the single-line default**: T4 will parse the output with the
  project's own JSON layer; compact one-line keeps that trivial.
- **`db exec` flag set is special**: `sql` (value), `file` (value),
  `sql_stdin` (boolean) — all in `entity_flag_specs`; the mutually
  exclusive / first-wins / 64 KiB rules belong in the entry description,
  not new flags.
- **`exec set-raw`**: `--raw` is a required string flag (`has_value: 1`,
  `required: 1`); success echoes the unchanged current status — the
  table's `success` cell must say exactly that (spec T1 decision).

## 7. Open sign-offs

1. Top-level object with `global` section + `tools` array (D1) — or a
   flat array with per-entry repetition. **Recommendation: object.**
2. `--pretty` implemented, scoped to `--tools`, via
   `tools_print(FILE*, int pretty)` (D3).
3. `help` actions included in the table (69 entries, T4 count = 69).
4. M1–M3 spec-table fixes as the single source of truth (code wins).

## 8. Unblocks / next

T4 (`test --tools`): valid JSON via the project's json layer **in both
compact and `--pretty` modes** (pretty was invalid JSON before the
leading-comma fix in `jsep`), 69-entry count, per-entity action
coverage, and per-entry flag/positional cross-check by feeding
generated commands through the raw-argv path (`tests/helpers/test_helpers`
`stest_run_argv`) — which also exercises the S2 global-parse layer.
