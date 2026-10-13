# Plan — backend dialects: `llamacpp` mode and `openai` mode

Status: **implemented, verified and COMMITTED (2026-10-12)** —
§9 holds the four answers (round 2); plan phases 0–6 and 8 are in git
(`9786d3c` portability fixes, `52dd03c` Phases 1–4 + 6, `cb2be70` Phase 5,
`7810e7c` Phase 7 docs, `083dd71` harness fix) and green on Windows/MinGW and
WSL/Ubuntu (`acta_runner` 13 suites: 681 checks / 0 failures on Windows, 712 / 0
on WSL; `acta_cli` 17/17 PASS on both; ASAN clean apart from the known libcurl
global-init leak; acceptance criteria 1–8 verified, including live end-to-end
against Strata with an `output_schema` skill). Phase 7 (GUI) is written
(`src.pro` + `modelDialog`), **compiled** (debug + release, zero warnings,
2026-10-12) and **committed** (`bb05950` + the `dialect.h` include-path fix
`e6d45ea`); it is still not buildable on WSL — no Qt 6 in `insightcae` — and
running it there remains the owner's. Progress and gotchas:
[`docs/session_dialect_progress.md`](../session_dialect_progress.md) (§10–§17).
Depends on:
[`docs/session_strata_backend.md`](../session_strata_backend.md) (issues A and B
are exactly what this plan resolves).

> **Decision record (2026-10-12):** the dialect is a property of the **model
> revision**. A CLI flag exists — but only where no model row exists, or as a
> *logged override*. See §3.2. Replay semantics are unchanged.

---

## 0. My opinion first

**Yes, it is a good idea — with three conditions.**

1. **Call them *dialects*, not backends.** The DB column is already named
   `backend` and it is currently *meaningless*: the runner never reads it. What
   actually varies between servers is the **HTTP dialect** (preflight paths,
   catalog shape, `response_format` envelope, parameter names), not the model
   or the process. So: keep the column, give it a closed vocabulary.
2. **Exactly two dialects, plus capability flags.** `llamacpp` and `openai`.
   Resist `ollama`, `vllm`, `lmstudio`, `anthropic`… each new name is a new
   permanent contract to test and document. Differences that are not dialect
   differences (this server has no `json_schema` support, that one needs a
   longer timeout) belong in `model.configuration`, which already has a strict
   known-keys rule.
3. **The naming/migration problem must be solved before the code.** Today the
   README tells users to register a local `llama-server` with
   `"backend": "openai"`, while the runner implements the **llama.cpp** dialect.
   If `openai` suddenly means "strict OpenAI", every existing registration
   silently changes behavior. §4 is the decision to make.

What it is **not**: a plugin system, a server-detection mechanism, or a step
toward multi-provider support. It stays two hard-coded dialects behind one
small internal module.

---

## 1. Current state (verified facts)

| Fact | Where |
|---|---|
| `models.backend TEXT NOT NULL`, copied into every `model_revisions` row | `acta_db/schema.sql`, `acta_db/migrations/0.1.sql` |
| `backend` is stored, listed, updated, printed — and **never read by the runner** | `acta_cli/src/json.c:203`, `acta_cli/src/commands/model.c`; `acta_runner/src/run.c` uses only `base_url`, `model_identifier`, `configuration` |
| Preflight is hard-coded llama.cpp: `GET /health` → `GET /v1/models` | `acta_runner/src/backend.c` `backend_preflight()` |
| Catalog audit is hard-coded llama.cpp: `GET /` (`models.json` shape), best-effort, `"catalog":null` on failure | `acta_runner/src/run.c:876-939` |
| `max_context` is read from the matched `/v1/models` entry only | `acta_runner/src/backend.c:349` |
| `response_format` envelope is llama.cpp-shaped: `{"type":"json_schema","schema":{…}}` | `acta_runner/src/run.c:967-974` |
| Request params sent: `model`, `messages`, `temperature`, `max_tokens`, `top_k`, `stream` | `acta_runner/src/run.c:951-975` |
| `model.configuration` known keys (strict, unknown key = `EXIT_INVALID`): `temperature`, `max_tokens`, `top_k`, `supports_response_format` | `acta_runner/src/run.c:699-775` |
| Response read: `choices[0].message.content`, `usage.{prompt,completion,total}_tokens` | `acta_runner/src/run.c:1020-1045` |
| README example registers llama-server as `"backend": "openai"` | `README.md` |
| Strata rejects ACTA's envelope: `HTTP 400 response_format.json_schema needs a schema object` | verified live (session doc, issue A) |
| Strata reports `meta.n_ctx` in `/v1/models` and `max_context` in `/health` → ACTA logs `max_context: 0` | verified live (session doc, issue B) |

