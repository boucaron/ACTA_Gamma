# ACTA Gamma

![Logo](assets/logo.jpg)

**LLMs as actions, not agents.**

A small, stateless LLM execution engine for versioned skills and reproducible analysis.

> **The engine decides what happens. The LLM only does the work it's asked to do.**

The C targets build with plain `make` on Windows (MinGW/MSYS2), Linux (gcc/clang), and macOS (Xcode clang); the Qt 6 GUI additionally needs `qmake6` on any of those platforms.

## What is ACTA Gamma?

ACTA Gamma treats an LLM as a single controlled action in a larger deterministic flow.

```text
Context + Skill + Model
              │
              ▼
         One LLM Call
              │
              ▼
         Observation
```

The engine controls the execution. The LLM does not orchestrate itself, maintain state, delegate work, or decide what happens next. ACTA Gamma is deliberately not an agent framework — the full point of view is in [`docs/PointOfView.md`](docs/PointOfView.md).

Terms used throughout this README: a **skill** is a versioned prompt template with an optional output schema — it is not a tool, function, or agent capability; a **context** is a named, immutable snapshot of input data (a document, a code file, a log excerpt) — it is not the model's prompt window.

## Core ideas

* **Stateless** — every execution is independent and one-shot.
* **Versioned skills** — prompts and output schemas are revisioned.
* **Immutable contexts** — the exact input can be retained for replay.
* **Multi-model** — the backend serves several models; any model served by the llama.cpp router can be registered as a model record.
* **Auditable** — executions retain prompts, raw responses, results, errors, and execution events.
* **Replayable** — a replay reproduces the request inputs exactly when it reuses the same context, skill revision, model revision, and execution prompt. Output equivalence additionally depends on backend determinism and the model weights behind the model's `base_url`, which the system does not track.
* **Generic** — suitable for review, analysis, classification, extraction, auditing, and similar tasks.

## Architecture

```text
                 ┌──────────────┐
                 │    Context   │
                 └──────┬───────┘
                        │
                 ┌──────▼───────┐
                 │ Skill @ N    │
                 └──────┬───────┘
                        │
                 ┌──────▼───────┐
                 │  Model @ N   │
                 └──────┬───────┘
                        │
                 ┌──────▼───────┐
                 │  Execution   │
                 └──────┬───────┘
                        │
             ┌──────────┴──────────┐
             ▼                     ▼
        Observation              Audit
```

## Revisions and lifecycle

The whole lifecycle: create/update the parent, revisions are snapshotted automatically, executions reference revision ids. Concretely:

- **How revisions are created.** A DB trigger inserts a new revision row (per-parent sequence 1, 2, 3, …) every time the parent row is created, updated, or soft-deleted (`acta_cli skill create` / `skill update` / `skill delete`, same for `model`). The soft-delete trigger snapshots a final revision carrying `deleted_at`. There is no separate "snapshot" command.
- **Immutability.** Revision rows cannot be edited or deleted — they can only be read (`skill_revision get` / `get-latest` / `list` / `count`, same for `model_revision`). Contexts are likewise immutable (a trigger rejects updates), which is what makes replay inputs exact.
- **Execution binding.** An execution binds to explicit `skill_revision_id` and `model_revision_id`, and its final user message is `execution.prompt + "\n\n" + context.content`. The request inputs are exactly reproducible only with all four inputs: the context, the skill revision, the model revision, and the execution-level prompt.
- **Replay caveat.** Output equivalence additionally depends on backend determinism and the model weights behind the model's `base_url`, which the system does not track.
- **No promote / deprecate.** There is deliberately no `active` or `current` flag: the "current" revision is simply the latest one, and choosing what to run is done by pointing the execution at the revision id you want.
- **Editing creates, not modifies.** Updating a skill or model parent inserts a new immutable revision; it does not modify the existing one, and existing executions keep pointing at the revision they were bound to.

## How a run is assembled

The runner assembles the chat call from the bound revisions:

- `system` = `skill.prompt_template` (from the bound skill revision)
- `user`   = `execution.prompt` (the optional per-execution instruction given at `exec create`) + `\n\n` + `context.content` (from the bound context); when the execution has no `prompt`, the user message is just the context content

So `execution.prompt` is not a second template — it is an optional per-execution instruction layered on top of the skill's prompt template, and it is stored on the execution record so the audit trail shows exactly what was asked.

## Implementation

The implementation is C/C++ on top of SQLite:

