# acta_runner tests

Layout mirrors `acta_cli/tests/`: one directory per concern, one binary
per suite. Every suite is a standalone program that prints
`<n> checks, <m> failure(s)` and exits 1 on any failure.

## Build & run

From `acta_runner/`:

    make test        # builds + runs every default suite (argparse first,
                     # then the pipeline / batch / preflight / sweep /
                     # delete / api-key / conf / shadow / deathmark /
                     # check / dialect / openai-dialect suites)
    make test-e2e    # dead-runner end-to-end suite (real child
                     # acta_runner processes; ~10-15 s)
    make smoke       # tests/llama_smoke: manual check against a LIVE
                     # llama-server (tests/llama_smoke <base_url> <api_key>
                     # [model] [prompt] [timeout_sec])
    make clean

Run a single built suite from `acta_runner/` (NOT from `tests/run/`):
the suites load `../acta_db/schema.sql` relative to the cwd and abort
elsewhere. On Windows the make targets take **no `.exe` suffix**
(`EXEEXT` is empty under mingw32-make) while the produced binaries do:

    ./tests/run/test_run          # POSIX
    ./tests/run/test_run.exe      # Windows (built via
                                  # mingw32-make tests/run/test_run)

## Shared harness

- `tests/stub_server.{h,c}` — in-process stub OpenAI-compatible backend
  (POSIX sockets + pthread, winsock on Windows) serving exactly what the
  runner calls: `GET /health`, `GET /v1/models`, `GET /` (the llama.cpp
  catalog), `POST /v1/chat/completions`. Configurable per scenario
  through `stub_config_t`:

  | field | meaning |
  |---|---|
  | `port` | bind port on 127.0.0.1 |
  | `health_status` | 200 (ready) or 503 (still loading) |
  | `model_id` | id the `/v1/models` entry carries (mismatch tests) |
  | `models_status` | `/v1/models` status (500 simulates a catalog failure) |
  | `chat_status` / `chat_error` / `chat_content` | chat reply, status and message content |
  | `catalog_status` | `GET /` status (404 = no llama.cpp catalog) |
  | `max_context` | `max_context` of the served `/v1/models` entry (0 = field omitted) |
  | `models_meta_n_ctx` | `meta.n_ctx` of that entry (0 = omitted) — the shape OpenAI-dialect servers (Strata) use instead of a top-level `max_context` |
  | `health_max_context` | `max_context` in the `/health` body (0 = omitted) — last-resort ceiling source |
  | `dialect` | `"llamacpp"` or `"openai"`: the envelope this stub accepts on `POST /v1/chat/completions`; `NULL` (the default) inspects no request body, so every pre-existing scenario is unchanged |
  | `delay_ms` | sleep before replying (timeout tests) |

  All ceiling fields default to 0 = field absent, so every pre-existing
  scenario is byte-identical to the pre-dialect stub.

  Counters (reset on `stub_server_start`): `stub_server_chat_requests()`,
  `stub_server_health_requests()`, `stub_server_models_requests()`,
  `stub_server_catalog_requests()` (the `GET /` llama.cpp catalog audit),
  plus `stub_server_last_auth()` (the `Authorization` header of the last
  request) and `stub_server_last_chat_body()` (the body of the last POST,
  a tail-preserving window of the last 16 KiB — `response_format` is the
  runner's final body key, so the tail is what must survive; `NULL` before
  any POST). They pin "no request reached the server" and "once per
  acta_runner invocation", and let a test assert **what the runner really
  sent**: which `response_format` envelope, and whether a parameter such
  as `top_k` was sent at all. **A dialect change must not add a request:**
  the openai dialect skips the `GET /` catalog call entirely
  (`docs/plans/backend_dialects.md` §9 decision 4), which
  `stub_server_catalog_requests() == 0` proves.

  When `cfg->dialect` is set, a request whose `response_format` envelope is
  the **other** dialect's shape is refused with `400` before any generation,
  with the message the real server would return:

  | stub dialect | refused envelope | message |
  |---|---|---|
  | `openai` | llama.cpp `{"type":"json_schema","schema":…}` | `response_format.json_schema needs a schema object` (Strata's real message, `docs/session_strata_backend.md` issue A) |
  | `llamacpp` | OpenAI `{"type":"json_schema","json_schema":{…}}` | `response_format.schema is required for type json_schema` (the stub's mirror-image invention; llama.cpp's actual wording is not pinned by any test) |

  Detection is textual and safe because `cJSON_PrintUnformatted` emits no
  spaces: only the OpenAI envelope has a `json_schema` **key**
  (`"json_schema":{`); the llama.cpp envelope carries `json_schema` only as
  the value of `"type"`.

- Scratch `:memory:` DB seeded from `acta_db/schema.sql`. Suites that
  leave rows `pending` on purpose open a fresh DB per scenario.
- `runner_gopts` is defined by the suites that link `src/run.c` without
  `src/main.c`.

## Suites

- `tests/argparse/test_argparse.c` — pass-1/pass-2 parsing: global flag
  extraction, `--db`/`--verbose` forms, `--version`/`-h` early flags,
  positional skipping, boolean vs value flags, `--name=value` inline
  form, unknown-flag rejection.

- `tests/dialect/test_dialect.c` — `src/dialect.{h,c}` in isolation (no
  HTTP, no DB, no stub server): `dialect_from_string()` is exact-match
  only (`llamacpp`/`openai`; `llama`, `vllm`, `OpenAI`, `""`, NULL are
  `DIALECT_UNKNOWN`), `dialect_get(DIALECT_UNKNOWN) == NULL`,
  `dialect_name(DIALECT_UNKNOWN) == "(unknown)"`,
  `dialect_expected_names() == "llamacpp or openai"`, the capability
  flags of the dialect matrix (`fetch_catalog`, `sends_top_k`), both
  `response_format` envelopes (llama.cpp `{"type":"json_schema","schema":…}`
  vs OpenAI `{"type":"json_schema","json_schema":{"name":"acta_output",
  "strict":true,"schema":…}}`), and the context-ceiling extraction with
  its `source` label.

- `tests/run/test_run.c` — pipeline (`run_execution`) against the stub:
  success (raw_response + full phase log), `/health` 503, model
  mismatch, chat 500, timeout, non-pending row, unknown id, post-hoc
  `output_schema` validation (pass and fail), missing catalog →
  `"catalog":null`, the strict `configuration` rules (unknown key,
  malformed JSON, wrong type, `api_key` as an unknown key), empty
  context content, prompt-too-large preflight (exact message, no
  backend call) and the at-the-limit boundary, the `meta.n_ctx`-only
  ceiling (scenario 18), and a model revision whose `backend` is
  `"vllm"` → `EXIT_INVALID` **before any HTTP call** with the exact
  "unsupported backend" message and zero stub requests (scenario 19).
  The dialect audit trail is asserted on both paths: `llm_request`
  metadata carries `"dialect"`, `"dialect_source"`
  (`"model_revision"` normally, `"cli-override"` for
  `run_execution_dialect`) and `"model_backend"`; `preflight_passed`
  carries `"dialect"` and `"max_context_source"`. Scenarios 20–22 cover
  the override: override ≠ row → `dialect_override` warn event, override
  = row → no warn, and a row registered `openai` needing no flag at all.

- `tests/run/test_openai_dialect.c` — the **openai dialect end-to-end**,
  against a stub configured with `cfg.dialect` (the one suite that proves
  the request body the runner builds is the one an OpenAI-compatible server
  accepts — the `llamacpp`-only suites cannot see the envelope). Six
  scenarios: (1) a `backend:"openai"` revision + an `output_schema` skill →
  `completed`, body carries the OpenAI envelope
  (`json_schema{name:"acta_output",strict:true,schema}`),
  `preflight_passed` records `dialect=openai`, `"catalog":null`, the ceiling
  from `entry.meta.n_ctx`, `llm_request` records
  `dialect/dialect_source=model_revision/model_backend`, **zero** `GET /`
  catalog calls (decision 4); (2) that OpenAI envelope against a `llamacpp`
  stub → `EXIT_HTTP` (12), row `failed`, the refusal recorded, no
  `llm_response`; (3) the llama.cpp envelope against an `openai` stub →
  `EXIT_HTTP` with Strata's real message (issue A reproduced
  deterministically); (4) `top_k` in `model.configuration` is **not** sent
  on the openai dialect while `temperature`, configured in the same object,
  **is** (decision 3); (5) `top_k` **is** sent on the llamacpp dialect, with
  exactly one `GET /` catalog call and the ceiling from `entry.max_context`;
  (6) `supports_response_format:false` → no envelope at all, accepted by the
  openai stub, post-hoc `validation_started`. Stub port 8923.

