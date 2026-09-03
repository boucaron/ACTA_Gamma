# acta_runner tests (phase 2)

Layout mirrors `acta_db_cli/tests/`: one directory per concern, one
binary per suite.

- `tests/stub_server.{h,c}` — in-process stub OpenAI-compatible backend
  (POSIX sockets + pthread, winsock on Windows). Serves exactly what the
  runner calls: `/health`, `/v1/models`, `/v1/chat/completions`.
  Configurable per scenario: health status (200/503), served model id,
  chat status (200/500), chat content, reply delay (timeout tests).
- `tests/run/test_run.c` — pipeline (`run_execution`) against the stub
  on a scratch `:memory:` DB seeded from `acta_gamma/db/schema.sql`:
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

Build & run from `acta_runner/`:

    make test        # builds tests/run/test_run and runs it
    make clean

Exit code 0 = all checks pass, 1 = at least one failure.

- `tests/argparse/test_argparse.c` — pass-1/pass-2 parsing tests
  (global flag extraction, `--db`/`--verbose` forms, `--version`/`-h`
  early flags, positional skipping, boolean vs value flags,
  `--name=value` inline form, unknown-flag rejection). Run with
  `make test` (runs before the pipeline suite).

Still planned (not yet implemented):

- Coverage of `--pending` batch looping and `--max` clamping
  (the batch loop itself is exercised by hand until a dedicated suite
  lands).
