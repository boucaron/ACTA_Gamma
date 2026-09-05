# Active Actions — from `cli_review.md`

Action plan derived from [`cli_review.md`](cli_review.md). The earlier
`db`-surface round closed its in-scope items; what remained there (F2, F3)
was folded into the plan below (F3 → S2, F2 → S3).

## Structural (highest long-term payoff)

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| S2 | **Test the global parse layer** — raw `argv` through `parse_globals` + `commands_dispatch` (in-process or spawning the binary). Every bug in the P2 class (`--json` blob ignored, `--stdin` eaten, bare `--json` unparseable, dead local `--count`) lives in this untested seam (former F3) | P5 #3 | the `stest_run_argv` helper (`tests/helpers/test_helpers`) is already the in-process raw-argv path used by the P2 input-source regression cases — a dedicated suite is the missing piece (the earlier orphaned `tests_parse_globals.c` seed was deleted in `d31ce98`; the helper above is the starting point) |
| S3 | **Per-action stdout-schema table in the spec** — success shapes are now emitted through the shared atoms (`emit_ok_id` / `emit_ok_folder` / `emit_deleted` in `cli_util.h`), so the shared-emit-helper half is subsumed; what remains is documenting the per-action success shapes in the spec (`db`'s shapes in `db_usage()` are the template) and settling the root-folder wire representation (`null` in `model_to_json` vs `0` in move success lines), and the restore-shape drift (`model_folder restore` → hand-rolled `{"id":N,"restored":true}` vs `{"id":N}` on `model`/`skill`/`skill_folder` restore) | P3 #3 / P4 #8 | Cross-entity; db row is the template |
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

## Residual (from `cli_review.md`, low priority)

| # | Action | Source | Notes |
|---|--------|--------|-------|
| J1 | **JSON layer cleanup** — include cycle `json.c` → `commands.h` (move entity structs to an `entities.h` so `json.c` doesn't include the dispatch header); `jget_int` truncates/wraps (`3.7 → 3`, silent wrap past `INT_MAX`) — range-check it; negative-id `(id > 0) ? id : 0` clamping hides user errors — error instead; `jget_str`/`dup_or_null` conflate "absent" and OOM in the NULL return; opaque `-1` parse errors — `VLOG` the `cJSON_GetErrorPtr()` offset on failure | P2 #5–#10 | Layer under every entity; J1's cycle is the one with real rework cost |
| J2 | **`model get --live` help wording** — "Include soft-deleted rows" is backwards; the flag selects the unfiltered fetch ("fetch even if soft-deleted") | P4 #6 | One line in `model_usage` (+ per-action snippet) |
| J3 | **`*_usage` declarations** — `model_usage`, `ctx_usage`, … are non-static ("the dispatch layer can call this") but declared in no header, so the dispatch layer can't call them; declare them in `commands.h` (enables central `acta <entity> --help`) or make them static | P4 #9 | `db_usage` already resolved as static; 9 entities carry the issue |

## Summary

- **Next:** S2–S4, then the J1–J3 residual items.

(History of the closed rounds lives in the git log; `cli_review.md` keeps
the full original list.)
