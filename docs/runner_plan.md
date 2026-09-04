# Runner — implementation plan

Concrete, file-level plan for the queued runner work. Scope and status per
[`runner_active_action.md`](runner_active_action.md); specs and decisions per
[`runner_analysis.md`](runner_analysis.md). Order: R1 → ~~R2~~ (shipped) →
~~R3~~ (shipped) → ~~R4~~ (shipped) → R5.

---

## R1 — In-app "Run" button (Plan D) in `acta_gamma`

Goal: selected execution row → **Run** action → spawns
`acta_runner run <id>` via `QProcess` → execution panel polls the DB for
live status + phase log → closes UR #18 remainder and #44.

### 1. Where the code lives

- All of R1 belongs in `acta_gamma/src/widgets/executionPanel.{h,cpp}`:
  the panel already owns `m_db` (`ExecutionPanel(db_t *db, QWidget *parent)`),
  the execution tree (`list`), the log table (`logList`) and the
  `showExecutionLogs(QTreeWidgetItem*)` / `reload()` refresh paths.
- `QProcess` + `QTimer` become private members of `ExecutionPanel`.
  No new top-level class; `MainWindow` only owns `DbHandle m_db` and hands
  the handle to the panel.

### 2. Locating and launching the runner

- **Exe lookup order** (at spawn time):
  1. `QApplication::applicationDirPath() + "/acta_runner"` — the dev layout
     is expected to put `acta_runner.exe` next to `acta_gamma.exe`;
     `acta_runner.exe` is currently built in `acta_runner/` and the GUI
     exe is not built yet, so the copy step happens once the GUI build
     lands;
  2. `QStandardPaths::findExecutable("acta_runner")` (PATH fallback);
  3. neither found → message box
     ("acta_runner not found — build it in `acta_runner/` and place it next
     to the app") and do not spawn.
- **DB path:** pass `--db <path>` with the path the GUI is actually using:
  `MainWindow::storedDbPath()` (`QSettings "database/path"`) or, when unset,
  `defaultDbPath()` (`QStandardPaths::writableLocation(AppDataLocation) +
  "/acta.db"`). The runner's own fallback (`$ACTA_DB`, `./acta.db`) must not
  be relied on — its CWD is not the app dir.
- **Arguments:** `run <id>` `--db <path>` (`--timeout` optional passthrough,
  default is fine). Do **not** pass `--api_key` from the GUI: the runner
  already resolves `--api_key` → `$OPENAI_API_KEY` → per-model
  `configuration.api_key`.
- Launch with `QProcess::start()`, not `execute()` (we need
  `finished(int, QProcess::ExitStatus)`).
- **Re-entrancy:** while the process is running, disable the Run button;
  a non-`pending` row is refused by the runner itself (atomic `start()`
  claim, exit 4) — the GUI only needs to enable Run for `pending` rows.

### 3. Polling while the runner is active

- `QTimer` at 1–2 s, started with the process, stopped on `finished`.
- Each tick:
  1. `acta_db_execution_get(m_db, id, &err)` → update that row's status
     column (`pending` → `running` → `completed`/`failed`) in place. A full
     `reload()` is heavier (rebuilds the whole tree); a targeted row update
     preserves selection and scroll position — use the existing
     `acta_db_execution_query`/list pattern in `reload()` but touch only
     the running row, or just call `reload()` at 1–2 s (acceptable at this
     scale; prefer the targeted update).
  2. `acta_db_execution_log_list_by_execution(m_db, id, …)` → refresh
     `logList` (the same call `showExecutionLogs()` already makes), phase
     rows: `execution_started`, `context_loaded`, `prompt_resolved`,
     `llm_request`, `llm_response`, `validation_*`,
     `execution_completed/failed`.
  3. Auto-scroll the log table to the newest row (UR #44): after refresh,
     select the last log row and `logList->scrollTo(lastRowIndex)` (Qt has
     no `scrollToBottom` on `QTableView`).
  4. Progress indicator while `status == running`: simplest is a "…"
     suffix on the status column or a small busy `QLabel` in the panel
     header; decide against the existing widgets at implementation time.
- WAL already supports concurrent GUI reader + runner writer — no extra
  sync. If the DB is locked (`SQLITE_BUSY`), the poll just skips the tick
  (log via `qWarning`, retry next tick).

### 4. Finishing

- On `QProcess::finished`: stop the timer, final row + log refresh.
  - exit 0 → `completed`; nothing more to do.
  - non-zero → surface the failure. The runner prints a single-line JSON
    error on stderr: `{"error":"ACTA_RUNNER_ERROR","code":<n>,"message":...}`
    for non-DB failures (`ACTA_DB_ERR_*` contract for DB failures). Parse
    the last stderr line with `QJsonDocument`; if it parses, show
    `message`; otherwise show raw stderr. The execution row is already
    `failed` with the error in the DB, so the UI display is purely
    informational (status bar line or small non-modal dialog, matching
    P4/UR #36 conventions).

### 5. UI wiring

- Run affordance: third button in the Execution panel's existing icon-only
  toolbar row (`newExecutionBtn`, `showDetailsBtn` — P2 convention of one
  icon-only row per panel), e.g.
  `style()->standardIcon(QStyle::SP_DialogOpenButton)` variant or
  `SP_MediaPlay`, tooltip `tr("Run the selected execution")`.
- Enable state: selection exists && row status is `pending` && no active
  `QProcess`. Re-evaluated in the selection handler and on each poll tick.
- All new literals wrapped in `tr()` (UR #43); new strings land in
  `translations/acta_gamma.ts`.

### 6. Files touched (expected)

- `acta_gamma/src/widgets/executionPanel.h/.cpp` — `QProcess* m_runner`,
  `QTimer* m_pollTimer`, slots `onRunBtnClicked()`,
  `onRunnerFinished()`, `onPollTick()`, Run button + enable logic,
  targeted status/log refresh.
- No runner-side changes.
- Docs: drop #18 remainder + #44 from `ui_review.md`; update
  `ui_active_action.md` Summary; update `runner_analysis.md`
  "Remaining work" (mark Plan D shipped).

### 7. Testing

- Manual E2E: GUI against a live local `llama-server` — create execution,
  Run, watch `running` + phase log rows appear and auto-scroll,
  `completed` on success; failure paths (503, wrong model id) show the
  JSON error message; Run disabled while running; re-run refused.
- Headless check: run `acta_runner run <id>` manually while the GUI polls
  the same DB (concurrent reader/writer sanity, WAL).

---

## R2 — `argparse` pass-1/pass-2 test suite — SHIPPED (51e375c)

Shipped as `tests/argparse/test_argparse.c` (plain asserts, no DB, no
stub server); `make test` runs it before the pipeline suite. Since
51e375c the suite has grown to 47 checks (extra clamp/inline-value/
remainder edge cases) and is green; the pipeline suite is green as well
(47 checks).
Companion build fixes for the phase-2 suite on mingw (sock_read/recv,
strncasecmp header lookup, test-local `runner_gopts`): 5616500.

What the suite pins:
- **Pass 1** (`parse_globals`): `--db x` / `--db=x`; `--db` missing value
  → `EXIT_CLI`; globals-only / no args → `EXIT_CLI`; `-v` stackable,
  clamped at 3; `--verbose=2` / `=7` (clamp) / `=abc` → `EXIT_INVALID`;
  `--version` (returns 0 with `show_version` set, no remainder); `-h`;
  global extracted mid-line; unknown global passes through as the action.
- **Pass 2** (`cmd_args_*`, action token excluded — main.c inits with
  `gopts.argv + 1`): positional extraction and exhaustion; `--pending`
  bool leaves no positional and doesn't eat the following token;
  `--max <n>` / `--max=n` values; `--timeout`/`--api_key` values +
  positional after them; value flag doesn't eat the following flag;
  `cmd_args_validate` on known flags, unknown flag, `--bogus=x`, no flags,
  `--pending=x`.

Also fixed: the stale `parse_globals` doc in `include/argparse.h` (claimed
`EXIT_CLI` for `--version/--help`; the implementation returns 0 with the
flag set).

**Decided (code updated):** the help text in `main.c`/`run.c`
documented `--api-key <key>`, but the flag table in `argparse.c` and
`cmd_run` use `api_key` (underscore); a user typing the documented
`--api-key` is rejected by `cmd_args_validate` as an unknown option.
Decision: keep BOTH sources — the flag as an explicit override, the env
var as the default (plus the per-model `configuration.api_key` fallback,
so the chain stays `--api_key` → `$OPENAI_API_KEY` →
`configuration.api_key`, per decision 4 in `runner_analysis.md`). Env
var as the primary source: OpenAI/llama.cpp convention, stays out of
shell history/Makefiles/scripts; the flag covers one-off overrides, local
stubs with dummy keys, and CI. The flag name stays `api_key` (no rename
to `api-key`): the flag table, `cmd_run` handler, and the argparse tests
already agree on the underscore form, so the mismatch is fixed by
rewording the help text in `main.c` and `run.c` to `--api_key <key>`.

---

## R3 — `--pending` batch and `--max` clamping tests — SHIPPED (4967a78)

Shipped as `tests/run/test_pending.c` (same scratch-`:memory:` DB + stub
server harness as `test_run.c`; `make test` runs it after the pipeline
suite; 33 checks, green). The run exposed one `cmd_run` bug — the
lister returns NULL for an empty result, so `run --pending` with zero
pending rows exited with an `execution_query` error instead of clean
exit 0; fixed in 7ed1794 (same fix for the test's `count_status`
helper).

What the suite pins:
- 3 pending, no `--max` → all run, all `completed`, one log sequence per
  row, no pending left, exit 0.
- 4 pending, `--max 2` → exactly the first two (by `id ASC`) run, the
  other two stay `pending`; a follow-up unbounded batch consumes them.
- 3 pending, `--max 0` → no limit, all run.
- mixed outcomes (model-mismatch failure first by id, then success) →
  batch continues, the later row is still processed, exit = worst exit
  code seen (12 = HTTP/preflight).
- no pending rows → clean exit 0.

Note: `--max` is applied at the lister (`acta_db_execution_query` limit),
not in the loop; the single stub serves one configuration per scenario,
so the failing row uses a model-identifier mismatch (same EXIT_HTTP
class as the health-503 case).

Goal: cover the `run --pending` loop (claim next `pending` via the atomic
`start()`, run, record) and `--max`.

- Extend `tests/run/test_run.c` (or a new `tests/run/test_pending.c`,
  same harness: scratch `:memory:` DB + stub server):
  - N pending rows → all run, all `completed`, one log sequence per row.
  - `--max M < N` → exactly M run, the rest stay `pending`.
  - `--max 0` → no limit, all run.
  - mixed outcomes (one health 503, one success) → batch continues and
    exits with the worst exit code (12 HTTP/preflight, 13 timeout, 4
    claim/validation); document that later rows are still processed.
  - no pending rows → clean exit 0.

---

## R4 — Stale-`running` cleanup sweep (`--stale-seconds`) — SHIPPED

Goal: per decision 6, recover rows stuck in `running` after a dead runner.

Shipped (commit 8c4d6cd) as `acta_runner/src/sweep.c` (`acta_runner sweep
--stale-seconds N`, dispatched next to `run` in `main.c`;
`--stale-seconds` registered in `argparse.c`) with
`tests/run/test_sweep.c` (36 checks, run last by `make test`).
Implemented semantics (where the plan left room):

- Last runner activity per row = **max**(latest `execution_log.created_at`,
  `started_at`), falling back to `created_at` — a live runner's row always
  has a fresh max, so a row claimed within the last N seconds is never
  swept.
- `--stale-seconds` must be a **positive** integer; `0` is rejected
  (`EXIT_INVALID`) — a zero-second sweep would fail every `running` row.
- If a row leaves `running` between the query and `fail()` (a live runner
  finished in the meantime), the sweep skips it rather than overwriting
  the outcome; `execution_failed` is logged only when the fail succeeds.
- Exit 0 when nothing is running or every row was swept; the initial
  query failure exits per the standard DB error contract.

- New action: `acta_runner sweep --stale-seconds N` (separate action keeps
  the `run` path pure; dispatched next to `run` in `main.c`).
- Algorithm:
  1. `acta_db_execution_query` for `status = running`.
  2. For each row, take the latest `execution_log` timestamp
     (`acta_db_execution_log_list_by_execution`); a row that died before
     logging anything is unreachable, but that cannot happen —
     `execution_started` is logged immediately after `start()`.
  3. If now − last log timestamp > N seconds →
     `acta_db_execution_fail(db, id, "stale running: no runner activity
     for N s")` + `execution_failed` log row.
- Semantics to pick and document: `--stale-seconds 0` (all `running` are
  stale) vs. required `> 0`.
- Exit codes: 0 when nothing swept or all swept; use the runner's existing
  `runner_error`/`map_rc_to_exit` contract for DB failures.
- Tests (scratch `:memory:` DB, no HTTP): insert `running` rows with old
  log timestamps → swept to `failed`; fresh `running` row left alone;
  `N = 0` case per the chosen semantics.

---

## R5 — JSON validation (analysis first, then UI)

Goal (H2 / UR #15): validate `output_schema` and model `configuration`
with clear "invalid JSON" feedback.

### Analysis (decision, then implement)

- `model.configuration`: JSON object (keys `api_key`, `temperature`,
  `max_tokens`, `top_k`, `supports_response_format` per phase 2) →
  **validate**, must be a JSON *object*.
- `skill.output_schema`: JSON Schema → **validate**, must be a JSON
  *object*.
- `context.content`: may be plain text → **do not validate as JSON**.

### Implementation

- `QJsonDocument::fromJson` on the field text; on parse error show
  `QJsonParseError` message with line/column — inline warning label, plus a
  "Validate JSON" affordance.
- Exact widgets (verified):
  - `ModelDialog::ui->configurationTextEdit` (`modelDialog.cpp`, save path
    at the `m.configuration = dupString(...)` sites, both create and edit);
  - `SkillDialog::ui->outputSchemaTextEdit` (`skillDialog.cpp`, save path at
    the `s.output_schema = ...` site).
- Block save on invalid JSON in both dialogs (creation + edit paths),
  matching the existing error-handling style (P4). Empty fields stay
  allowed (both fields are optional today — `dupString` gets `nullptr`
  on empty text).
- `tr()`-wrap all new strings; add to `translations/acta_gamma.ts`.

---

## Sequencing and documentation hygiene

1. **R1** (High; unblocks the full user story) → commit; drop UR #18
   remainder + #44 from `ui_review.md`, fold into `ui_active_action.md`
   Summary, mark Plan D shipped in `runner_analysis.md`.
2. ~~**R2**~~ — shipped (51e375c); `runner_analysis.md` "Remaining work"
   updated. Its open decision — the `--api_key`/`--api-key` help-text
   mismatch (R8) — shipped as d90e23e: flag stays `api_key`, help text
   fixed to `--api_key <key>`.
3. ~~**R3**~~ — shipped (4967a78, plus the `cmd_run` empty-result fix
   7ed1794); `runner_analysis.md` "Remaining work" updated.
4. **R4** → decision 6 marked implemented in `runner_analysis.md`.
5. **R5** → #15 closed in `ui_review.md` / `ui_active_action.md`.
6. **R6** (JSON highlighting / line numbers, UR #26) and **R7**
   (housekeeping: fate of untracked `docs/llamacpp_server_README.md`,
   `.gitignore` for build outputs) whenever convenient.
