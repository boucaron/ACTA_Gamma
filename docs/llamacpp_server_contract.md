# llama.cpp server — runner-relevant contract

Distilled from the llama.cpp server README (the full auto-generated
server reference in the upstream llama.cpp repository; only the
runner-relevant subset is kept here). Tested on **llama.cpp 0.4.x** (router mode); compatibility with later versions is not guaranteed — if the router surface changes, the preflight and catalog-fetch steps need re-validation.

## 1. Server startup (canonical: router mode)

The canonical ACTA deployment is **router mode** — one `llama-server`
process serves several models and routes each request to the matching
model instance (this is what the README, `building.md` and the runner
preflight assume):

```
llama-server --models-dir models -c 2048
```

- Launched **without** `-m`; every GGUF file in `--models-dir` becomes a
  served model, and each model's `id` defaults to the file's basename
  (e.g. `qwen3-8b` for `models/qwen3-8b.gguf`) — that is the value to
  store in the DB `model_identifier`.
- Listens on `127.0.0.1:8080` by default.
- Relevant flags:
  - `--models-dir <dir>` directory of GGUF models (router mode)
  - `-m <file>`        single-model mode: load one model at process start;
                       the file path becomes the model `id` unless aliased
  - `--alias <name>`   custom model `id` instead of the file path
                       (single-model mode)
  - `--port / --host`  endpoint the DB `base_url` points at
  - `-c <n>`           prompt context size
  - `--api-key KEY`    if set, requests need `Authorization: Bearer <key>`;
                       if not set, any/absent key is accepted
  - `-j <json-schema>` schema-constrained generation at the server side
  - `-n <n>`           max tokens to predict
- Router mode: **one server = N models**. Adding/removing a model =
  adding/removing a GGUF file in `--models-dir` and restarting the
  process (loading/unloading = process start/stop in either mode).
- Single-model mode (`-m`) remains a fully supported alternative; every
  contract below holds in both modes.

## 2. Readiness check

`GET /health` (also `/v1/health`), public, no key:

- `200` → `{"status": "ok"}` — model loaded, ready.
- `503` → `{"error": {"code": 503, "message": "Loading model",
  "type": "unavailable_error"}}` — still loading.

Runner: a single `GET /health` check before the call (no retries —
runner decision 5); a `503` fails the execution as "model still
loading".

## 3. Model identity check

`GET /v1/models` returns one entry per served model — all directory
models in router mode, exactly one in single-model mode. Each entry's
`id` is the basename/alias described in §1. The runner checks that the
DB record's `model_identifier` is **among** the returned ids (a subset
check, not an exact single-id match): a mismatched server config fails
cleanly instead of producing garbage (the execution fails with the ids
the server actually serves).

## 3a. Model catalog — `GET /` (llama.cpp-specific, best-effort)

The llama.cpp server also serves its model catalog at `GET /` (the
`models.json` shape). Per loaded model the entry carries:

- `id` — the model id/alias
- `status.value` — `loaded` / `unloaded`
- `status.args` — the **server launch arguments** (`--ctx-size`,
  `--temperature`, `--top-k`, `--flash-attn`, ...): the actual
  server-instance configuration
- `meta` — `n_ctx`, `n_params`, `size`, `ftype` (quantization)

The runner fetches it during preflight and records the matched entry in
the `preflight_passed` log event. This is **best-effort audit data**:
a fetch failure never fails the execution (`"catalog":null` is recorded
instead).

## 4. The call: `POST /v1/chat/completions`

- Headers: `Content-Type: application/json`,
  `Authorization: Bearer <api-key>` (key only required if the server was
  started with `--api-key`; otherwise any value works, e.g.
  `sk-no-key-required`).
- Request (standard OAI fields, plus llama.cpp extras):
  - `model`        – must match the loaded model id/alias
  - `messages`     – `[{role: system|user|assistant, content: ...}]`
  - `max_tokens`, `temperature`, `top_k`, `stream` (use `false`)
  - `response_format`:
    - `{"type": "json_object"}` — plain JSON output
    - `{"type": "json_schema", "schema": {...}}` — **schema-constrained
      generation: this is where a skill's `output_schema` plugs in
      directly**
- Response (non-streaming):
  - `choices[0].message.content` — the text (this is the `raw_response`)
  - `usage` — `prompt_tokens`, `completion_tokens`, `total_tokens`
  - `timings` — llama.cpp-specific: `prompt_ms`, `predicted_ms`,
    `predicted_per_second`, `cache_n`, ...
  - `reasoning_content` — reasoning trace, if the template/model produces
    one
- Non-streaming is fully supported; streaming (SSE) is also supported but
  the runner does not need it in phase 2.

## 5. Errors

OAI-shaped: `{"error": {"code": <http>, "message": "...", "type": "..."}}`

- `401` `authentication_error` — bad/missing API key.
- `503` `unavailable_error` — model still loading / server busy.
- llama.cpp-specific types exist too (`not_supported_error`,
  `invalid_request_error`, ...).

Runner mapping: any non-2xx → `execution_fail` with the error body as
the error string; `EXIT_HTTP` for transport failures. Exit codes
(canonical values in `acta_runner/include/runner.h`): `EXIT_INVALID 4`,
`EXIT_HTTP 12`, `EXIT_TIMEOUT 13`.

## 6. What the runner deliberately does NOT use

`/v1/responses`, `/v1/embeddings`, `/v1/reranking`, multimodal
(`image_url`/`input_audio`/`input_video` content parts), tool calling /
function calling, MCP servers, `/slots` cache save/restore, `/metrics`.
LoRA adapters and speculative decoding are **not a priority** for ACTA
Gamma; revisit only if a later feature genuinely needs them.