- `tests/run/test_pending.c` — `run --pending` batch loop and `--max`
  clamping (R3): N pending all run, `--max M < N` runs exactly the
  first M by `id ASC`, `--max 0` = no limit, mixed outcomes keep the
  batch going with the worst exit code, empty batch exits 0.

- `tests/run/test_autopreflight.c` — the auto preflight before the
  atomic claim (runner-ops item 4), with the stdout verdict line
  captured (and stderr captured for scenario 7):
  - dead server / `/health` 503 / model not served → exit 12 + the
    `check` verdict line, rows stay `pending`, no `execution_log` rows,
    zero chat requests (no claim, no DB writes)
  - slow server + `--timeout 1` → exit 13, "server unreachable"
  - `run <id>`, dead server → exit 12, row stays `pending`, no DB writes
  - healthy server, 2 pending rows → both `completed`; `/health` and
    `/v1/models` counts (3 each: 1 pre-claim auto preflight + 1
    per-execution pipeline preflight) pin "once per invocation"
  - `run --pending` with a model revision backend outside the dialect
    vocabulary (`"vllm"`) → exit 4, the message on stderr naming the
    bad backend and the accepted vocabulary, no verdict line on stdout,
    rows stay `pending`, no `execution_log` rows, **zero** stub requests
    including `/health`
  - `run --pending --dialect openai` → exit 4 (a batch mixes model
    revisions), same "nothing claimed, no DB writes, zero requests"
    assertions
  - `run <id> --dialect vllm` → exit 4 before the claim and before any
    HTTP call
  - `run <id> --dialect openai` on a `llamacpp` row → exit 0,
    `completed`, `llm_request` records dialect=openai /
    dialect_source=cli-override / model_backend=llamacpp plus the
    `dialect_override` warn

