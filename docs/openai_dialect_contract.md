# OpenAI-compatible dialect — runner-relevant contract

The contract of the runner's second backend dialect: a server that speaks the
**strict OpenAI `chat/completions` HTTP dialect**. `acta_runner` selects it
from the model revision's `backend` value `openai` (runner decision 9 in
[`runner_contract.md`](runner_contract.md); module
`acta_runner/src/dialect.{h,c}`; matrix in
[`archive/backend_dialects.md`](archive/backend_dialects.md) §2).

> **`openai` names a dialect, not a vendor.** It means "strict
> OpenAI-compatible HTTP dialect" — Strata, vLLM, and any other
> OpenAI-compatible server qualify. It does **not** mean "the hosted OpenAI
> API", and it does not imply any OpenAI-specific endpoint, auth or SDK.
>
> Tested against **Strata** (`service: "strata"`, llama.cpp/ggml-derived,
> `serve/server.py --engine strata --port 8080`) serving
> `qwen3.8-flash-next-iq3_xxs`. Compatibility with other OpenAI-compatible
> servers is expected but not measured here — the surface ACTA uses is the
> narrow one below.

The sibling document is [`llamacpp_server_contract.md`](llamacpp_server_contract.md)
(the `llamacpp` dialect: a local `llama-server` router). Everything not listed
as different below is shared: one call, no retries, no streaming,
`raw_response = choices[0].message.content`, the exit-code space, the log
phases.

---

## 1. Registering an `openai`-dialect model

```bash
acta_cli model create --json '{
  "name": "strata-local",
  "backend": "openai",
  "base_url": "http://127.0.0.1:8080",
  "model_identifier": "qwen3.8-flash-next-iq3_xxs"
}'
```

`backend` is a closed vocabulary — `llamacpp` or `openai`, exact and
case-sensitive (`llama`, `vllm`, `OpenAI`, `ollama` are rejected by `acta_cli`
with `invalid backend '<value>' (expected llamacpp or openai)`, and the runner
re-validates at run time before any HTTP call). Changing `backend` is a
`model update`, which creates a new model revision: old executions stay tied
to the dialect they actually used.

`model.configuration` stays the capability layer in both dialects:
`temperature`, `max_tokens`, `top_k`, `supports_response_format` (unknown keys
→ `EXIT_INVALID`).

---

## 2. Readiness check — `GET /health`

Same request as the `llamacpp` dialect, same interpretation:

- `200` → ready. Strata's body is richer than llama.cpp's
  `{"status":"ok"}`:

  ```json
  {"status":"ok","max_context":204800,"model":"qwen3.8-flash-next-iq3_xxs",
   "images":false,"api_key":false,"loaded":true,"service":"strata"}
  ```

  ACTA reads `status` only. `max_context` here is the **last-resort** context
  ceiling (§3).
- `503` → "model still loading" (same meaning as in `llamacpp`).
- Connection refused / DNS / timeout / any other non-200 → "server
  unreachable".

`Authorization: Bearer <key>` is sent when a key is configured (identical
policy to the `llamacpp` dialect: `$OPENAI_API_KEY`, else the config file's
`"api_key"`).

## 3. Model identity + context ceiling — `GET /v1/models`

One entry per served model, `id` matched against `model_identifier` (subset
check, same as `llamacpp`).

The dialect difference is **where the context window is published**:

| source | order on `openai` | order on `llamacpp` |
|---|---|---|
| entry `meta.n_ctx` | **1st** | 2nd |
| entry `max_context` (top level) | 2nd | 1st |
| `/health` `max_context` | 3rd | 3rd |

Strata's entry carries `meta.n_ctx` and **no** top-level `max_context`, so the
first source is the one that yields `204800`. Whichever field is used is
recorded in `preflight_passed` as `max_context_source`
(`entry.meta.n_ctx` | `entry.max_context` | `health.max_context` | `none`),
and the value feeds the `max_chars` prompt-size guard. A ceiling of `0`
(`none`) is recorded as such — it does not fail the run, it just means the
server published no window.

## 4. No model catalog — `GET /` is not requested

`llama-server` publishes its model catalog (launch `status.args`, `meta`) at
`GET /`. Strict OpenAI-compatible servers have no such endpoint, so on this
dialect the call is **not made at all** (no pointless 404 per execution) and
`preflight_passed` records `"catalog":null` — the same shape a `llamacpp`
server without a catalog already produced, so log readers are unaffected.
`acta_runner/tests/run/test_openai_dialect.c` pins `GET /` == 0 requests on
this dialect and == 1 on `llamacpp`.

No non-standard substitute (e.g. `/props`) is probed: adopting one would be a
new permanent contract for no audit gain.

