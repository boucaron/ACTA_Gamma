# T2 — Error-contract unification: analysis

Analysis of the `code`/`exit` invariant and the error namespace, grounding
the T2 item in [`cli_active_action.md`](cli_active_action.md) and the
error-line contract in [`cli_spec.md`](cli_spec.md). Source: P1 #5 and
Agentic #2 in [`cli_review.md`](cli_review.md).

Goal (from the plan): one namespace, and a restored `code`/`exit`
invariant, so `--tools` (T3) can document the error shape truthfully.

## 1. Full inventory of error paths today

### CLI layer (parse/dispatch)

| Path | Source | `error` name | `code` | exit | Problem |
|------|--------|--------------|--------|------|---------|
| OOM in `parse_globals` | `argparse.c:32` | (none; main prints) `ACTA_DB_ERR_INVALID` | −4 | 4 | OOM reported as "missing entity and/or action" |
| Missing flag value (`--db`/`--fields`/`--json`/`--from_file`) | `argparse.c:67,76,127,138` | `ACTA_DB_ERR_INVALID` | −4 | 4 | Misleading message; no flag named |
| Fewer than 2 positionals | `argparse.c:151` | `ACTA_DB_ERR_INVALID` | −4 | 4 | Same lumped message |
| Invalid `--verbose=N` | `argparse.c:107` | `ACTA_CLI_ERR` | −10 | **4** | code/exit mismatch |
| Unknown entity | `commands.c:54` | `ACTA_CLI_ERR` | −10 | **10** | consistent; pinned by `tests/cli_util/cli_util_test_dispatch.c` |
| Unknown option (pass 2) | `argparse.c:294` | `ACTA_CLI_ERR` | −10 | **4** | code/exit mismatch; pinned by `tests/model/model_test_deleted.c` / `tests/skill/skill_test_deleted.c` (`TEST_EQ rc, EXIT_INVALID`) |
| `--verbose 99` clamp | `argparse.c:133` | plain text, no JSON | — | 0 (continues) | non-JSON diagnostic outside the contract |
| Unknown action (all 10 entities) | `cli_util.h:498` (`unknown_action`) + per-entity `finish_db_error` | `ACTA_DB_ERR_INVALID` | −4 | **4** | `cli_spec.md` legend assigns "unknown entity/action" to exit **10** |

### DB layer

| Path | Source | `error` | `code` | exit | Problem |
|------|--------|---------|--------|-------|---------|
| DB open failure | `main.c:105` (`cli_error(db_err, …)`) | `ACTA_DB_ERR_SQL` or `ACTA_DB_ERR_INVALID_DB` | −2 / −5 | 2 / 4 | **Spec exit `11` ("DB open failed") is phantom — `EXIT_DB_OPEN` is defined in `cli.h:18` but returned nowhere, no test covers the path** |
| Not found | `emit_not_found` | `ACTA_DB_ERR_NOT_FOUND` | −1 | 1 | ✓ |
| SQL | `finish_db_error` | `ACTA_DB_ERR_SQL` | −2 | 2 | ✓ |
| OOM | `finish_db_error` | `ACTA_DB_ERR_ALLOC` | −3 | 3 | ✓ |
| Argument validation | cli_util atoms | `ACTA_DB_ERR_INVALID` | −4 | 4 | ✓ (matches spec "4") |
| Duplicate (create) | `finish_op_error` | `ACTA_DB_ERR_DUPLICATE` | **−6** | 4 | code/exit mismatch |
| FK violation | `finish_op_error` | `ACTA_DB_ERR_FK` | **−7** | 4 | code/exit mismatch |
| Invalid DB file | open path | `ACTA_DB_ERR_INVALID_DB` | **−5** | 4 | code/exit mismatch |

## 2. Inconsistency clusters

1. **Four different encodings for one class.** "CLI usage error" (per the
   spec's legend: *unknown entity/action, too few positionals, bad input
   source*) is currently encoded as: `ACTA_CLI_ERR`/−10/exit 10 (unknown
   entity), `ACTA_CLI_ERR`/−10/exit 4 (unknown option, bad `--verbose`),
   `ACTA_DB_ERR_INVALID`/−4/exit 4 (too few positionals, missing flag
   value), and `ACTA_DB_ERR_INVALID`/−4/exit 4 (unknown action).
2. **`|code| == exit` holds only for −1/−2/−3/−4** (and −10/10 for the one
   consistent CLI path). Failing: unknown option, bad `--verbose`,
   DUPLICATE, FK, INVALID_DB.
3. **Spec vs code:** the spec's exit 10 covers "unknown entity/**action**"
   — the code gives action exit 4. The spec's exit **11 is unreachable**.
4. **Test pins** to coordinate with:
   - `cli_util_test_dispatch.c` — unknown entity → `code:-10`, exit
     `EXIT_CLI` (10) — *stays valid under any option*
   - `model_test_deleted.c`, `skill_test_deleted.c` — unknown option → exit
     `EXIT_INVALID` (4) — *would flip to 10*
   - DUPLICATE/FK tests — pin exit 4 only, **not** the `code` field —
     *cheap either way*
   - ~10 entity "unknown action" tests — pin exit 4 — *flip only if
     unknown action moves to 10*
   - No tests touch the db-open-failure path or the `code` −5/−6/−7 values