- `tests/run/test_deleted.c` — `run <id>` and `run --pending` against
  soft-deleted `pending` rows (single-claim guard + live-only batch
  claim).

- `tests/run/test_api_key.c` — the API-key presence policy (decision 4):
  unset `$OPENAI_API_KEY` → the run does not start (`EXIT_INVALID`),
  empty → warning and no `Authorization` header, set → `Bearer <key>`
  (asserted through `stub_server_last_auth()`).

- `tests/run/test_conf.c` — per-machine config file integration:
  env-set-wins, file fallback, bad-permissions refusal, missing-file
  hard error, `max_chars` / `timeout` resolution.

- `tests/run/test_shadow.c` — the `OPENAI_API_KEY` shadow warning
  (runner-ops item 1), with per-run stderr capture: env empty + file key
  → no `Authorization` header + the one-line warning; env set + file key
  → env key used + warning; env unset + file key → file key, no warning;
  env set + no file → env key, no warning; plus the
  `acta_conf_api_key_shadow_warning` truth table.

- `tests/run/test_sweep.c` — `sweep --stale-seconds` stale-`running`
  cleanup (R4), pure DB + backdated timestamps: fresh row kept, stale
  row → `failed` with the "stale running" error + `execution_failed`,
  mixed → only the stale row swept, orphan swept via the `started_at`
  fallback, newest-wins, `--stale-seconds 0`/missing/non-numeric →
  `EXIT_INVALID`, inline `--stale-seconds=<n>` form.

- `tests/run/test_deathmark.c` — death marker for orphaned `running`
  rows (runner-ops item 3): in-process, pure DB — claim a row, invoke
  `deathmark_exit_path`, assert `failed` + the "runner process exited
  during execution" diagnostic + one error-level `execution_failed`
  row; idempotent; no-op on `completed` rows, unknown ids, a released
  claim. The signal wiring is covered by `test_deadrunner.c`.

- `tests/run/test_deadrunner.c` — end-to-end dead-runner recovery (real
  `acta_runner` child processes, scratch FILE db in `tests/run/`, stub
  server on port 8918): a child is killed mid-run, the row stays
  `running`, a later real `sweep` child transitions it to `failed` with
  the exact stale error. ~10-15 s, so it is deliberately NOT in the
  default `test` target: `make test-e2e`.

- `tests/check/test_check.c` — the token-free `check` action: every
  scenario leaves `stub_server_chat_requests() == 0` and creates no
  execution rows; the stdout verdict JSON shape, `--base-url` +
  `--model-identifier` standalone mode, DB mode by **model record id**,
  dead server, `/health` 503, model not served, timeout, the
  `max_context` fallback chain with its source label (scenario 10), and
  a model whose `backend` is `"vllm"` → `EXIT_INVALID` with zero
  requests (scenario 11). Scenarios 12–15 pin `--dialect`: accepted in
  standalone mode (where it also changes the ceiling order —
  `meta.n_ctx` beats the entry `max_context` on the openai dialect),
  rejected on a value outside the vocabulary, rejected with a
  `<model-record-id>` (the row decides in DB mode), and a row
  registered `openai` checked without any flag.

- `tests/llama_smoke.c` — manual smoke check against a LIVE
  OpenAI-compatible server (`make smoke`), not part of `make test`.
