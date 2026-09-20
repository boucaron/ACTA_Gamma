# Runner analysis

Analysis and spec of the `acta_runner` component: a
component that connects to the database, resolves a pending execution's
skill + model + context, calls the OpenAI-compatible backend, and records
the outcome (raw response, result, error, phase logs) back into the
database.

## Status: phase 2 shipped, in-app Run shipped, R8 shipped

Phase 2 is implemented in `acta_runner/` (commit d142a8e). What landed:

- `src/backend.{h,c}` — curl wrapper exactly as specced: one GET/POST JSON
  request with timeout, optional `Authorization: Bearer`, result codes
  `BACKEND_OK / ERR_TRANSPORT / ERR_TIMEOUT / ERR_ALLOC / ERR_CANCELED`
  (cooperative UI cancel; the CLI never triggers it — the timeout
  `CURLcode` is selected by `LIBCURL_VERSION_NUM`, curl ≥ 8.8 renamed
  `CURLE_OPERATION_TIMED` to `CURLE_OPERATION_TIMEDOUT`).
- `src/run.c` — the full `run_execution` pipeline: claim (`start()`),
  resolve (context / skill revision / model revision), preflight
  (`/health`, `/v1/models` id check), `POST /v1/chat/completions`,
  `set_raw_response`, optional post-hoc validation, `complete()`/`fail()`,
  with one `execution_log` row per phase and JSON metadata
  (http status, latency, token usage, …). Every post-claim failure
  funnels through `fail_execution()` so a row never stays stuck in
  `running`.
- R8 (commit 2f8085e): preflight catalog logging — after the
  `/v1/models` id match, the runner fetches the llama.cpp model catalog
  (`GET /`) and logs a `preflight_passed` event with the matched entry's
  server-instance configuration (`status.args`, `meta`: `n_ctx`,
  `n_params`, `size`, `ftype`) plus `max_context`. Best-effort: a missing
  catalog records `"catalog":null` and never fails the execution; the
  stub server serves `GET /` with a canned catalog
  (`catalog_status`), and `test_run.c` scenario 10 covers the missing-
  catalog path. Tests green, no regressions.
- `tests/` — in-process stub OpenAI server (`tests/stub_server.{h,c}`,
  POSIX sockets + pthread / winsock) and `tests/run/test_run.c`: 13
  scenarios (success, health 503, model mismatch, chat 500, timeout,
  non-pending, not-found, schema validation fail/pass, missing
  catalog, unknown config key, malformed config JSON, wrong config
  key type), check count printed at runtime, green under `make test`,
  on a scratch `:memory:` DB. Batch/claim/cleanup suites alongside:
  `tests/run/test_pending.c` (R3: `run --pending` loop, `--max`
  clamping, worst exit code across mixed outcomes),
  `tests/run/test_deleted.c` (soft-delete claim: `run <id>` on a
  deleted row → not-found before claim; `run --pending` is live-only),
  `tests/run/test_sweep.c` (in-process sweep logic).
  `tests/argparse/test_argparse.c` (51e375c, since extended): 47
  pass-1/pass-2 parsing checks, green. `tests/llama_smoke.c` (0b06b25):
  manual smoke test against a LIVE OpenAI-compatible server
  (`make smoke`).
  `make -C acta_runner test-e2e`: dead-runner end-to-end suite — spawns
  real `acta_runner` processes against the stub server (~10–15 s),
  separate from `make test` (see `building.md`).

Implementation notes (where the spec left room):

- The model `configuration` JSON keys the runner understands:
  `temperature`, `max_tokens`, `top_k`, and `supports_response_format`
  (bool, default `true`). The API key is NOT a configuration key — it
  comes only from the `--api_key` flag or `$OPENAI_API_KEY` (decision 4);
  a `configuration` carrying `api_key` is rejected as an unknown key
  (`EXIT_INVALID`). `false` means the backend has no json_schema
  `response_format`: the field is not sent and the raw response is
  validated post-hoc instead. This is the concrete resolution of
  decision 3's "when the backend supports it".
