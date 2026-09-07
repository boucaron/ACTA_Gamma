# llama.cpp server — runner-relevant contract

Distilled from the llama.cpp server README (the full auto-generated
server reference in the upstream llama.cpp repository; only the
runner-relevant subset is kept here).

## 1. Server startup (model load = process start)

```
llama-server.exe -m models\my-model.gguf -c 2048
```

- Loads the GGUF model at startup; listens on `127.0.0.1:8080` by default.
- Relevant flags:
  - `-m <file>`        model path (this also becomes the default model `id`)
  - `--alias <name>`   custom model `id` (use this so the DB
                       `model_identifier` is stable, not a file path)
  - `--port / --host`  endpoint the DB `base_url` points at
  - `-c <n>`           prompt context size
  - `--api-key KEY`    if set, requests need `Authorization: Bearer <key>`;
                       if not set, any/absent key is accepted
  - `-j <json-schema>` schema-constrained generation at the server side
  - `-n <n>`           max tokens to predict
- **One server = one model.** Loading/unloading = starting/stopping the
  process; swapping a model = restarting with a different `-m`.

## 2. Readiness check

`GET /health` (also `/v1/health`), public, no key:

- `200` → `{"status": "ok"}` — model loaded, ready.
- `503` → `{"error": {"code": 503, "message": "Loading model",
  "type": "unavailable_error"}}` — still loading.

Runner: a single `GET /health` check before the call (no retries —
runner decision 5); a `503` fails the execution as "model still
loading".

## 3. Model identity check

`GET /v1/models` returns exactly one model. Its `id` is the `-m` path
unless `--alias` was given. Runner should compare the DB
`model_identifier` against this so a mismatched server config fails
cleanly instead of producing garbage.

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
non-llama OpenAI-compatible backends have no catalog, so a failure
there never fails the execution (`"catalog":null` is recorded instead).

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
the error string; `EXIT_HTTP` for transport failures.

## 6. What the runner deliberately does NOT use

`/v1/responses`, `/v1/embeddings`, `/v1/reranking`, multimodal
(`image_url`/`input_audio`/`input_video` content parts), tool calling /
function calling, MCP servers, `/slots` cache save/restore, `/metrics`,
LoRA adapters, speculative decoding. Revisit only if a later feature
needs them.