---

## 2. Dialect matrix (what actually differs)

| Concern | `llamacpp` (current behavior) | `openai` (Strata, OpenAI, vLLM, …) |
|---|---|---|
| `GET /health` | `{"status":"ok"}`; `503` = model loading | same shape accepted; Strata adds `max_context`, `model`, `service` |
| `GET /v1/models` | one entry per served model, `id` = file basename/alias | same |
| Context ceiling | entry `max_context` (router) | entry `meta.n_ctx` (Strata) — **accept both, prefer the dialect's** |
| Catalog audit | `GET /` (`models.json`: `status.args`, `meta`) | no such endpoint → skip the call, record `"catalog":null` (or `/props`-like endpoint later) |
| Structured output | `{"type":"json_schema","schema":{…}}` | `{"type":"json_schema","json_schema":{"name":…,"strict":true,"schema":{…}}}` |
| `top_k` in the request | accepted | not part of the OpenAI chat spec → omit (or send and accept that servers ignore it) |
| Response extras | `timings`, `reasoning_content` | `system_fingerprint`, `refusal` |
| Error body | `{"error":{"code","message","type"}}` | same shape |
| Auth | `Authorization: Bearer <OPENAI_API_KEY>` | identical |

Everything else (one call, no retries, no streaming, `raw_response` =
`choices[0].message.content`, exit codes, logging phases) is shared and must
stay shared.

---

## 3. Design

### 3.1 One internal module

New files `acta_runner/src/dialect.h` / `dialect.c`:

```c
typedef enum { DIALECT_LLAMACPP = 0, DIALECT_OPENAI = 1, DIALECT_UNKNOWN = -1 } dialect_id_t;

typedef struct {
    dialect_id_t id;
    const char  *name;              /* "llamacpp" | "openai" */
    int          fetch_catalog;     /* 1 = GET / during preflight */
    int          sends_top_k;       /* 1 = include top_k in the request */
    /* response_format envelope builder for this dialect */
    cJSON *(*build_response_format)(const cJSON *schema);
    /* context ceiling extraction */
    long (*max_context_of)(const cJSON *model_entry, const cJSON *health_body);
} dialect_t;

dialect_id_t dialect_from_string(const char *s);      /* NULL/unknown -> DIALECT_UNKNOWN */
const dialect_t *dialect_get(dialect_id_t id);
```

`run.c`, `backend.c` and `check.c` receive a `const dialect_t *` and stop
hard-coding paths and shapes. No function-pointer sprawl beyond the two
genuinely varying items; the rest are plain flags.

### 3.2 Decision — where the dialect comes from

**Adopted rule:** the dialect belongs to the **model revision**. The CLI flag is
an escape hatch, not the normal path.

| Surface | Dialect source | Notes |
|---|---|---|
| `acta_runner run <id>` | `model_revision.backend` | authoritative; no flag needed |
| `acta_runner run --pending` | `model_revision.backend`, **per row** | `--dialect` is rejected (`EXIT_INVALID`, no claim, no DB writes) — one flag cannot describe a batch of different model revisions |
| `acta_runner check <model-record-id>` | `model_revision.backend` | |
| `acta_runner check --base-url … --model-identifier …` | `--dialect` (default `llamacpp`) | standalone mode has **no model row**, so the dialect has nowhere else to come from |
| `acta_gui` **Run** button | `model_revision.backend` | the GUI calls `run_execution()` in-process, there is no argv; keeping the source identical prevents CLI/GUI drift |
| `acta_cli model create/update --backend` | the operator declares it | stored in `models`, copied into every `model_revisions` row → changing dialect creates a new revision, old executions stay tied to what they used |

**Why the flag is not the primary source** (the replay core idea):

