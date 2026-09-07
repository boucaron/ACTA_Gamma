# ACTA Gamma

![Logo](assets/logo.jpg)

**LLMs as actions, not agents.**

A small, stateless LLM execution engine for versioned skills and reproducible analysis.

> **The engine decides what happens. The LLM only does the work it's asked to do.**

The C targets build with plain `make` on Windows (MinGW/MSYS2) and Linux (gcc/clang); the Qt 6 GUI additionally needs `qmake6` on either platform.

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

The engine controls the execution. The LLM does not orchestrate itself, maintain state, delegate work, or decide what happens next.

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

Skills and models are versioned by automatic snapshots: a DB trigger inserts a new revision row (per-parent sequence 1, 2, 3, …) every time the parent row is created or updated (`acta_cli skill create` / `skill update`, `model create` / `model update`). Revision rows are **immutable** — they can be read (`skill_revision get` / `get-latest` / `list` / `count`, same for `model_revision`) but not edited or deleted. Contexts are likewise immutable (a trigger rejects updates), which is what makes replay inputs exact.

An execution binds to explicit `skill_revision_id` and `model_revision_id`, and its final user message is `execution.prompt + "\n\n" + context.content` — so the request inputs are exactly reproducible only with all four inputs: the context, the skill revision, the model revision, and the execution-level prompt (output equivalence additionally depends on backend determinism and the model weights behind the model's `base_url`, which the system does not track). There is deliberately no `promote` / `deprecate` / `active` marking: the "current" revision is simply the latest one, and choosing what to run is done by pointing the execution at the revision id you want. That is the whole lifecycle — create/update the parent, revisions are snapshotted automatically, executions reference revision ids. Editing a skill or model therefore creates a new immutable revision; it does not modify the existing one, and executions keep pointing at the revision they were bound to.

## How a run is assembled

The runner assembles the chat call from the bound revisions:

- `system` = `skill.prompt_template` (from the bound skill revision)
- `user`   = `execution.prompt` (the optional per-execution instruction given at `exec create`) + `\n\n` + `context.content` (from the bound context); when the execution has no `prompt`, the user message is just the context content

So `execution.prompt` is not a second template — it is an optional per-execution instruction layered on top of the skill's prompt template, and it is stored on the execution record so the audit trail shows exactly what was asked.

## Implementation

The implementation is C/C++ on top of SQLite:

| Component | Language | Description |
|---|---|---|
| `acta_db/` | C11 | SQLite persistence library (`libacta_db`) — skills, skill folders, skill revisions, models, model folders, model revisions, contexts, executions, execution logs |
| `acta_cli/` | C11 | Command-line client (`acta_cli`) over `acta_db` (uses cJSON for output) |
| `acta_runner/` | C11 | Standalone LLM execution runner (`acta_runner`) — drives pending executions against the model's OpenAI-compatible backend: claim → resolve → preflight → chat call → record → complete/fail, with `execution_log` phase rows (uses curl + cJSON) |
| `acta_gui/` | C++ / Qt 6 (Core, Widgets) | Desktop GUI: manage skills, models, contexts, review executions, and run them (the in-app "Run" button runs the runner's pipeline in-process — it directly compiles and reuses the runner's own source files `acta_runner/src/run.c` and `acta_runner/src/backend.c` via its qmake project, no `acta_runner` binary needed; there is a single pipeline codebase, not a second copy of the pipeline logic) |

The backend is a llama.cpp `llama-server` running in **router mode** (launched without a model, e.g. with `--models-dir` pointing at local GGUF files): an OpenAI-compatible endpoint that serves several models and routes each request to the matching model instance. A model record stores `backend`, `base_url`, `model_identifier`, and a JSON configuration blob. The runner reads the keys `api_key`, `temperature`, `max_tokens`, `top_k`, and `supports_response_format`; any deviation from that contract — an unknown key (typo), a wrong value type, or a malformed blob — fails the execution with `EXIT_INVALID` instead of silently falling back to backend defaults.

**Preflight** (the `preflight` step of the pipeline) verifies the backend before the chat call: `GET /health` must return 200 (503 means the model is still loading → execution `failed`), `GET /v1/models` must list the model record's `model_identifier` (if not, the execution fails with the ids the server actually serves), and the matched entry's `max_context` is read. As a best-effort audit step, the router's model catalog (`GET /`, models.json format) records the matched model's launch args and meta (`n_ctx`, `n_params`, `size`, `ftype`, …) into the execution timeline, so the server-instance configuration is part of the audit trail — the same model id can be served under different server flags. Success is logged as `preflight_passed`.

**Output-schema validation** is post-hoc: it runs only when the backend did not apply the schema itself, i.e. when `supports_response_format` is false (the schema is then checked against the raw response after the call). If the response does not match the skill's `output_schema`, the execution **fails** with a `validation_failed` log row (`EXIT_INVALID`) — there is no "complete with a flag" mode. The validator is a hand-rolled subset check, not full JSON Schema.

Concurrency: the SQLite connection uses WAL journal mode, and the runner's claim step is an optimistic `UPDATE … WHERE status = 'pending'` (checked for affected rows), so two runner processes cannot claim the same execution. Sequential use is the normal pattern; parallel runners are safe for claiming (the claim step guarantees two processes cannot grab the same execution), but interleaving two runners over the same batch is not supported, since per-execution ordering is not guaranteed.

### Building

**Dependencies:** C compiler (MinGW or gcc/clang), SQLite 3, Qt 6 (Core, Widgets), cJSON (CLI, runner, GUI), curl (runner, GUI).

```sh
# 1. Database library (also builds and runs its test suite)
cd acta_db
make            # → libacta_db.a / libacta_db.so
make test       # C unit tests

# 2. CLI tool
cd ../acta_cli
make            # → ./acta_cli
make test       # per-entity CLI tests

# 3. Runner (drives pending executions; needs a running
#    llama.cpp llama-server in router mode)
cd ../acta_runner
make            # → ./acta_runner
make test       # pipeline tests against a local stub backend

# 4. GUI
cd ../acta_gui
qmake6 "CONFIG+=debug" acta_gui.pro -o Makefile
make
```

MinGW (MSYS2) setup for development:

```sh
pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-make
pacman -S mingw-w64-x86_64-qt6
pacman -S mingw-w64-x86_64-curl
```

Linux (Debian/Ubuntu):

```sh
sudo apt install build-essential libsqlite3-dev libcjson-dev libcurl4-openssl-dev
# GUI only:
sudo apt install qt6-base-dev
```

The same `make` targets work unchanged on Linux: `CC ?= cc` picks up gcc/clang, `EXEEXT` is empty under POSIX make, and the Makefiles link `-lsqlite3`, `-lcjson`, and `-lcurl` straight from the system packages.

macOS: Xcode command-line tools for the compiler, Homebrew for SQLite 3 and curl (`brew install sqlite curl`). cJSON is not packaged by Homebrew — build it from source and pass `CJSON_DIR=...` / `CJSON_LIB=...` to the CLI and runner Makefiles.

Or, from the repository root, build the three C targets in dependency order with the top-level wrapper:

```sh
make all     # acta_db -> acta_cli -> acta_runner
make test    # all three C test suites
make -C acta_runner test-e2e    # optional: dead-runner end-to-end suite
                                # (real runner processes; ~10-15 s)
make clean
```

(the GUI still needs its own `qmake6` + `make` step in `acta_gui/`)

## Minimal end-to-end example

Against a running llama.cpp `llama-server` in router mode (e.g. on `127.0.0.1:8080`):

```sh
# 1. Register the model
acta_cli model create --json '{"name":"llama-local","backend":"openai","base_url":"http://127.0.0.1:8080","model_identifier":"qwen3-8b"}'

# 2. Create a versioned skill (prompt template + optional output schema)
acta_cli skill create --json '{"name":"sentiment","prompt_template":"Classify the sentiment of the input. Reply with JSON: {\"label\": \"positive\"|\"negative\", \"confidence\": number}"}'

# 3. Create an immutable context (the input snapshot)
acta_cli context create --json '{"type":"text","content":"The build system shipped on time and the release went smoothly."}'

# 4. Create an execution binding context + skill revision + model revision
# the "prompt" field is optional; omit it to send just the context
acta_cli exec create --json '{"prompt":"What is the sentiment of the context?","context_id":1,"skill_revision_id":1,"model_revision_id":1}'

# 5. Run it (hard per-call HTTP timeout: --timeout, default 300 s)
acta_runner run 1

# 6. Inspect the result and the audit trail
acta_cli exec get 1
acta_cli log list
```

## Current status

Early prototype / POC.

**Done:** entity model and persistence (C library + CLI + GUI), skill/model versioning and folder organization, execution lifecycle and execution log, replayable immutable contexts, the LLM call path as a standalone runner (`acta_runner`: claim → resolve → preflight → OpenAI-compatible chat call → raw response capture → optional output-schema validation → complete/fail, with `execution_log` phase rows — see `docs/runner_analysis.md`), the in-app "Run" button (the GUI runs the runner's pipeline in-process on a worker thread — it directly uses the same runner source files (`acta_runner/src/run.c`, `acta_runner/src/backend.c`), one shared pipeline codebase with no duplicated pipeline logic, on its own DB connection — with live status polling of the shared database; while a run is in flight the button toggles into Cancel, which cooperatively cancels the run and transitions the row to `cancelled`), `sweep` (`acta_runner sweep --stale-seconds N`) cleans up executions left in `running` after a dead runner process: an execution is stale when its last runner activity — the latest of its newest `execution_log.created_at` and `started_at` (falling back to `created_at`) — is older than `now − N` seconds; a stale row transitions `running → failed` with the error `stale running: no runner activity for N s` and an `execution_failed` log row, and any row that leaves `running` between the query and the fail is skipped rather than overwriting a live outcome. `--stale-seconds` is required (there is no default) and must be a positive integer (0 is rejected). Normal timeout handling is done by the runner itself (hard per-call HTTP timeout, `--timeout`, default 300 s), and rerun of failed executions (`failed → pending` via `acta_db_execution_reset`).

**Not yet implemented:** streaming responses and automatic retries (a failed execution can be retried manually via the `failed → pending` reset). Automatic retries are deliberately deferred: transient backend failures are rare in the current single-node deployment, and a manual reset is simpler to reason about and avoids retry storms.

## Philosophy

ACTA Gamma is deliberately not an agent framework: the engine decides what happens, and the LLM only does the work it's asked to do.

## License

BSD Zero Clause License (BSD-0-Clause).
