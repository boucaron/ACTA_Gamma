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

`tools_print` (`commands.c`) is a stub that prints `[]`, while help
advertises it as "full command reference". The review calls this the
single biggest gap for agentic use: agents must otherwise scrape help
prose and guess the contract. Design rule from the review: *generate
`--tools` from the same per-action data (spec §11) rather than
hand-writing it* — one table drives both the spec doc and the emitted
JSON.

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| T1 | **Wire-format decisions + spec §11 table** — ✅ *done*: [`cli_spec.md`](cli_spec.md) settles all three decisions (transitions emit `{"id":N,"status":"<s>"}` via `emit_ok_transition`, `set-raw` echoes the unchanged current status; root folder = `null` in every JSON emit; restore = `{"id":N,"restored":true}` via `emit_ok_restored`) and carries the full per-action table for all 10 entities plus the exit-code / error-line contract. A code audit verified the implementation matches the table | P5 #4 / P4 #7–8, Agentic #1–3 | Done; the table is now the generation source for T3 `--tools` |
| T2 | **Error-contract unification** — ✅ *done* (Option A from [`t2_analysis.md`](t2_analysis.md)): one namespace — `ACTA_DB_ERR_*` names for library failures, `ACTA_CLI_ERR`/`code:-10` for every argv/usage error — and the `code`/`exit` invariant restored: `code` = −exit everywhere (`finish_db_error` now prints `code` as `map_rc_to_exit(rc)` negated, so `ACTA_DB_ERR_DUPLICATE`/`FK`/`INVALID_DB` are `code:-4` with exit `4`); all CLI-usage errors (unknown entity/action/option, bad `--verbose`, missing flag value, too few positionals) emit `ACTA_CLI_ERR`/`code:-10`/exit `10` via the new `emit_cli_error` atom (unknown action via the shared `unknown_action`); DB open failure gets its own emit (`emit_db_open_error`) with `code:-11`/exit `11` (`EXIT_DB_OPEN`), making the spec's 11 reachable; `parse_globals` distinguishes OOM (exit 3) / missing flag value / too-few-positionals and the `--verbose` clamp line is VLOG-only. Contract pinned in `tests/cli_util/cli_util_test_error_contract.c`; `cli_spec.md` documents the invariant | P1 #5, Agentic #2 | Done; T3's error-shape documentation is unblocked |
| T3 | **Implement `--tools`** — a static per-(entity, action) data table in one file (e.g. `src/tools.c`) that is the single source of truth; `tools_print` renders it with the existing json emitter. The table covers: command + doc aliases (`execution`, `execution_log` — agents will emit those), description, positionals, flags (incl. `has_value`), input modes (`flags` / `--json` / `--stdin` / `--from_file` + required JSON keys), success stdout schema, exit codes (`0/1/2/3/4/10/11` + raw DB codes), error shape. Also settle dead `--pretty` (implement it or emit tools compact) | Agentic #1, P1 nitpick, Agentic #4–5 | Data-only table — distinct from the excluded V3 field-descriptor + shared-driver refactor. 10 entities × their actions. After this the help line "full command reference" is true. *Current state (audited):* `tools_print` (`commands.c`) still prints `[]`; `--pretty` is still parsed (`argparse.c`) and advertised in help but honored by no emitter — both remain open items here |
| T4 | **Test `--tools`** — output is valid JSON (parse with the project's own json layer); it covers all 10 entities and the full action set (count check); each entry's flags/positionals match what `parse_globals` + dispatch actually accept (ideally by feeding generated commands through the raw-argv path, which also exercises the S2 layer) | Agentic #1, P5 #3 | Depends on T1–T3 |

## Residual (from `cli_review.md`, low priority)

| # | Action | Source | Notes |
|---|--------|--------|-------|
| J1 | **JSON layer cleanup** — include cycle `json.c` → `commands.h` (move entity structs to an `entities.h` so `json.c` doesn't include the dispatch header); `jget_int` truncates/wraps (`3.7 → 3`, silent wrap past `INT_MAX`) — range-check it; negative-id `(id > 0) ? id : 0` clamping hides user errors — error instead; `jget_str`/`dup_or_null` conflate "absent" and OOM in the NULL return; opaque `-1` parse errors — `VLOG` the `cJSON_GetErrorPtr()` offset on failure | P2 #5–#10 | Layer under every entity; J1's cycle is the one with real rework cost |
| J2 | **`model get --live` help wording** — "Include soft-deleted rows" is backwards; the flag selects the unfiltered fetch ("fetch even if soft-deleted") | P4 #6 | One line in `model_usage` (+ per-action snippet) |
| J3 | **`*_usage` declarations** — `model_usage`, `ctx_usage`, … are non-static ("the dispatch layer can call this") but declared in no header, so the dispatch layer can't call them; declare them in `commands.h` (enables central `acta <entity> --help`) or make them static | P4 #9 | `db_usage` already resolved as static; 9 entities carry the issue |

## Summary

- **Next:** T3 (implement `--tools` — T2, its blocker, is done), then
  T4. T1 and T2 are done and S3 is closed; S2 and S4 stand independently
  of the T-chain. Then the J1–J3 residual items.

(History of the closed rounds lives in the git log; `cli_review.md` keeps
the full original list.)