* the dialect decides the **request body**; if it comes from argv, the same
  execution row + the same model revision can produce two different requests —
  which is exactly the input determinism the revision design exists to guarantee;
* a stored `raw_response` would not say which envelope produced it;
* `--pending` batches mix model revisions;
* the GUI has no argv, so a flag-only design forks the behavior.

**Override on `run <id> --dialect <llamacpp|openai>`** — allowed, deliberately
loud, never silent:

* accepted only with a single explicit execution id; rejected with `--pending`
  and unavailable to the GUI;
* never mutates the DB or the model revision;
* recorded in the `llm_request` log metadata:
  `{"dialect":"openai","dialect_source":"cli-override","model_backend":"llamacpp"}`
  (`dialect_source` is `"model_revision"` in the normal case);
* if it disagrees with the model row, a `warn`-level log event plus a stderr
  warning — the audit trail states which dialect was used *and* that it
  overrode the row;
* replay: the same flag reproduces the request; without the flag the model row
  decides. Documented in `docs/runner_contract.md`.

**Precedence:** `--dialect` (where accepted) → `model_revision.backend` →
otherwise **fail before any network call**, `EXIT_INVALID`:
`model revision N has unsupported backend '<value>' (expected llamacpp or openai)`.
No guessing, no auto-detection, no silent default on `run` — consistent with
ACTA's "fail loudly, record it" style and with the existing strict
`configuration` unknown-key rule.

**Resulting CLI surface:**

```
acta_runner run <id> [--dialect llamacpp|openai]              # logged override
acta_runner run --pending                                     # --dialect rejected
acta_runner check <model-record-id>
acta_runner check --base-url URL --model-identifier ID [--dialect llamacpp|openai]
acta_cli  model create|update … --backend llamacpp|openai     # the declaration site
```

Name it `--dialect`, **not** `--backend`: `--backend` would read like it edits
the model row and collides with `acta_cli model … --backend`.

### 3.3 `configuration` stays the capability layer

Keep `supports_response_format` (escape hatch when a server has no
schema-constrained generation). Add nothing else unless a real need appears.
If a new key is added, it must be added to the `known[]` list in the same
commit — otherwise every model using it fails with "unknown model
configuration keys".

---

## 4. The decision to make: naming and migration

| Option | Description | Pros | Cons |
|---|---|---|---|
| **A — `backend` becomes a closed vocabulary + migration** *(recommended)* | Allowed values: `llamacpp`, `openai`. Add `acta_db/migrations/0.2.sql` that rewrites existing `backend='openai'` rows to `'llamacpp'` (they were llama-server rows), then validate in `acta_cli` and enforce in the runner | One source of truth; honest labels; the migration mechanism already exists (`PRAGMA user_version`); the README example becomes correct | A migration touches existing DBs; `openai` changes meaning for anyone who used it literally |
| **B — keep `backend` free text, add `configuration.dialect`** | `backend` stays a label; the dialect comes from a new config key | No migration | Two sources of truth; a "backend" column that still means nothing; more config keys |
| **C — auto-detect the dialect at runtime** | Probe `/health` body, error strings, `service` field, then choose | Zero config | Non-deterministic, silent, hard to audit — contradicts ACTA's core premise (immutable inputs, reproducible requests). **Reject.** |

Recommendation: **A**, with the migration written as a no-op-safe statement
(`UPDATE models SET backend='llamacpp' WHERE backend='openai'` guarded so a
genuinely-OpenAI server can be re-registered as `openai` afterwards), plus a
CLI validation error listing the allowed values.

Alternative if a migration is unwanted: name the dialects `llamacpp` and
`openai_strict` so no existing value changes meaning. Less elegant, zero risk.

> **Decided (§9): option A's vocabulary, no migration.** No clients and no
> deployed databases exist, so every existing row/seed/fixture/example is simply
> adapted in situ to `llamacpp` and `openai`.

---

## 5. Implementation phases

