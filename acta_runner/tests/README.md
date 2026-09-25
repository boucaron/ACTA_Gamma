# acta_runner tests (phase 2)

Layout mirrors `acta_cli/tests/`: one directory per concern, one
binary per suite.

- `tests/stub_server.{h,c}` — in-process stub OpenAI-compatible backend
  (POSIX sockets + pthread, winsock on Windows). Serves exactly what the
  runner calls: `/health`, `/v1/models`, `/v1/chat/completions`.
  Configurable per scenario: health status (200/503), served model id,
  chat status (200/500), chat content, reply delay (timeout tests).
- `tests/run/test_run.c` — pipeline (`run_execution`) against the stub
  on a scratch `:memory:` DB seeded from `acta_db/schema.sql`:
  - success → `completed`, raw_response stored, full phase log
    (execution_started, context_loaded, prompt_resolved, llm_request,
    llm_response, execution_completed)
  - `/health` 503 → `failed` ("model still loading") + `EXIT_HTTP`
  - model mismatch (`/v1/models` id) → `failed` + `EXIT_HTTP`
  - chat 500 → `failed` + `EXIT_HTTP`
  - slow reply + small `--timeout` → `failed` + `EXIT_TIMEOUT`
  - non-pending row → `EXIT_INVALID`, row untouched
  - unknown id → `EXIT_NOT_FOUND`
  - `output_schema` + `supports_response_format: false` → post-hoc
    validation: bad content → `failed` + `EXIT_INVALID`
    (validation_started/validation_failed logged); valid content →
    `completed`

- `tests/run/test_pending.c` — `run --pending` batch loop and `--max`
  clamping (R3), same stub + scratch-`:memory:` harness:
  - N pending → all run, all `completed`, one log sequence per row
  - `--max M < N` → exactly the first M (by `id ASC`) run, the rest stay
    `pending`; a follow-up unbounded batch consumes the remainder
  - `--max 0` → no limit, all run
  - mixed outcomes (model-mismatch failure then success) → batch
    continues, later rows are still processed, exit = worst exit code
    seen (12 = HTTP/preflight; 13 timeout, 4 claim/validation in other
    mixes)
  - no pending rows → clean exit 0

Build & run from `acta_runner/`:

    make test        # builds tests/run/test_run + test_pending +
                     # test_sweep and runs them all (after tests/argparse)
    make test-e2e    # dead-runner end-to-end suite (real child
                     # acta_runner processes; ~10-15 s)
    make clean

Exit code 0 = all checks pass, 1 = at least one failure.

- `tests/argparse/test_argparse.c` — pass-1/pass-2 parsing tests
  (global flag extraction, `--db`/`--verbose` forms, `--version`/`-h`
  early flags, positional skipping, boolean vs value flags,
  `--name=value` inline form, unknown-flag rejection). Run with
  `make test` (runs before the pipeline suite).

- `tests/run/test_sweep.c` — `sweep --stale-seconds` stale-`running`
  cleanup (R4), pure DB + time on a scratch `:memory:` DB (no stub
  server; timestamps backdated via UPDATE, no sleeping):
  - no running rows → clean exit 0
  - fresh `running` row (started_at + logs ≈ now) → left running
  - stale row (≈ now − 2h) → `failed` with the "stale running" error +
    `execution_failed` log row
  - mixed (stale + fresh) → only the stale row swept
  - orphan (stale started_at, no log rows) → swept via the started_at
    fallback
  - newest-wins (fresh started_at, old log) → kept (last activity is
    max(log, started_at))
  - `--stale-seconds 0` / missing / non-numeric → `EXIT_INVALID`
  - inline `--stale-seconds=<n>` form

- `tests/run/test_deadrunner.c` — end-to-end dead-runner recovery
  (real processes, scratch FILE db in `tests/run/`, in-process stub
  server on port 8918; fork/execv/SIGKILL on POSIX,
  CreateProcess/TerminateProcess on Windows). The `:memory:` suites
  above prove the sweep logic; this one proves the real loop:
  - a real `acta_runner run <id>` child claims the execution and blocks
    on the delayed /health preflight call → observed in `running`
  - the child is SIGKILL'd mid-run → row stays stuck in `running`
  - after aging past `--stale-seconds 5` (no timestamp backdating), a
    second real `acta_runner sweep` child exits 0 and transitions the
    row `running → failed` with the exact `stale running: no runner
    activity for 5 s` error + `execution_failed` log row
  - `execution_started` row intact, no `execution_completed` row
  - repeat sweep → exit 0, row unchanged
  Takes ~10-15 s (stale-aging sleep), so it is deliberately NOT in the
  default `test` target: `make test-e2e`.