## 3. Decision options

### Option A — exit is canonical; `code = -exit`; `error` name keeps granularity

- `finish_db_error` prints `code` as `map_rc_to_exit(rc)` negated, keeping
  the raw `ACTA_DB_ERR_*` name: DUPLICATE lines become
  `{"error":"ACTA_DB_ERR_DUPLICATE","code":-4}` — one-line change in
  `finish_db_error`, zero impact on pinned tests.
- All CLI-usage errors → `ACTA_CLI_ERR`, `code:-10`, exit 10: add one
  `emit_cli_error` atom in `cli_util.h`, replace the three hand-rolled
  `fprintf` sites; `parse_globals` must distinguish its three error classes
  (OOM → exit 3 with its own message; missing flag value → exit 10 naming
  the flag; too few positionals → exit 10) — this overlaps S4's
  "conflation" fix, so do it in one pass.
- Unknown action → `ACTA_CLI_ERR`/−10/10 (matches the spec legend) via
  `emit_cli_error` + `unknown_action` returning `EXIT_CLI`; updates ~10
  entity call sites and their unknown-action tests.
- DB open failure → dedicated emit:
  `{"error":"ACTA_DB_ERR_INVALID_DB"|"ACTA_DB_ERR_SQL","code":-11}`, exit
  `EXIT_DB_OPEN` (11) — makes the spec's 11 real; add a test (none exists).
- Drop (or VLOG-only) the plain-text `--verbose` clamp line.

**Result:** `|code| == exit` holds everywhere; all seven documented exits
(0/1/2/3/4/10/11) are reachable; one namespace per layer (DB names for db
errors, `ACTA_CLI_ERR` for argv errors); the spec legend becomes exactly
true. Cost: moderate diff, ~12–15 test files touched, the spec caveat
flipped ("exit field is authoritative; `code` = −exit; name keeps
granularity").

### Option B — code (raw rc) stays canonical; extend the exit table

- Fix only the mismatches: unknown option and bad `--verbose` → exit 10;
  too-few-positionals/missing-flag-value → `ACTA_CLI_ERR`/−10/10 (same
  `parse_globals` class-split as A); db open failure → exit 11.
- DUPLICATE/FK: either add new exit codes 6/7/8 (spec table grows; agents
  learn three more codes) or keep exit 4 and explicitly document "exit is
  coarse, `code` is fine" — i.e. **give up the invariant**.
- Unknown action: keep −4/4 and amend the spec legend (cheapest), or move
  to 10 as in A.

**Result:** raw rc preserved on the wire; smallest real diff for the two
`ACTA_CLI_ERR` mismatches. But T2's stated goal — "restore the
`code`/`exit` invariant" — is only half met, and the 6/7/8 question is a
new open decision.

## 4. Recommendation

**Option A.** Rationale:

- Agents branch on **exit** (shell) and **error name** (parsing); the raw
  `ACTA_DB_ERR_*` rc is db-library internals that leak onto the wire by
  accident. Keeping the distinction in the `error` name costs nothing.
- The spec's exit legend (0/1/2/3/4/10/11) already exists; Option A is the
  only option that makes every documented exit reachable and makes the
  legend byte-for-byte true, including killing the phantom 11.
- Cheapest possible core change (`finish_db_error` prints `code` from
  `map_rc_to_exit`), and the test churn is small because no test pins the
  `code` field except the one `code:-10` dispatch test, which stays valid.
- The `parse_globals` class-split it requires is literally S4's second
  item — doing T2 and S4 in one pass avoids touching the same lines twice.

## 5. Concrete change list (Option A)

1. `cli_util.h`: new `emit_cli_error(msg)` atom (prints the
   `ACTA_CLI_ERR`/−10 line, returns `EXIT_CLI`); `finish_db_error` prints
   `code` as `map_rc_to_exit(rc)` negated; `unknown_action` returns
   `EXIT_CLI`.
2. `argparse.c`: bad `--verbose` and `cmd_args_validate` unknown option →
   `emit_cli_error`; `parse_globals` distinguishes OOM (exit 3) / missing
   flag value (names the flag, exit 10) / too-few-positionals (exit 10) and
   emits its own JSON line; drop the plain-text clamp (→ `VLOG(3)`).
3. `main.c`: the `rc == EXIT_CLI` branch just returns (parse_globals
   already emitted); db-open failure → new emit with `code:-11`, return
   `EXIT_DB_OPEN`.
4. Entity files (~10): unknown-action JSON line switches from
   `finish_db_error(ACTA_DB_ERR_INVALID, …)` to `emit_cli_error(…)`.
5. Tests: `model_test_deleted.c`, `skill_test_deleted.c` (unknown option →
   `EXIT_CLI`), ~10 unknown-action tests (→ `EXIT_CLI`), add a
   db-open-failure test.
6. Docs: `cli_spec.md` exit-code/error section — replace the "until then
   the `code` field is authoritative" caveat with the invariant; close T2
   in `cli_active_action.md` (and P1 #5 in `cli_review.md`).
