# Runner analysis

The missing "Run" path (UI review #18): a component that connects to the
database, resolves a pending execution's skill + model + context, calls the
OpenAI-compatible backend, and records the outcome (raw response, result,
error, phase logs) back into the database.

## Status: phase 2 shipped, Plan D shipped, R8 shipped

Phase 2 is implemented in `acta_runner/` (commit d142a8e). What landed:

- `src/backend.{h,c}` — curl wrapper exactly as specced: one GET/POST JSON
  request with timeout, optional `Authorization: Bearer`, result codes
  `BACKEND_OK / ERR_TRANSPORT / ERR_TIMEOUT / ERR_ALLOC` (the timeout
  `CURLcode` is selected by `LIBCURL_VERSION_NUM` — curl ≥ 8.8 renamed
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
  POSIX sockets + pthread / winsock) and `tests/run/test_run.c`: 10
  scenarios (success, health 503, model mismatch, chat 500, timeout,
  non-pending, not-found, post-hoc validation fail/pass, missing
  catalog), 58 checks, green under `make test`, on a scratch
  `:memory:` DB.
  `tests/argparse/test_argparse.c` (51e375c, since extended): 47
  pass-1/pass-2 parsing checks, green. `tests/llama_smoke.c` (0b06b25):
  manual smoke test against a LIVE OpenAI-compatible server
  (`make smoke`). Run with `make test` in `acta_runner/`.

Implementation notes (where the spec left room):

- The model `configuration` JSON keys the runner understands:
  `api_key` (takes precedence over `--api_key` / `$OPENAI_API_KEY` when
  set to a non-empty string),
  `temperature`, `max_tokens`, `top_k`, and `supports_response_format`
  (bool, default `true`). `false` means the backend has no json_schema
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
- User message concatenation order: the optional `execution.prompt` comes
  first (when present), then `context.content` is appended:
  `execution.prompt + "\n\n" + context.content` (either half may be
  empty; both empty → fail). Together with the system message
  (`skill.prompt_template`), the full prompt sent to the LLM is therefore:
  skill prompt first, then the execution prompt (if present), finally the
  context data. The `prompt_resolved` log event
  records the fully resolved `system` and `user` strings (plus their
  byte counts) in its `metadata`, so each execution is self-describing.
  `executions.prompt` keeps its original meaning: the optional,
  user-entered instruction at creation time.
- Preflight catalog (R8): after the `/v1/models` id match, the runner
  fetches the llama.cpp model catalog (`GET /`) and logs a
  `preflight_passed` event whose `metadata` carries `model_id`,
  `max_context` (when present in the `/v1/models` entry) and the
  matched catalog entry's `status.args` + `meta` — the server-instance
  configuration (launch flags, `n_ctx`, `ftype`, `size`). Best-effort:
  a missing/unparseable catalog or id miss records `"catalog":null` and
  never fails the execution.

## Codebase analysis

### acta_db (C11, libacta_db)

SQLite wrapper with a disciplined API (uniform getter/lister/mutator
contracts, ACTA_DB_* error codes, pagination cap 10000):

- model_t / model_revision_t: backend, base_url, model_identifier,
  configuration — the "connection to the backend" record. Revisions are
  trigger-managed, immutable snapshots.
- skill_t / skill_revision_t: prompt_template, output_schema.
- context_t: immutable, append-only input (type, content).
- execution_t: full lifecycle already exists — create → start() (pending
  → running) → complete(result) / fail(error), plus set_raw_response()
  and cancel(). Transitions are atomic (WHERE id = ? AND status = ?),
  explicitly designed so an external process can drive them.
- execution_log_t: level/event/message/metadata per execution — designed
  for exactly the phase rows in DBDesign.md (execution_started,
  context_loaded, prompt_resolved, llm_request, llm_response,
  validation_*, execution_completed/failed).

### acta_db_cli (C11, actagamma_db)

Thin CRUD client over the library (create/get/list/count/start/
cancel/complete/fail/set-raw for exec). Deliberately DB-only; contains
no HTTP anywhere. JSON via cJSON, verbose stderr logging, JSON error
contract. `exec create` takes `--prompt` as an **optional** field
(context-only is a valid execution; both prompt and context empty
fails at run time with the runner's clear error) — aligned with the
UI and runner decision; `--context_id`, `--skill_revision_id` and
`--model_revision_id` remain required.

### acta_gamma (C++ / Qt 6 Widgets)

management GUI. DbHandle is a small RAII wrapper around db_t*.
ExecutionCreateDialog creates a pending execution (context + skill
revision + model revision + prompt + optional parent). The Execution
panel's "Run" button (Plan D, commit 184d574) spawns
`acta_runner run <id> --db <path>` via `QProcess` and polls the
`execution_log` rows for live status. *(Since commit f6efe22, M1 / UR
#45: the button no longer spawns a process — `run.c`/`backend.c` are
compiled into the GUI and run on a worker thread with their own DB
connection; the panel's polling is unchanged. See
`ui_review.md` / `ui_active_action.md`.)*

## Key observations for the runner

1. The DB schema + state machine were built for exactly this. A runner
   is just: query pending → claim via start() → fetch skill/model
   revisions + context → HTTP call → set_raw_response → validate →
   complete/fail, logging execution_log rows along the way. Nothing new
   is needed in acta_db.
2. OpenAI-compatible is a single wire format. Whatever backend says, the
   endpoint is {base_url}/chat/completions with model =
   model_identifier. The configuration JSON can carry backend-specific
   knobs (temperature, max_tokens, api key).
3. One ambiguity to decide: prompt resolution. An execution already
   stores a user-entered prompt at creation, and the skill stores
   prompt_template. The DBDesign "prompt_resolved" event implies
   combining context + template + prompt. Simplest sensible mapping:
   system = skill.prompt_template, user = context.content +
   execution.prompt; store the resolved prompt back into the execution
   row if it should be auditable.
4. Concurrency: start() is atomic, so the runner can safely claim a
   pending execution; a dead runner leaves a row stuck in running (needs
   a cancel/retry policy).
5. Dependencies available on the current stack: cURL (MSYS2
   mingw-w64-x86_64-curl), cJSON already used by the CLI. No HTTP code
   exists yet, so this is the one genuinely new piece.
6. Backend reference: `llamacpp_server_README.md` is the full
   auto-generated llama.cpp server reference (long); the
   runner-relevant subset is distilled in `llamacpp_server_contract.md`
   (startup, /health, /v1/models, /v1/chat/completions, errors).

## Plans

### Plan A — standalone C runner binary acta_runner (selected)

A new top-level acta_runner/ in C, linking libacta_db.a + curl + cJSON,
run as acta_runner run <execution-id> or acta_runner run --pending. It
claims the execution with the atomic start() transition, fetches the
context, skill revision and model revision, POSTs to the model's
OpenAI-compatible endpoint, and walks the lifecycle back into the DB:
set_raw_response, optional output_schema validation, complete or fail,
with execution_log rows for each phase. It is headless and testable
against a mock OpenAI stub server, matches the repo's C style and CLI
conventions, and touches no existing component; the only cost is adding
cURL as a dependency.

### Plan B — runner as a C++/Qt shared library, UI runs it in a QThread

The runner becomes a C++ library using QNetworkAccessManager, called
from a QThread worker inside the GUI so a "Run" button drives
executions in-app with live status updates (covering UI review #44
directly). The upside is native in-app UX with no extra process; the
price is tying the engine to Qt, which makes it harder to unit-test
headless, pulls the HTTP stack into the GUI binary, and makes the
runner harder to reuse from the CLI.

### Plan C — new exec run action inside acta_db_cli

Add a run action to the existing actagamma_db CLI so no new binary is
needed. It breaks the clean split, though: the CLI is deliberately a
thin DB client with no HTTP, and bolting network calls, timeouts and
call failures into a CRUD tool muddles its error contract and scope. Not
recommended.

### Plan D — Plan A + GUI spawns it (recommended end state)

Keep the standalone runner from A and have the Qt "Run" button simply
spawn acta_runner run <id> via QProcess, with the execution panel
polling acta_db_execution_query for status and log rows. The SQLite
database becomes the message bus/queue between the two — no IPC, no
sockets, and WAL already supports concurrent readers/writers. This
satisfies UI review #18 and #44 with the least coupling, keeps the
runner independently testable, and leaves the door open for a
headless/CLI-driven mode later.

## Decisions (finalized)

1. **Plan A is the plan.** Standalone C runner in `acta_runner/`.
   Plan D (GUI spawns it) shipped in `acta_gamma` (commit 184d574).
   Later refinement (M1 / UR #45, commit f6efe22): the GUI runs the
   same pipeline in-process (a closer cousin of Plan B, but reusing
   the C `run.c`/`backend.c` directly instead of a Qt port) — the
   standalone runner and its CLI remain available.
2. **Server lifecycle is user-managed.** The user launches
   `llama-server` (or any OpenAI-compatible backend) manually with
   whatever model they want. The runner is a pure HTTP client: it never
   spawns, loads, unloads, or terminates a server. The DB model record
   (`base_url`, `model_identifier`, `configuration`) is the only link to
   the server instance.
3. **Prompt resolution:** `system = skill.prompt_template`,
   `user = execution.prompt + "\n\n" + context.content` (the optional
   execution prompt is appended first, then the context data; either
   half may be empty). If a skill has an
   `output_schema`, use `response_format: {"type":"json_schema",
   "schema": ...}` when the backend supports it, otherwise validate the
   raw response post-hoc. The resolved prompt is NOT stored in
   `executions.prompt` (that column stays the user-entered instruction);
   it is recorded in the `prompt_resolved` event of `execution_logs`
   (`metadata`: `system`, `user`, `system_bytes`, `user_bytes`), which
   makes the execution self-describing and protects the audit trail if
   prompt-resolution behavior changes later.
4. **Auth:** resolution order, highest first: per-model `configuration`
   JSON (`{"api_key": ...}`, only when a non-empty string) → `--api_key`
   flag → `$OPENAI_API_KEY`. The configuration value is applied last in
   the code, so it wins over the flag and the environment variable. Sent
   as `Authorization: Bearer <key>` on every request (preflight GETs and
   the chat POST); optional when the server has no `--api-key` set.
5. **Timeouts / retries:** single request, configurable `--timeout` (s),
   no retries — failures are first-class artifacts here.
6. **Claim semantics:** the runner only acts on `pending`; `start()` is
   the atomic lock. Stale-`running` cleanup (dead runner) shipped as the
   `sweep` action: `acta_runner sweep --stale-seconds N` fails `running`
   rows whose last activity (max of latest `execution_log` timestamp and
   `started_at`) is older than N; `--stale-seconds` must be a positive
   integer.
7. **Logging granularity:** follow the DBDesign event list exactly
   (execution_started, context_loaded, prompt_resolved, preflight_passed,
   llm_request, llm_response, validation_*,
   execution_completed/failed) so the UI timeline shows meaningful
   phases. `preflight_passed` records the server-instance configuration
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
   prompt_template, user: prompt + context.content]`, `model =
   model_identifier`, params from `configuration`, and
   `response_format = json_schema(output_schema)` when a skill has one;
   log `llm_request` (url, model id, params).
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

## Code shape in `acta_runner/`

- `src/backend.c/h` — small curl wrapper: GET/POST JSON with timeout →
  (http status, body); optional `Authorization: Bearer` from
  `configuration` / `--api_key`. Contract: `llamacpp_server_contract.md`.
- `src/run.c` — the pipeline above, logging every phase to
  `execution_log`.
- `tests/` — tiny local stub server (fixed port, echoing `/health`,
  `/v1/models`, `/v1/chat/completions`) covering: success →
  `completed`, 503 → `fail`, model mismatch → `fail`, HTTP error →
  `fail` + `EXIT_HTTP`, timeout.

## Explicitly out of scope

- Server manager mode (`--start-server`, process spawn/termination,
  model load/unload, load-timeout handling, vendored llama.cpp build).
- Streaming (SSE) responses.
- Automatic retries (a failed execution is retried manually via the
  `failed → pending` reset, `acta_db_execution_reset`).
- Multimodal, tool calling, embeddings, LoRA, slot caching — anything
  beyond `chat/completions` from the backend (see
  `llamacpp_server_contract.md` §6).

Stale-`running` cleanup used to be listed here; it is now shipped as the
`sweep` action (decision 6).
