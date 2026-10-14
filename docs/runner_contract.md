# Runner analysis

Analysis and spec of the `acta_runner` component: a
component that connects to the database, resolves a pending execution's
skill + model + context, calls the OpenAI-compatible backend, and records
the outcome (raw response, result, error, phase logs) back into the
database.

## Status: phase 2 shipped, in-app Run shipped, R8 shipped, max_chars size check shipped, `check` health action shipped (no completion tokens, no DB writes), run auto preflight shipped, backend dialects shipped

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
  `running`. In addition, `cmd_run` (`run <id>` and `run --pending`) runs the auto preflight **before the claim**: the same two preflight GETs `check` uses (via `backend_preflight()`), once per `acta_runner` invocation — for the execution the claim is about (in a `--pending` batch, the first pending row); dead/unreachable backend or model not served → exit with the `check` verdict code (12/13), the same JSON verdict line, nothing claimed, no DB writes.
- R8 (commit 2f8085e): preflight catalog logging — after the
  `/v1/models` id match, the runner fetches the llama.cpp model catalog
  (`GET /`) and logs a `preflight_passed` event with the matched entry's
  server-instance configuration (`status.args`, `meta`: `n_ctx`,
  `n_params`, `size`, `ftype`) plus `max_context`. Best-effort: a missing
  catalog records `"catalog":null` and never fails the execution; the
  stub server serves `GET /` with a canned catalog
  (`catalog_status`), and `test_run.c` scenario 10 covers the missing-
  catalog path. Tests green, no regressions.