| Phase | Work | Files |
|---|---|---|
| **0. Decision** | Choose §4 option; write it into `docs/runner_contract.md` as a numbered decision | this doc, `docs/runner_contract.md` |
| **1. Ceiling fix (independent, do first)** | `max_context` fallback: entry `max_context` → entry `meta.n_ctx` → `/health` `max_context`; log which one was used | `acta_runner/src/backend.c` (+ `check.c` output unchanged) |
| **2. Dialect module** | Add `dialect.{h,c}`; `dialect_from_string()`; unit tests | new `acta_runner/src/dialect.{h,c}`, new `acta_runner/tests/dialect/` |
| **3. Wire the request** | `run.c`: build `response_format` through the dialect; gate `top_k`; add `"dialect"` + `"dialect_source"` to the `llm_request` log metadata | `acta_runner/src/run.c` |
| **4. Wire the preflight + the CLI surface** | gate the `GET /` catalog call on `fetch_catalog`; pass the dialect into `backend_preflight()`; `check` standalone mode gains `--dialect` (default `llamacpp`); `run <id> --dialect` override (logged, warn on disagreement); `run --pending --dialect` rejected; argparse + help/usage updates | `acta_runner/src/backend.c`, `check.c`, `run.c`, `argparse.{h,c}`, `main.c` |
| **5. CLI + DB** | validate `backend` on create/update against the canonical vocabulary (`llamacpp` / `openai`); update `--tools` schema output; **no migration** (§9) | `acta_cli/src/commands/model.c`, `acta_cli/src/tools.c` |
| **6. Tests** | `stub_server` gains a dialect parameter (rejects the wrong envelope, serves `meta.n_ctx`); new `test_dialect`, `test_openai_dialect`; keep all 446 existing runner checks green; e2e against real Strata via `scripts/probe_strata.sh --dialect openai` | `acta_runner/tests/…` |
| **7. GUI** | `acta_gui` compiles `run.c`/`backend.c` directly → rebuild; model editor must show/accept the new vocabulary | `acta_gui/src/…` |
| **8. Docs** | rename/extend the backend contract doc; CLI spec vocabulary; README examples (one `llamacpp`, one `openai`/Strata); `docs/building.md` WSL section | `docs/llamacpp_server_contract.md`, new `docs/openai_dialect_contract.md`, `docs/cli_spec.md`, `docs/DBDesign.md`, `README.md` |

Phase 1 is worth landing on its own: it is small, fixes a real defect
(`max_context: 0` disables the `max_chars` ceiling), and needs no decision.

---

## 6. Acceptance criteria

1. A model registered `backend:"llamacpp"` behaves **identically** to today
   (same preflight calls, same request body, same log events — the
   `llm_request` metadata only *gains* the additive `dialect` /
   `dialect_source` keys) — proven by the existing suites staying green.
2. A model registered `backend:"openai"` against the live Strata server
   completes **with an `output_schema` skill** (today it 400s).
3. `max_context` is non-zero for both dialects, and the `max_chars` preflight
   uses it.
4. An unsupported `backend` value fails **before** any HTTP call with an
   explicit message and `EXIT_INVALID`, and the failure is recorded.
5. `llm_request` metadata records the dialect used, so an audit can tell which
   envelope produced a stored `raw_response`.
6. `acta_runner check` reports the same verdicts in both dialects.
7. `run <id> --dialect X` records `{"dialect":X,"dialect_source":"cli-override","model_backend":<row value>}` in `llm_request` metadata and logs a `warn` when they differ; `run --pending --dialect X` exits `EXIT_INVALID` (4) with **no claim and no DB writes**.
8. `check --base-url … --model-identifier …` defaults to `llamacpp`, i.e. today's standalone behavior is unchanged.

---

## 7. Risks

| Risk | Mitigation |
|---|---|
| Existing DBs silently change meaning | migration (§4 A) **or** non-colliding names; plus a `model get` output that shows the resolved dialect |
| llama.cpp version drift (docs pin 0.5.0) | keep `supports_response_format:false` as the escape hatch; document the tested version |
| Strict `configuration` unknown-key rule breaks on new keys | add new keys to `known[]` in the same commit; test it |
| GUI drift (it compiles the runner sources directly) | rebuild `acta_gui` in the same phase; run its build in CI/local |
| Two dialects slowly accreting flags until they are indistinguishable | keep the matrix in §2 as the single reference; a new flag must appear in the matrix first |
| `openai` name implies "OpenAI the vendor" | document that it means "strict OpenAI-compatible HTTP dialect", not the hosted API |

---

## 8. Explicit non-goals

