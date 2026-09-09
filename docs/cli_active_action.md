# Active Actions — from `cli_review.md`

Action plan derived from [`cli_review.md`](cli_review.md). The earlier
`db`-surface round closed its in-scope items; what remained there (F2, F3)
was folded into the plan below (F3 → S2, F2 → S3).

## Structural (highest long-term payoff)

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| S2 | **Test the global parse layer** — raw `argv` through `parse_globals` + `commands_dispatch` (in-process or spawning the binary). Every bug in the P2 class (`--json` blob ignored, `--stdin` eaten, bare `--json` unparseable, dead local `--count`) lives in this untested seam (former F3) | P5 #3 | the `stest_run_argv` helper (`tests/helpers/test_helpers`) is already the in-process raw-argv path used by the P2 input-source regression cases — a dedicated suite is the missing piece (the earlier orphaned `tests_parse_globals.c` seed was deleted in `d31ce98`; the helper above is the starting point) |
| S3 | **Per-action stdout-schema table in the spec** — ✅ *closed via T1*: [`cli_spec.md`](cli_spec.md) is the single source of truth for the per-action stdout schema (all 10 entities, plus the error line and exit codes); a code audit confirmed every shape is emitted through the shared atoms (`emit_ok_id` / `emit_ok_folder` / `emit_deleted` / `emit_ok_transition` / `emit_ok_restored` / `emit_ok_parent`), root folder is `null` in every JSON emit, and restore lines are unified on `{"id":N,"restored":true}` | P3 #3 / P4 #8 | Done; nothing left |
| S4 | Resolve the remaining parse-layer inconsistencies: `cmd_args_flag` doc/implementation mismatch (present-vs-absent indistinguishable); `parse_globals` conflates OOM / missing flag value / too-few positionals into one `EXIT_CLI` with the wrong main.c message; bare `--json` (no value) still unparseable after the input-source fix — parser still requires a value (help text no longer documents it) | P1 #1–2 | Small, but the flag API is what W1 and the dead flags keep tripping on |

## Hand-rolled residue (intentional — no `cli_util.h` atom expresses these)

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

## Agentic usage — `--tools` machine-readable tool schema

The review called this the single biggest gap for agentic use: agents
must otherwise scrape help prose and guess the contract. Design rule
from the review: *generate `--tools` from the same per-action data (spec
§11) rather than hand-writing it* — one table drives both the spec doc
and the emitted JSON. *(Resolved by T3 below: the static table in
`src/tools.c` renders the schema with the existing emitter; `--pretty`
implemented, scoped to `--tools`.)*

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| T1 | **Wire-format decisions + spec §11 table** — ✅ *done*: [`cli_spec.md`](cli_spec.md) settles all three decisions (transitions emit `{"id":N,"status":"<s>"}` via `emit_ok_transition`, `set-raw` echoes the unchanged current status; root folder = `null` in every JSON emit; restore = `{"id":N,"restored":true}` via `emit_ok_restored`) and carries the full per-action table for all 10 entities plus the exit-code / error-line contract. A code audit verified the implementation matches the table | P5 #4 / P4 #7–8, Agentic #1–3 | Done; the table is now the generation source for T3 `--tools` |
| T2 | **Error-contract unification** — ✅ *done* (Option A from [`t2_analysis.md`](t2_analysis.md)): one namespace — `ACTA_DB_ERR_*` names for library failures, `ACTA_CLI_ERR`/`code:-10` for every argv/usage error — and the `code`/`exit` invariant restored: `code` = −exit everywhere (`finish_db_error` now prints `code` as `map_rc_to_exit(rc)` negated, so `ACTA_DB_ERR_DUPLICATE`/`FK`/`INVALID_DB` are `code:-4` with exit `4`); all CLI-usage errors (unknown entity/action/option, bad `--verbose`, missing flag value, too few positionals) emit `ACTA_CLI_ERR`/`code:-10`/exit `10` via the new `emit_cli_error` atom (unknown action via the shared `unknown_action`); DB open failure gets its own emit (`emit_db_open_error`) with `code:-11`/exit `11` (`EXIT_DB_OPEN`), making the spec's 11 reachable; `parse_globals` distinguishes OOM (exit 3) / missing flag value / too-few-positionals and the `--verbose` clamp line is VLOG-only. Contract pinned in `tests/cli_util/cli_util_test_error_contract.c`; `cli_spec.md` documents the invariant | P1 #5, Agentic #2 | Done; T3's error-shape documentation is unblocked |
| T3 | **Implement `--tools`** — a static per-(entity, action) data table in one file (e.g. `src/tools.c`) that is the single source of truth; `tools_print` renders it with the existing json emitter. The table covers: command + doc aliases (`execution`, `execution_log` — agents will emit those), description, positionals, flags (incl. `has_value`), input modes (`flags` / `--json` / `--stdin` / `--from_file` + required JSON keys), success stdout schema, exit codes (`0/1/2/3/4/10/11` + raw DB codes), error shape. Also settle dead `--pretty` (implement it or emit tools compact) — ✅ *done*: `src/tools.c` holds the 69-entry table (59 actions + 10 help) and the renderer; `tools_print(FILE*, int pretty)` (no DB); default compact one-line JSON, `--pretty` = 2-space indent, both valid; global section carries the exit-code / error-line contract once (D1); per-entry `aliases` left empty with alias info in `entity_aliases` + descriptions; M1–M3 spec fixes applied, M4/M5 deferred (table follows spec). Audited status + the pretty-mode leading-comma bug (found & fixed) recorded in [`t3_analysis.md`](t3_analysis.md) §0 | Agentic #1, P1 nitpick, Agentic #4–5 | Done; the help line "full command reference" is now true |
| T4 | **Test `--tools`** — ✅ *done*: suite in `acta_cli/tests/tools/tools_test_main.c` (D1–D7 per [`t4_analysis.md`](t4_analysis.md)); compact **and** `--pretty` outputs validated via `json_validate` + cJSON shape walk; 69-entry count and per-entity action coverage from `cli_spec.md`; exactly the 8 `flags|json` entries carry `json_keys`; raw-argv cross-check of all 69 entries (`rc != EXIT_CLI`) plus `--stdin`/`--from_file` smoke runs; green in `make test`. The cross-check root-vs-`tools`-array arg bug found during development is recorded in [`t4_analysis.md`](t4_analysis.md) §0 | Agentic #1, P5 #3 | T1–T3 done; closed the T-chain |

