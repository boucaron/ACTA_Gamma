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

### High

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| R1 | **In-app "Run" button (Plan D)** in `acta_gamma`: "Run" action on the selected execution row spawns `acta_runner run <id>` via `QProcess`; the execution panel polls `acta_db_execution_query` + `execution_log` rows for live status and phase log | UR #18 remainder + #44; Plan D in `runner_analysis.md` | runner exists standalone (`acta_runner/`); the DB is the message bus (no IPC, WAL supports concurrent readers/writers); also delivers UR #44 live-status UX |

Details for R1:

- Locate the `acta_runner` exe (next to the app binary, or via PATH); pass
  `--db` and, if configured, `--api_key`.
- While the process is active: poll the execution row and its `execution_log`
  rows (interval a few seconds is enough at this scale); update the status
  column (`pending` → `running` → `completed`/`failed`) and the log table.
- Progress indicator in the panel while `running`; log list auto-scrolls to
  the newest row (UR #44).
- On `QProcess` finished: stop polling; if the exit code is non-zero, show the
  runner's JSON error line / exit code.
- Re-entrancy guards: disable "Run" while a process is active; ignore rows
  that are not `pending` (the runner itself refuses them atomically via
  `start()`).

### Medium (runner hardening)

| # | Action | Source | Notes |
|---|--------|--------|----------------------|
| R3 | Tests for `run --pending` batch looping and `--max` clamping | `runner_analysis.md` "Remaining work" | includes "worst exit code wins" for the batch |
| R4 | Stale-`running` cleanup sweep: `--stale-seconds N` finds `running` rows whose last `execution_log` timestamp is older than N and `fail()`s them with a dead-runner error | decision 6 in `runner_analysis.md` | pure DB + time; easy to test on a scratch DB |

### Low

| # | Action | Source | Notes / dependencies |
|---|--------|--------|----------------------|
| R5 | **JSON validation (to analyze)** for `output_schema` and model `configuration` (context `content` likely excluded — it may be plain text) | H2 in `ui_active_action.md` / UR #15 | analyze scope first; then `QJsonDocument::fromJson` with a clear "invalid JSON" message in the dialogs/panel editor, at minimum a "Validate JSON" button |
| R6 | UR #26 remaining: JSON highlighting / line numbers in the editor | `ui_review.md` Polish | independent polish |
| R7 | Housekeeping: decide fate of untracked `docs/llamacpp_server_README.md` (referenced by `runner_analysis.md` — commit or delete); consider `.gitignore` for build outputs | working tree | — |

## Summary

- **Now:** R1 (in-app "Run" button / Plan D) — the only remaining High action;
  it unblocks the full user story (create → run → live status) and closes
  UR #18 remainder and #44.
- **Shipped:** R2 (argparse pass-1/pass-2 test suite, `tests/argparse/test_argparse.c`,
  commit 51e375c — 47 checks as extended, `make test` runs it before the
  pipeline suite;
  also fixed the stale `parse_globals` doc in `include/argparse.h` and the
  mingw build of the phase-2 suite, commit 5616500).
  R8 (kept the `api_key` flag as-is; fixed the help-text mismatch in
  `main.c`/`run.c` to `--api_key <key>` — no rename to `api-key`,
  commit d90e23e).
- **Then:** R3–R4 (runner test coverage + stale sweep), R5 (JSON validation,
  analyze first), R6–R7 (polish / housekeeping).
- After shipping each item: drop it from the open lists in
  `ui_review.md` / `runner_analysis.md` and fold it into the Summary, per the
  repo's shipped-item convention.
