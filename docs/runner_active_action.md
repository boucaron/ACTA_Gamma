# Active Actions — runner (from `runner_analysis.md`)

Action plan for the runner workstream. Items reference
[`runner_analysis.md`](runner_analysis.md) (remaining work, decisions) and
[`ui_review.md`](ui_review.md) / [`ui_active_action.md`](ui_active_action.md)
for the UI side (UR #18 remainder, #44, #15).

## Status

**Phase 2 (runner backend) — shipped** (`acta_runner/`, commit d142a8e):

- `src/backend.{h,c}` — curl wrapper (GET/POST JSON, timeout,
  `Authorization: Bearer`, result codes
  `BACKEND_OK / ERR_TRANSPORT / ERR_TIMEOUT / ERR_ALLOC`).
- `src/run.c` — full `run_execution` pipeline: claim (`start()`) → resolve
  (context / skill revision / model revision) → preflight (`/health`,
  `/v1/models` id check) → `POST /v1/chat/completions` →
  `set_raw_response` → optional post-hoc validation → `complete()` /
  `fail()`, one `execution_log` row per phase.
- `tests/` — in-process stub OpenAI server + 9-scenario pipeline test
  suite, green (47 checks) on a scratch `:memory:` DB (`make test` in
  `acta_runner/`); post-hoc validation parses the raw response JSON and
  logs `validation_started` / `validation_failed`.
- Decisions finalized (f1abce0); docs updated (78325f9).

## Queued actions

### Low

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| R5 | **JSON validation (to analyze)** for `output_schema` and model `configuration` (context `content` likely excluded — it may be plain text) | H2 in `ui_active_action.md` / UR #15 | analyze scope first; then `QJsonDocument::fromJson` with a clear "invalid JSON" message in the dialogs/panel editor, at minimum a "Validate JSON" button |
| R6 | UR #26 remaining: JSON highlighting / line numbers in the editor | `ui_review.md` Polish | independent polish |
| R7 | Housekeeping: decide fate of untracked `docs/llamacpp_server_README.md` (referenced by `runner_analysis.md` — commit or delete); consider `.gitignore` for build outputs | working tree | — |

## Summary

- **Now:** R5 (JSON validation, analyze first), then R6–R7
  (polish / housekeeping).
- **Shipped:** R1 (in-app "Run" button / Plan D in `acta_gamma` — Run button
  + context-menu entry on the selected execution row spawn
  `acta_runner run <id> --db <path>` via `QProcess`, exe located next to the
  app binary then PATH; 1.5 s DB poll updates the status cell in place
  (`running…`) and refreshes the `execution_log` table with auto-scroll to
  the newest row; on non-zero exit the runner's single-line JSON stderr is
  parsed and shown; `setDb(db, path)` kills an active runner on DB switch;
  commit 184d574 — closes UR #18 remainder and #44). R2 (argparse pass-1/pass-2 test suite, `tests/argparse/test_argparse.c`,
  commit 51e375c — 47 checks as extended, `make test` runs it before the
  pipeline suite;
  also fixed the stale `parse_globals` doc in `include/argparse.h` and the
  mingw build of the phase-2 suite, commit 5616500).
  R8 (kept the `api_key` flag as-is; fixed the help-text mismatch in
  `main.c`/`run.c` to `--api_key <key>` — no rename to `api-key`,
  commit d90e23e).  R3 (`run --pending` batch + `--max` clamping test
  suite, `tests/run/test_pending.c`, commit 4967a78 — the run exposed a
  `cmd_run` bug, the lister returns NULL for an empty result: fixed in
  7ed1794).  R4 (stale-`running` sweep: `acta_runner sweep
  --stale-seconds N` in `src/sweep.c` + `tests/run/test_sweep.c`, 36
  checks, commit 8c4d6cd — last activity is max(latest `execution_log`
  timestamp, `started_at`); `--stale-seconds` must be a positive
  integer).
- After shipping each item: drop it from the open lists in
  `ui_review.md` / `runner_analysis.md` and fold it into the Summary, per the
  repo's shipped-item convention.