- Post-hoc validation is a schema SUBSET validator (recursive `type`,
  `required`, `properties`, `items`; `pattern`/`enum`/`format`/… are out of
  scope). The raw response content is first parsed as a JSON document
  (unparseable → validation failure), then the parsed value is checked
  against the schema subset. Failure → `fail()` + `EXIT_INVALID`, with
  `validation_started` / `validation_failed` log rows.
- Non-DB failures emit a runner-layer JSON error line
  `{"error":"ACTA_RUNNER_ERROR","code":<exit code>,"message":...}`
  (code always equals the process exit code); DB failures keep the
  `ACTA_DB_ERR_*` contract. Exit codes: 12 for HTTP/preflight failures,
  13 for timeout, 4 for validation/claim failures.
- User message: `user = context.content`, always. The skill's
  `prompt_template` is the only instruction source (it is the `system`
  message); there is no per-execution prompt field. An empty context
  content fails the execution with `EXIT_INVALID` (`empty context
  content`). The `prompt_resolved` log event
  records the fully resolved `system` and `user` strings (plus their
  byte counts) in its `metadata`, so each execution is self-describing.
  The `executions` table has no prompt column.
- Preflight catalog (R8): after the `/v1/models` id match, the runner
  fetches the llama.cpp model catalog (`GET /`) and logs a
  `preflight_passed` event whose `metadata` carries `model_id`,
  `max_context` (when present in the `/v1/models` entry) and the
  matched catalog entry's `status.args` + `meta` — the server-instance
  configuration (launch flags, `n_ctx`, `ftype`, `size`). Best-effort:
  a missing/unparseable catalog or id miss records `"catalog":null` and
  never fails the execution.

## Decisions (finalized)