- `src/check.c` — the `check` action (no completion tokens, no DB writes):
  `acta_runner check
  <model-record-id>` (DB mode: the model row is read read-only for
  `base_url` + `model_identifier` + `backend`; no execution is created, claimed, or
  logged) or `acta_runner check --base-url <url> --model-identifier <id>`
  (standalone: no DB lookup at all). Two GETs — `GET /health`
  (200 ok; 503 `model still loading`; unreachable `server unreachable`)
  then `GET /v1/models` (id present → ok with the catalog entry's
  `max_context`; absent → `model not served`; endpoint error →
  `catalog unreachable`). One JSON verdict line on stdout
  (`{"ok":true,...}` / `{"ok":false,...,"verdict":"..."}`); exit
  0/1/12/13 from the existing code space. The shared `/health` +
  catalog request/response path is factored into
  `backend_preflight()` in `src/backend.c` — the `run` pipeline's
  preflight and `check` call the same function, so the two cannot drift.
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
  `tests/run/test_autopreflight.c` (pre-claim auto preflight: dead
  server / `/health` 503 / model not served / timeout → exit 12/13 with
  the `check` verdict line, rows stay `pending`, no DB writes; healthy
  → the run proceeds, with the /health + /v1/models counts pinning
  "once per invocation"),
  `tests/run/test_deleted.c` (soft-delete claim: `run <id>` on a
  deleted row → not-found before claim; `run --pending` is live-only),
  `tests/run/test_sweep.c` (in-process sweep logic). The size-check
  scenarios are
  `tests/run/test_run.c` scenarios 16 (over the limit: fails preflight,
  no backend call, exact message) and 17 (at the limit: proceeds and
  completes).
  `tests/check/test_check.c` — the `check` action against the stub on a
  scratch `:memory:` DB: DB-mode success (exit 0, `max_context`
  reported), `/health` 503 (`model still loading`), model not in catalog
  (`model not served`), catalog endpoint error (`catalog unreachable`),
  connection refused (`server unreachable`), slow `/health` + small
  `--timeout` (EXIT_TIMEOUT), unknown `<model-record-id>` (EXIT_NOT_FOUND),
  standalone mode (no DB lookup, `db` handle NULL), argument conflicts
  (EXIT_INVALID). In EVERY scenario the stub's
  `/v1/chat/completions` request count stays 0 and no execution / 
  execution_log rows exist — the zero-token / no-DB-writes guarantee.
  `tests/argparse/test_argparse.c` (51e375c, since extended): 47
  pass-1/pass-2 parsing checks, green. `tests/llama_smoke.c` (0b06b25, rewritten
  as a two-dialect live check): the LIVE check of either dialect — its request
  bodies are built through `src/dialect.c`, so it sends exactly what `run.c`
  sends (`top_k` only when the dialect sends it, `response_format` only through
  the dialect's builder). `make smoke` builds it, `make smoke-live`
  (`SMOKE_DIALECT=llamacpp|openai`) builds and runs it; its exit codes are 0
  usable / 1 hard failure / 3 usage-or-dialect error. Never part of `make test`.
  The shell counterpart is `scripts/live_backend_check.sh` (`make test-live*`),
  which adds the full CLI→runner pipeline in a scratch DB and asserts the audit
  keys above; a live check exits 0 with `SKIP: no backend at <url>` when no
  server answers at all, so a live target is never red because a server is off
  (see `building.md`, "Live backend check`).
  `tests/util/test_util.c`: the emitter contract of `runner_util.h` — every
  diagnostic is one parseable JSON line and the printed `code` is the exit
  code the emitter returns (see the error contract above); fd-level capture,
  no HTTP, no DB.
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
  13 for timeout, 4 for validation/claim failures. The line is pinned by
  `tests/util/test_util.c`, which captures the emitters' output and asserts
  both the exact bytes and that the line parses as JSON — a malformed line
  (a missing opening quote) is a contract break even with the right exit
  code.
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
   the standalone runner and its CLI remain available. **Cancel:** the
   pipeline runs on a worker thread; **Cancel** sets a flag checked
   between pipeline steps, and a response that arrives after cancel is
   discarded — an HTTP call already in flight runs to completion (the
   backend still generates the full response; the tokens are consumed
   regardless), so cancel stops the **recording**, not the compute, and
   transitions the row to `cancelled`. The worker must be
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
   `output_schema`, send `response_format` in the **dialect's** envelope
   (decision 9: `llamacpp` `{"type":"json_schema","schema":…}`,
   `openai` `{"type":"json_schema","json_schema":{"name":"acta_output",
   "strict":true,"schema":…}}`) when the backend supports it, otherwise
   validate the raw response post-hoc. The resolved prompt is NOT stored in the
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
   file; the file is a fallback, not a second channel. The shadowing is
   not silent: when `$OPENAI_API_KEY` is set (even to the empty string)
   and the file key is non-empty, both binaries print a one-line
   warning — `warning: OPENAI_API_KEY is set (empty or not) and shadows
   the config file "api_key"; the effective key is the environment
   value` — to stderr in `acta_runner` (once per `run` invocation) and
   in the run result message in `acta_gui`; no warning when the env var
   is unset or the file key is absent/empty (nothing is being shadowed).
   Having **both**
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
   check only warns, so the file is read anyway — keeping the file
   private is the owner's responsibility) is a fail-closed hard
   error before any claim, even when `$OPENAI_API_KEY` is set. When a key is present it is sent
   as `Authorization: Bearer <key>` on every request (preflight GETs and
   the chat POST); the header is optional on the server side when it has
   no `--api-key` set.
5. **Timeouts / retries:** single request, per-call timeout resolved as
   `--timeout` (s, per-run flag) → the config file's `"timeout"` (s,
   per-machine default) → built-in default 600 s; no retries —
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
   (R8): the matched model id, `max_context` with its `max_context_source`,
   the `dialect` used (decision 9), and the catalog entry's launch `args` +
   `meta`. Best-effort: a missing catalog — or a dialect with no catalog
   endpoint at all — logs `"catalog":null` and never fails the execution.
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
9. **Backend dialect (`llamacpp` / `openai`).** `models.backend` is a
   **closed vocabulary**, not a free-text label: `llamacpp` or `openai`.
   `acta_runner/src/dialect.{h,c}` is the single source of the vocabulary and
   of the two shapes that genuinely differ (`dialect_from_string()`,
   `dialect_expected_names()` → `"llamacpp or openai"`); `acta_cli` compiles
   that same module. A dialect is a set of HTTP request/parse shapes, not a
   vendor: `openai` means "strict OpenAI-compatible HTTP dialect" (Strata,
   vLLM, …), not "the hosted OpenAI API". There is no third dialect, no plugin
   registry and **no runtime auto-detection** — a request body chosen by
   probing the server is exactly the input nondeterminism the revision design
   exists to forbid.
   - **Source:** the dialect belongs to the **model revision**
     (`model_revisions.backend`, copied from `models.backend` at every
     revision). `run <id>`, `run --pending` (per row), `check
     <model-record-id>` and the GUI **Run** button all take it from the row:
     the same execution row + the same model revision must always produce the
     same request body.
   - **Override:** `run <id> --dialect <llamacpp|openai>` is the CLI escape
     hatch — accepted only with a single explicit execution id, never mutating
     the DB or the revision, and never silent: `llm_request` metadata gains
     `"dialect_source":"cli-override"` plus `"model_backend":"<row value>"`,
     and a `warn`-level `dialect_override` log event is written when the
     override disagrees with the row. `run --pending --dialect …` is rejected
     with `EXIT_INVALID` (4) **before the claim** — a batch mixes model
     revisions, one flag cannot describe them. `check` standalone mode (no
     model row) accepts `--dialect` (default `llamacpp`); `check
     <model-record-id> --dialect …` is rejected, the row decides in DB mode.
   - **Unknown value:** fails **before any HTTP call** with `EXIT_INVALID` and
     `model revision N has unsupported backend '<value>' (expected llamacpp or
     openai)`. `acta_cli model create/update` validates the same vocabulary at
     the declaration site (`invalid backend '<value>' (expected llamacpp or
     openai)`); the runner re-validates at run time as defense for rows written
     by another tool. Matching is exact and case-sensitive: `llama` is **not**
     an alias of `llamacpp`, `OpenAI` is not `openai`.
   - **What actually differs** (matrix: `docs/archive/backend_dialects.md` §2):
     the `response_format` envelope (decision 3), `top_k` (sent by `llamacpp`,
     omitted by `openai` — `model.configuration.top_k` is still *validated* in
     both dialects, a bad value still fails the execution), the `GET /` catalog
     audit (made only by `llamacpp`; `openai` makes no request and records
     `"catalog":null`), and the preferred context-ceiling source
     (`entry.max_context` first for `llamacpp`, `entry.meta.n_ctx` first for
     `openai`, `/health max_context` as last resort for both). Everything else
     — one call, no retries, no streaming, `raw_response =
     choices[0].message.content`, exit codes, log phases — is shared and stays
     shared.
   - **Audit:** `llm_request` metadata always carries `dialect`,
     `dialect_source` (`model_revision` | `cli-override`) and `model_backend`
     (the row value), so an audit can tell which envelope produced a stored
     `raw_response`; `preflight_passed` carries `dialect` and
     `max_context_source` (`entry.max_context` | `entry.meta.n_ctx` |
     `health.max_context` | `none`).
   - Contracts: [`llamacpp_server_contract.md`](llamacpp_server_contract.md)
     and [`openai_dialect_contract.md`](openai_dialect_contract.md).

## Phase 2 pipeline (spec for `run_execution`)

**Pre-claim auto preflight (cmd_run level, before step 1)** — `run <id>`
and `run --pending` call the same two preflight GETs `check` uses (via
`backend_preflight()`), once per `acta_runner` invocation — for the
execution the claim is about (in a `--pending` batch, the first pending
row). Dead/unreachable backend or model not served → exit with the
`check` verdict code (12/13) and the same JSON verdict line, nothing
claimed, no DB writes. The
per-execution step-3 preflight below still runs, after the claim.

1. **Claim** — fetch execution; must be `pending`; `start()` →
   `running`; log `execution_started`.
2. **Resolve** — fetch context (`content`), skill revision
   (`prompt_template`, `output_schema`), model revision (`base_url`,
   `model_identifier`, `configuration`, `backend`); log `context_loaded`,
   `prompt_resolved`. The model revision's `backend` resolves to the dialect
   (decision 9); a value outside the vocabulary fails here, before any backend
   call.
3. **Preflight** (cheap, makes failures readable) — prompt size check
   first: `total_chars = strlen(prompt_template) +
   strlen(context.content)` vs. the resolved `max_chars` limit; over the
   limit → `fail(EXIT_INVALID, "prompt too large: N chars total (context
   X + skill prompt Y) exceeds max_chars Z")` before any backend call
   (decision 8). `GET /health`: `503` → fail "model still loading".
   `GET /v1/models`: mismatched `model_identifier` → fail "server is
   running a different model". The ceiling is read through the dialect and the
   field it came from is recorded as `max_context_source`.
   Then best-effort `GET /` (llama.cpp model catalog) — **only on the
   `llamacpp` dialect** (`dialect_t.fetch_catalog`): the matched
   entry's `status.args` + `meta` (`n_ctx`, `n_params`, `size`, `ftype`)
   and `max_context` are recorded in a `preflight_passed` log event.
   A missing/unparseable catalog never fails the execution — it records
   `"catalog":null`, which is also what the `openai` dialect records without
   making the call at all.
4. **Call** — `POST /v1/chat/completions` with `messages = [system:
   prompt_template, user: context.content]`, `model =
   model_identifier`, params from `configuration` (`top_k` only on the
   `llamacpp` dialect), and `response_format = the dialect's json_schema
   envelope` when a skill has an `output_schema` and
   `supports_response_format` is not `false`; log `llm_request` (url, model
   id, `response_format`, `dialect`, `dialect_source`, `model_backend`).
   Empty context content →
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

## check action (backend health check — no completion tokens, no DB writes)

The only way to find out whether the backend is usable used to be
starting a real execution — which claims the row (`pending →
running`), performs an actual `POST /v1/chat/completions` (tokens and
model compute), and, on failure, leaves a `failed` row that needs
`exec reset` or `sweep`. `check` answers "is the server up, is my model
loaded, what context window is it serving?" **without paying for a
completion and without touching the audit database**.

```
acta_runner check <model-record-id>                            # DB mode
acta_runner check --base-url <url> --model-identifier <id>     # standalone
                      [--dialect llamacpp|openai]
```

- **DB mode** (default): `<model-record-id>` is a **`models` row id, not an
  execution id** (`check 4` says `model 4 not found` even when execution 4
  exists); the
  runner reads `base_url` + `model_identifier` + `backend` from the record. No
  execution is created, claimed, or logged — the DB is only read for the
  model row. Unknown id → `EXIT_NOT_FOUND`.
- **Standalone mode**: `--base-url` / `--model-identifier` are mutually
  exclusive with the positional id; no DB lookup at all (the DB is not
  even resolved/opened in this mode) — useful to probe a server before
  any model is registered. With no model row the dialect has nowhere else to
  come from, so `--dialect` is accepted here (default `llamacpp`, i.e. today's
  standalone behavior is unchanged); it also changes the ceiling order
  (`meta.n_ctx` beats the entry `max_context` on `openai`). `--dialect` without
  `--base-url`+`--model-identifier`, or combined with a positional id, is
  rejected with `EXIT_INVALID` (4).

**Check sequence** (neither call asks for a completion and neither writes to
the DB; both send `Authorization: Bearer <key>` only when a key resolves — see
the API-key paragraph below and decision 4; they go through the shared
`backend_preflight()` in `src/backend.c`):

1. `GET /health` — `200` ok; `503` → verdict `model still loading`
   (same meaning as the run preflight); connection refused / DNS /
   timeout (or any other non-200) → verdict `server unreachable`.
2. `GET /v1/models` — only after `/health` succeeds (a 503 server is
   "loading", not "unknown model"): `model_identifier` present in the
   catalog → verdict ok, the catalog entry's `max_context` is
   reported through the dialect (entry `max_context` / entry `meta.n_ctx` /
   `/health max_context`); absent → verdict `model not served`;
   endpoint error / unparseable body → verdict `catalog unreachable`.
   `check` makes no `GET /` catalog call in either dialect.

**API key** (decision 4, extended for `check` 2026-10-13): the two GETs
send `Authorization: Bearer <key>` when a key resolves from
`$OPENAI_API_KEY` (if set, even empty) -> the config file `"api_key"`
(same resolution as `run`; a set env var, even empty, shadows the file
key). Without that, `check` could never reach a key-protected server — a
llama-server started with `--api-key` answers 401 on `/v1/models` without
the key (its `/health` stays open) — and the scripted live pipeline stops
at the `check` step. With no key anywhere the calls are keyless, exactly
as before: keyless servers are unchanged, and a missing key is NOT a
hard error here (unlike `run`, where it exits 4 before the claim).

**Result contract** — one JSON line on stdout (scriptable):

- success: `{"ok":true,"model":"<id>","max_context":<n>,"base_url":"<url>"}`
- failure: `{"ok":false,"model":"<id>","base_url":"<url>","verdict":"model
  still loading" | "server unreachable" | "model not served" | "catalog
  unreachable"}`

**Exit codes** (the runner's existing code space, no new numbers):
`0` all checks passed; `1` unknown `<model-record-id>` (DB mode);
`4` invalid arguments (flag conflicts, bad `--timeout`, malformed
config file — the file is always read, for the api-key fallback);
`12` (`EXIT_HTTP`) `/health`
non-200, model not in catalog, HTTP-level failure; `13`
(`EXIT_TIMEOUT`) a check call hit `--timeout`. Timeout resolution is the
existing one: `--timeout` flag → config file `"timeout"` → built-in
default 600 s.

**Guarantees (the point of the command):** no
`POST /v1/chat/completions` — ever; zero tokens, zero inference. No
`execution_log` rows, no execution state change, no DB writes of any
kind. No streaming, no retry loop (consistent with the product
decisions). The `run` action now runs this pre-flight automatically
before the claim; `check`
remains the manual triage tool: a standalone probe before any model is
registered, and the first triage step after a `failed` backend call.

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