## `--tools` table follow-ups (deferred in T3)

| # | Action | Source | Notes |
|---|--------|--------|-------|
| M4 | **`--all` on `skill list`/`count`** — the real boolean flag (`skill.c:895,1002`; in `entity_flag_specs`; documented in the usage text) is missing from both the `cli_spec.md` flag cells and the `tool_table` (`f_skill_list`/`f_skill_count`), so an agent cannot discover it via `--tools` | T3 audit, [`t3_analysis.md`](t3_analysis.md) §3 | Add `--all` to the two spec flag cells, then to the two table flag arrays; no dispatch change |
| M5 | **`skill_folder move` `--parent_id*`** — spec and table say required, but the code makes it optional (`skill_folder.c:676`: `parse_nonneg_int_flag(..., 0, ...)`, 0/omitted = root) | T3 audit, [`t3_analysis.md`](t3_analysis.md) §3 | Spec cell → `--parent_id` (0 = root); table → `f_parent_id` (optional). `model_folder move` stays required (`model_folder.c:716`) |
| M6 | **Per-entry `aliases`** — all 69 entries carry empty `aliases`; the alias info lives only in the global `entity_aliases` map plus repeated description prose, so the schema never literally says "`execution create` is not a command" | T3 §0 deviation (chosen during implementation) | Optional: fill `aliases` with `["execution"]` on the 10 exec entries and `["execution_log"]` on the 5 log entries |

Maintenance note: `tool_table[].flags` and argparse's `entity_flag_specs` remain two hand-synced
sources of truth — `make test`'s 69× raw-argv cross-check is the drift detector, and for the
M4/M5 cells it can only prove acceptance, not spec-vs-code agreement (see
[`t4_analysis.md`](t4_analysis.md) §4). A shared `has_value` header was considered in
T3 and is not required.

## Residual (from `cli_review.md`, low priority)

| # | Action | Source | Notes |
|---|--------|--------|-------|
| J1 | **JSON layer cleanup** — include cycle `json.c` → `commands.h` (move entity structs to an `entities.h` so `json.c` doesn't include the dispatch header); `jget_int` truncates/wraps (`3.7 → 3`, silent wrap past `INT_MAX`) — range-check it; negative-id `(id > 0) ? id : 0` clamping hides user errors — error instead; `jget_str`/`dup_or_null` conflate "absent" and OOM in the NULL return; opaque `-1` parse errors — `VLOG` the `cJSON_GetErrorPtr()` offset on failure | P2 #5–#10 | Layer under every entity; J1's cycle is the one with real rework cost |
| J2 | **`model get --live` help wording** — "Include soft-deleted rows" is backwards; the flag selects the unfiltered fetch ("fetch even if soft-deleted") | P4 #6 | One line in `model_usage` (+ per-action snippet) |
| J3 | **`*_usage` declarations** — `model_usage`, `ctx_usage`, … are non-static ("the dispatch layer can call this") but declared in no header, so the dispatch layer can't call them; declare them in `commands.h` (enables central `acta <entity> --help`) or make them static | P4 #9 | `db_usage` already resolved as static; 9 entities carry the issue |

## Summary

- **T1–T4 done** (S3 was already closed) — the T-chain is complete:
  spec table (T1), error contract (T2), `--tools` implementation (T3),
  and its contract test (T4; `make test` green, both compact and
  `--pretty` validated, 69-entry count asserted).
- **Next:** S2 (dedicated global-parse suite) and S4 (parse-layer
  inconsistencies) stand independently of the T-chain; then the
  `--tools` table follow-ups M4–M6, and the J1–J3 residual items.

(History of the closed rounds lives in the git log; `cli_review.md` keeps
the full original list.)