1. **Runner architecture.** Standalone C runner in `acta_runner/`.
   The GUI "Run" button shipped first as spawning
   `acta_runner run <id>` (commit 184d574); later refinement
   (M1 / UR #45, commit f6efe22) runs the same pipeline in-process,
   reusing the C `run.c`/`backend.c` directly instead of a Qt port —
   the standalone runner and its CLI remain available. The worker must be
   `moveToThread()`-ed into its `QThread`; reparenting it to the
   `QThread` (which lives on the GUI thread) would keep the worker's
   affinity on the GUI thread and dispatch the blocking pipeline into
   the GUI event loop, freezing the UI for the whole run. Ownership is
   therefore manual: `stopRunner()` posts `deleteLater()` to the
   worker's queue before `quit()` + `wait()` (see `runnerWorker.h`).
2. **Server lifecycle is user-managed.** The user launches
   `llama-server` (or any OpenAI-compatible backend) manually with
   whatever model they want. The runner is a pure HTTP client: it never
   spawns, loads, unloads, or terminates a server. The DB model record
   (`base_url`, `model_identifier`, `configuration`) is the only link to
   the server instance.
3. **Prompt resolution:** `system = skill.prompt_template`,
   `user = context.content`. There is no per-execution prompt field:
   the skill is the only instruction source, and the context is the
   user message content. If a skill has an
   `output_schema`, use `response_format: {"type":"json_schema",
   "schema": ...}` when the backend supports it, otherwise validate the
   raw response post-hoc. The resolved prompt is NOT stored in the
   `executions` row (the table has no prompt column);
   it is recorded in the `prompt_resolved` event of `execution_logs`
   (`metadata`: `system`, `user`, `system_bytes`, `user_bytes`), which
   makes the execution self-describing and protects the audit trail if
   prompt-resolution behavior changes later.
4. **Auth:** resolution order, highest first: `--api_key` flag →
   `$OPENAI_API_KEY`. The key is never read from the model
   `configuration` blob (it must not be stored in the database): a
   `configuration` carrying an `api_key` key is rejected as an unknown
   key and fails the execution with `EXIT_INVALID`. Sent as
   `Authorization: Bearer <key>` on every request (preflight GETs and
   the chat POST); optional when the server has no `--api-key` set.
5. **Timeouts / retries:** single request, configurable `--timeout` (s),
   no retries — failures are first-class artifacts here.
6. **Claim semantics:** the runner only acts on `pending`; `start()` is
   the atomic lock. Soft-deleted executions are never claimed: the
   `run --pending` batch queries live rows only (`include_deleted = 0`,
   so the DB layer appends the static `deleted_at IS NULL` clause), and
   `run <id>` on a soft-deleted row fails with the standard not-found
   path before any claim (tested in `acta_runner/tests/run/test_deleted.c`).
   Stale-`running` cleanup (dead runner) shipped as the
   `sweep` action: `acta_runner sweep --stale-seconds N` fails `running`
   rows whose last activity (max of latest `execution_log` timestamp and
   `started_at`) is older than N; `--stale-seconds` must be a positive
   integer.
7. **Logging granularity:** follow the DBDesign event list exactly
   (execution_started, context_loaded, prompt_resolved, preflight_passed,
   llm_request, llm_response, validation_*,
   execution_completed/failed, execution_cancelled) so the UI timeline
   shows meaningful phases. `preflight_passed` records the server-instance configuration
   (R8): the matched model id, `max_context`, and the catalog entry's
   launch `args` + `meta`. Best-effort: a missing catalog logs
   `"catalog":null` and never fails the execution.

## Phase 2 pipeline (spec for `run_execution`)

1. **Claim** — fetch execution; must be `pending`; `start()` →
   `running`; log `execution_started`.
2. **Resolve** — fetch context (`content`), skill revision
   (`prompt_template`, `output_schema`), model revision (`base_url`,
   `model_identifier`, `configuration`); log `context_loaded`,
   `prompt_resolved`.
3. **Preflight** (cheap, makes failures readable) — `GET /health`:
   `503` → fail "model still loading". `GET /v1/models`: mismatched
   `model_identifier` → fail "server is running a different model".
   Then best-effort `GET /` (llama.cpp model catalog): the matched
   entry's `status.args` + `meta` (`n_ctx`, `n_params`, `size`, `ftype`)
   and `max_context` are recorded in a `preflight_passed` log event.
   A missing/unparseable catalog never fails the execution — it records
   `"catalog":null` (non-llama OpenAI-compatible backends have no
   catalog; the engine must not depend on llama.cpp itself).
4. **Call** — `POST /v1/chat/completions` with `messages = [system:
   prompt_template, user: context.content]`, `model =
   model_identifier`, params from `configuration`, and
   `response_format = json_schema(output_schema)` when a skill has one;
   log `llm_request` (url, model id, params). Empty context content →
   `fail(EXIT_INVALID, "empty context content")` before the call.
5. **Record** — `set_raw_response(choices[0].message.content)`; log
   `llm_response` (HTTP status, latency, `usage` tokens, `timings`).
6. **Validate** — if `output_schema` is set and `response_format` was
   not used (non-llama backend), parse/validate post-hoc; log
   `validation_started` / `validation_failed`.
7. **Close** — `complete(result)` or `fail(error)`; log
   `execution_completed` / `execution_failed`. `run --pending` processes
   all pending rows in `id ASC` order, optionally capped by `--max <n>`
   (0 = no limit); the batch continues past a failure and returns the
   worst exit code seen.

## Explicitly out of scope (we do not implement these)

- Server manager mode (`--start-server`, process spawn/termination,
  model load/unload, load-timeout handling, vendored llama.cpp build).
- Streaming (SSE) responses.
- Automatic retries (a failed execution is retried manually via the
  `failed → pending` reset, `acta_db_execution_reset` — exposed by the
  GUI Retry button, and `acta_cli exec reset <id>`).
- Multimodal, tool calling, embeddings, LoRA, slot caching — anything
  beyond `chat/completions` from the backend (see
  `llamacpp_server_contract.md` §6).