| Component | Language | Description |
|---|---|---|
| `acta_db/` | C11 | SQLite persistence library (`libacta_db`) — skills, skill folders, skill revisions, models, model folders, model revisions, contexts, executions, execution logs ([schema design: `docs/DBDesign.md`](docs/DBDesign.md)) |
| `acta_cli/` | C11 | Command-line client (`acta_cli`) over `acta_db` (uses cJSON for output) |
| `acta_runner/` | C11 | Standalone LLM execution runner (`acta_runner`) — drives pending executions against the model's OpenAI-compatible backend: claim → resolve → preflight → chat call → record → complete/fail, with `execution_log` phase rows (uses curl + cJSON) |
| `acta_gui/` | C++ / Qt 6 (Core, Widgets) | Desktop GUI: manage skills, models, contexts, review executions, and run them (the in-app "Run" button runs the runner's pipeline in-process — it directly compiles and reuses the runner's own source files `acta_runner/src/run.c` and `acta_runner/src/backend.c` via its qmake project, no `acta_runner` binary needed; there is a single pipeline codebase, not a second copy of the pipeline logic) |

The backend is a llama.cpp `llama-server` running in **router mode** (launched without a model, e.g. with `--models-dir` pointing at local GGUF files): an OpenAI-compatible endpoint that serves several models and routes each request to the matching model instance. A model record stores `backend`, `base_url`, `model_identifier`, and a JSON configuration blob. The runner reads the keys `api_key`, `temperature`, `max_tokens`, `top_k`, and `supports_response_format`; any deviation from that contract — an unknown key (typo), a wrong value type, or a malformed blob — fails the execution with `EXIT_INVALID` instead of silently falling back to backend defaults.

**Preflight** (the `preflight` step of the pipeline) verifies the backend before the chat call: `GET /health` must return 200 (503 means the model is still loading → execution `failed`), `GET /v1/models` must list the model record's `model_identifier` (if not, the execution fails with the ids the server actually serves), and the matched entry's `max_context` is read. As a best-effort audit step, the router's model catalog (`GET /`, models.json format) records the matched model's launch args and meta (`n_ctx`, `n_params`, `size`, `ftype`, …) into the execution timeline, so the server-instance configuration is part of the audit trail — the same model id can be served under different server flags. Success is logged as `preflight_passed`.

**Output-schema validation** is post-hoc: it runs only when the backend did not apply the schema itself, i.e. when `supports_response_format` is false (the schema is then checked against the raw response after the call). If the response does not match the skill's `output_schema`, the execution **fails** with a `validation_failed` log row (`EXIT_INVALID`) — there is no "complete with a flag" mode. The validator is a hand-rolled subset check, not full JSON Schema.

Concurrency: the SQLite connection uses WAL journal mode, and the runner's claim step is an optimistic `UPDATE … WHERE status = 'pending'` (checked for affected rows), so two runner processes cannot claim the same execution. Sequential use is the normal pattern; parallel runners are safe for claiming (the claim step guarantees two processes cannot grab the same execution), but interleaving two runners over the same batch is not supported, since per-execution ordering is not guaranteed.

### Building

See [`docs/building.md`](docs/building.md) for dependencies, platform-specific setup (MinGW/MSYS2, Linux, macOS), the per-component `make` steps, and the top-level wrapper (`make all`, `make test`).

## Minimal end-to-end example

Against a running llama.cpp `llama-server` in router mode (e.g. on `127.0.0.1:8080`):

```sh
# 1. Register the model
acta_cli model create --json '{"name":"llama-local","backend":"openai","base_url":"http://127.0.0.1:8080","model_identifier":"qwen3-8b"}'

# 2. Create a versioned skill (prompt template + optional output schema)
acta_cli skill create --json '{"name":"sentiment","prompt_template":"Classify the sentiment of the input. Reply with JSON: {\"label\": \"positive\"|\"negative\", \"confidence\": number}"}'

# 3. Create an immutable context (the input snapshot)
# ("hash" is optional here: when omitted it is derived as SHA-256 of the content)
acta_cli context create --json '{"type":"text","content":"The build system shipped on time and the release went smoothly."}'

# 4. Create an execution binding context + skill revision + model revision
# the "prompt" field is optional; omit it to send just the context
acta_cli exec create --json '{"prompt":"What is the sentiment of the context?","context_id":1,"skill_revision_id":1,"model_revision_id":1}'

# 5. Run it (hard per-call HTTP timeout: --timeout, default 300 s)
acta_runner run 1

# 6. Inspect the result and the audit trail
acta_cli exec get 1
acta_cli log list 1
```

## Current status

Early prototype / POC. See [`docs/status.md`](docs/status.md) for what is done and what is not yet implemented.

## License

BSD Zero Clause License (BSD-0-Clause).

## Third-party code

The SHA-256 implementation in `acta_cli/src/sha256.c` is taken from
[Brad Conte's crypto-algorithms](https://github.com/B-Con/crypto-algorithms/tree/master)
(`sha256.c`). This code is released into the public domain free of any
restrictions. The author requests acknowledgement if the code is used,
but does not require it. This code is provided free of any liability and
without any quality claims by the author.
