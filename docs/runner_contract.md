# Runner analysis

Analysis and spec of the `acta_runner` component: a
component that connects to the database, resolves a pending execution's
skill + model + context, calls the OpenAI-compatible backend, and records
the outcome (raw response, result, error, phase logs) back into the
database.

## Status: phase 2 shipped, in-app Run shipped, R8 shipped, max_chars size check shipped

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
- `src/run.c` preflight size check (decision 8 below): the assembled
  prompt is exactly `system =
  skill.prompt_template` + `user = context.content`, so preflight compares
  `strlen(prompt_template) + strlen(context.content)` against the
  resolved `max_chars` limit (config file `"max_chars"` → built-in
  default `ACTA_CONF_DEFAULT_MAX_CHARS` = 100,000 chars, resolved by
  `acta_conf_resolve_max_chars`) and, over the limit, fails the
  execution with `EXIT_INVALID` and `prompt too large: N chars total
  (context X + skill prompt Y) exceeds max_chars Z` **before any backend
  call** — a deterministic local cause instead of an opaque backend 400
  at chat time (decision 8 below).
- `tests/` — in-process stub OpenAI server (`tests/stub_server.{h,c}`,
  POSIX sockets + pthread / winsock) and `tests/run/test_run.c`: 17
  scenarios (success, health 503, model mismatch, chat 500, timeout,
  non-pending, not-found, schema validation fail/pass, missing
  catalog, unknown config key, malformed config JSON, wrong config
  key type, config api_key unknown key, empty context content,
  prompt-too-large, at-limit), check count printed at runtime, green
  under `make test`, on a scratch `:memory:` DB. Batch/claim/cleanup suites alongside:
  `tests/run/test_pending.c` (R3: `run --pending` loop, `--max`
  clamping, worst exit code across mixed outcomes),
  `tests/run/test_deleted.c` (soft-delete claim: `run <id>` on a
  deleted row → not-found before claim; `run --pending` is live-only),
  `tests/run/test_sweep.c` (in-process sweep logic). The size-check
  scenarios are
  `tests/run/test_run.c` scenarios 16 (over the limit: fails preflight,
  no backend call, exact message) and 17 (at the limit: proceeds and
  completes).
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
  comes only from `$OPENAI_API_KEY`, or, when that variable is unset,
  the `"api_key"` key of the per-machine config file (decision 4);
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
2. **Server lifecycle is user-managed.** The user launches the
   llama.cpp `llama-server` (router mode) manually with whatever model
   they want. The runner is a pure HTTP client: it never
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
4. **Auth:** the key sources are `$OPENAI_API_KEY` and, as a fallback,
   the `"api_key"` key of the per-machine config file (`ACTA_Gamma.conf`
   in the app-data directory) — there
   is no `--api_key` flag, and the key is never read from the model
   `configuration` blob (it must not be stored in the database; a
   `configuration` carrying an `api_key` key is rejected as an unknown
   key and fails the execution with `EXIT_INVALID`). Precedence:
   `$OPENAI_API_KEY` (if set — even to the empty string) wins over the
   file; the file is a fallback, not a second channel. Having **both**
   sources is a deliberate owner decision, not an accident: the env var
   is the primary channel for scripted and programmatic use (and stays the
   only source for a machine without a config file); the file key is the
   per-machine fallback for GUI and no-shell-environment setups. An
   "env var only" policy was considered and rejected — the file fallback
   costs one precedence rule and removes the need to export a secret on
   machines where the GUI is the primary interface. No key anywhere
   (env unset + file key absent/missing) → the run does not start
   (runner: `ACTA_RUNNER_ERROR`, exit 4, before any claim; GUI: error in
   the run result); key empty (from either source) → warning and no
   `Authorization` header sent (acceptable only for a keyless localhost
   server; a bad idea in general). A readable-but-malformed config file
   (or one whose mode gives read access to group or other — the file
   must be `0600`; on Windows the mode bits are meaningless and the
   check only warns, so the file is read anyway) is a fail-closed hard
   error before any claim, even when `$OPENAI_API_KEY` is set. When a key is present it is sent
   as `Authorization: Bearer <key>` on every request (preflight GETs and
   the chat POST); the header is optional on the server side when it has
   no `--api-key` set.
5. **Timeouts / retries:** single request, per-call timeout resolved as
   `--timeout` (s, per-run flag) → the config file's `"timeout"` (s,
   per-machine default) → built-in default 300 s; no retries —
   failures are first-class artifacts here.
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
8. **Prompt size limit (`max_chars`).** The assembled prompt is exactly
   `system = skill.prompt_template` + `user = context.content`, so a
   plain char count is a deterministic, model-agnostic guard: preflight
   checks `total_chars = strlen(prompt_template) +
   strlen(context.content)` against the resolved `max_chars` limit and,
   if `total_chars > max_chars`, fails the execution **before any
   backend call** with `EXIT_INVALID` and the message `prompt too large:
   N chars total (context X + skill prompt Y) exceeds max_chars Z`
   (same `pending → running → failed` + `execution_failed` log row as
   the other preflight failures). The limit is resolved per-machine as
   config file `"max_chars"` → built-in default
   `ACTA_CONF_DEFAULT_MAX_CHARS` = 100,000 chars
   (`acta_conf_resolve_max_chars`); it is a guard, not a window-fit
   guarantee — the backend's served `max_context` remains the final
   arbiter for under-limit prompts and keeps being recorded in
   `preflight_passed`; it is not used by the check itself.

## Phase 2 pipeline (spec for `run_execution`)

1. **Claim** — fetch execution; must be `pending`; `start()` →
   `running`; log `execution_started`.
2. **Resolve** — fetch context (`content`), skill revision
   (`prompt_template`, `output_schema`), model revision (`base_url`,
   `model_identifier`, `configuration`); log `context_loaded`,
   `prompt_resolved`.
3. **Preflight** (cheap, makes failures readable) — prompt size check
   first: `total_chars = strlen(prompt_template) +
   strlen(context.content)` vs. the resolved `max_chars` limit; over the
   limit → `fail(EXIT_INVALID, "prompt too large: N chars total (context
   X + skill prompt Y) exceeds max_chars Z")` before any backend call
   (decision 8). `GET /health`: `503` → fail "model still loading".
   `GET /v1/models`: mismatched `model_identifier` → fail "server is
   running a different model".
   Then best-effort `GET /` (llama.cpp model catalog): the matched
   entry's `status.args` + `meta` (`n_ctx`, `n_params`, `size`, `ftype`)
   and `max_context` are recorded in a `preflight_passed` log event.
   A missing/unparseable catalog never fails the execution — it records
   `"catalog":null`.
4. **Call** — `POST /v1/chat/completions` with `messages = [system:
   prompt_template, user: context.content]`, `model =
   model_identifier`, params from `configuration`, and
   `response_format = json_schema(output_schema)` when a skill has one;
   log `llm_request` (url, model id, params). Empty context content →
   `fail(EXIT_INVALID, "empty context content")` before the call.
5. **Record** — `set_raw_response(choices[0].message.content)`; log
   `llm_response` (HTTP status, latency, `usage` tokens, `timings`).
6. **Validate** — if `output_schema` is set and `response_format` was
   not used (i.e. the model record has `supports_response_format: false`),
   parse/validate post-hoc; log
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