* No plugin/registry system, no runtime-loaded adapters.
* No dialect auto-detection (see §4 option C).
* No new dialects for Ollama / vLLM / LM Studio / Anthropic / hosted OpenAI —
  they either already speak one of the two dialects or are out of scope.
* No streaming, no tool calling, no `/v1/responses`, no embeddings.
* No change to the execution lifecycle, exit codes, logging phases, or replay
  semantics.

---

## 9. Questions for you (answer before Phase 2)

**Resolved:** the dialect comes from `model_revision.backend`; `--dialect` exists
only for `check` standalone mode and as a logged override on `run <id>` (§3.2).

### Decision record (2026-10-12, round 2) — all four answered, nothing open

| # | Question | **Decision** | Consequence |
|---|---|---|---|
| 1 | Migration or new names? | **Neither: option A's vocabulary (`llamacpp` + `openai`) with NO `0.2.sql`.** There are no clients and no deployed databases, so the vocabulary is adapted **in situ** — seeds, fixtures, examples and docs move to the canonical names in the same commits. The schema stays frozen at `0.1`. | `acta_runner/tests/**` reseed `backend = "llama"` -> `"llamacpp"`; `acta_cli` model tests using `vllm` / `together` / `openai` move to the canonical vocabulary; README + docs examples become `llamacpp` (local llama-server) and `openai` (Strata). `llama` is **not** an alias: `dialect_from_string("llama") == DIALECT_UNKNOWN`. |
| 2 | Validate `backend` in `acta_cli`? | **Yes.** `acta_cli model create/update` rejects a `backend` outside `llamacpp` / `openai` with `EXIT_INVALID`, naming the accepted vocabulary. The runner re-validates at run time (defense for rows written by another tool or an older build). | One source of the vocabulary: `dialect_expected_names()` -> "llamacpp or openai". |
| 3 | `top_k` in `openai` mode? | **Omit it** (`sends_top_k = 0`). | `model.configuration.top_k` is still validated — a bad value still fails the execution — it is simply not sent on this dialect, and the `llm_request` metadata records the dialect, so the omission is explainable from the audit trail. |
| 4 | Catalog audit in `openai` mode? | **Skip it** (`fetch_catalog = 0`): no `GET /` call, `preflight_passed` records `"catalog": null`. No `/props` probe — adopting a non-standard endpoint would be a new permanent contract for no audit gain today. | One fewer request per execution on the `openai` dialect; the `"catalog":null` shape is unchanged, so existing log readers keep working. |
| 5 | Is a `0.2.sql` migration acceptable? | **Not needed** (see 1). If a real deployment ever appears it becomes a documented manual `model update`. | `acta_db/migrations/` stays at `0.1.sql`. |

Two implementation details settled in the same round:

* the openai envelope's `json_schema.name` is the fixed constant **`acta_output`**
  (never derived from the skill name or id, so the request body stays
  replay-deterministic);
* `max_context_of()` takes an extra `const char **source` out-parameter, so the
  "which field produced the ceiling" label required by Phase 1 survives moving
  the extraction into the dialect.

---

## 10. Suggested commit split

1. `backend.c`: `max_context` fallback (`max_context` → `meta.n_ctx` → `/health`) + test.
2. `dialect.{h,c}` + unit tests (no behavior change yet).
3. `run.c` + `backend.c` + `check.c`: dialect wired in; `llamacpp` path unchanged; `dialect` / `dialect_source` in the log metadata.
4. `--dialect` flag: `check` standalone + `run <id>` override + `--pending` rejection, with tests.
5. `openai` envelope + `openai` dialect tests (stub server rejects the wrong shape).
6. `acta_cli` validation (+ migration if option A) + `--tools` schema.
7. Docs + README + GUI rebuild.

**Landed differently (2026-10-12):** whole-file staging forces a file into the
earliest phase that touches it, and `backend.{c,h}`, `run.c`, `check.c`,
`Makefile`, `stub_server.{h,c}`, `test_run.c`, `test_check.c`,
`test_autopreflight.c` each carry several phases — so items 1–5 became ONE
`runner:` commit and the series is 7 commits, recorded with per-commit
worktree verification in `docs/session_dialect_progress.md` §12.1–§12.2.