## 5. The call — `POST /v1/chat/completions`

Body ACTA sends for a skill with an `output_schema` on this dialect
(`cJSON_PrintUnformatted`, hence no spaces):

```json
{"model":"qwen3.8-flash-next-iq3_xxs",
 "messages":[{"role":"system","content":"<skill.prompt_template>"},
             {"role":"user","content":"<context.content>"}],
 "temperature":0.7,
 "max_tokens":400,
 "response_format":{"type":"json_schema",
                    "json_schema":{"name":"acta_output","strict":true,
                                   "schema":{ …skill.output_schema… }}}}
```

- `temperature` / `max_tokens` appear only when `model.configuration` sets
  them.
- **`top_k` is not sent** on this dialect: it is not part of the OpenAI chat
  body. `model.configuration.top_k` is still *validated* (a bad value fails
  the execution either way), it is simply omitted from the request — and the
  `llm_request` metadata records the dialect, so the omission is explainable
  from the audit trail.
- `stream` is not sent (non-streaming is the only mode ACTA implements).
- `response_format` is omitted entirely when the skill has no
  `output_schema`, or when `model.configuration` sets
  `supports_response_format: false` (§6).
- `json_schema.name` is the fixed constant **`acta_output`** — never derived
  from the skill name or id, so the same skill + model revision always
  produces the same request body (the replay property the revision design
  exists to guarantee).

## 6. Structured output: the envelope *is* the dialect difference

| dialect | envelope |
|---|---|
| `llamacpp` | `{"type":"json_schema","schema":{…}}` |
| `openai` | `{"type":"json_schema","json_schema":{"name":"acta_output","strict":true,"schema":{…}}}` |

Each side rejects the other's shape. Sending the `llamacpp` envelope to an
OpenAI-dialect server is the failure ACTA used to have for every skill with an
`output_schema`; Strata answers, verbatim:

```json
{"error":{"type":"invalid_request_error",
          "message":"response_format.json_schema needs a schema object"}}
```

→ HTTP 400 → the execution fails with `EXIT_HTTP` (12) and the row error
`backend HTTP 400: {"error":…}`. `acta_runner/tests/stub_server.c` reproduces
both directions deterministically (`stub_config_t.dialect`), and
`scripts/probe_strata.sh` step 4b reproduces them live.

Escape hatch (unchanged in both dialects): `model.configuration
.supports_response_format: false` sends **no** `response_format` at all and
validates the stored `raw_response` post-hoc instead
(`validation_started` / `validation_failed` log events).

## 7. Response reading

Identical to the `llamacpp` dialect: `choices[0].message.content` becomes the
`raw_response` (and the `result` after validation), `usage.prompt_tokens` /
`completion_tokens` / `total_tokens` go to the `llm_response` metadata.
Extras a server may add — `system_fingerprint`, `refusal`,
`reasoning_content`, `timings` — are ignored, not stored, not required.

## 8. Errors

OpenAI-shaped error bodies, same as llama.cpp:
`{"error":{"code":<http>,"message":"…","type":"invalid_request_error"}}`.
Any non-2xx → the execution fails with `EXIT_HTTP` (12) and the error string
`backend HTTP <status>: <first 300 chars of the body>`. Transport failures →
`EXIT_HTTP` (12), timeouts → `EXIT_TIMEOUT` (13). No retries in either
dialect.

## 9. What the runner deliberately does NOT use

`/v1/responses`, `/v1/messages`, `/v1/embeddings`, `/v1/reranking`, streaming
(SSE), tool / function calling, multimodal content parts, image generation.
Same non-goal list as [`llamacpp_server_contract.md`](llamacpp_server_contract.md) §6.

## 10. How this contract is verified

- **Unit:** `acta_runner/tests/dialect/test_dialect.c` — the envelope shapes,
  the capability flags, the ceiling order and its `source` label.
- **End-to-end against a stub that speaks the dialect:**
  `acta_runner/tests/run/test_openai_dialect.c` — the OpenAI envelope accepted,
  the `llamacpp` envelope refused (400), `top_k` omitted, zero `GET /` calls,
  `meta.n_ctx` ceiling, the `dialect` / `dialect_source` / `model_backend`
  metadata.
- **Live:** `scripts/probe_strata.sh [--dialect llamacpp|openai]` (steps 1–4b +
  the WSL cross-side check) and `scripts/wsl_strata_demo.sh` (full
  `db init → model → skills → context → exec → check → run` against Strata,
  including a skill with an `output_schema`). Both run from Windows
  (`http://127.0.0.1:8080`) and from WSL over NAT
  (`http://<windows-host-ip>:8080`, never `127.0.0.1`) — see
  [`building.md`](building.md).
