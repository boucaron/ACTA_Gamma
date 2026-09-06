# ACTA Gamma

![Logo](assets/logo.jpg)

**LLMs as actions, not agents.**

A small, stateless LLM execution engine for versioned skills, reproducible analysis, and model benchmarking.

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

## Core ideas

* **Stateless** — every execution is independent and one-shot.
* **Versioned skills** — prompts and output schemas are revisioned.
* **Immutable contexts** — the exact input can be retained for replay.
* **Model independent** — use llama.cpp, cloud models, or other OpenAI-compatible backends.
* **Auditable** — executions retain prompts, raw responses, results, errors, and execution events.
* **Replayable** — run the same context and skill against another model or revision.
* **Benchmarkable** — compare models and skill revisions against the same datasets.
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

## Implementation

The implementation is C/C++ on top of SQLite:

| Component | Language | Description |
|---|---|---|
| `acta_db/` | C11 | SQLite persistence library (`libacta_db`) — skills, skill folders, skill revisions, models, model folders, model revisions, contexts, executions, execution logs |
| `acta_db_cli/` | C99 | Command-line client (`actagamma_db`) over `acta_db` (uses cJSON for output) |
| `acta_runner/` | C99 | Standalone LLM execution runner (`acta_runner`) — drives pending executions against the model's OpenAI-compatible backend: claim → resolve → preflight → chat call → record → complete/fail, with `execution_log` phase rows (uses curl + cJSON) |
| `acta_gamma/` | C++ / Qt 6 (Core, Widgets) | Desktop GUI: manage skills, models, contexts, review executions, and run them (the in-app "Run" button spawns `acta_runner`) |

Model backends are **OpenAI-compatible** endpoints (local llama.cpp server, cloud APIs, etc.). A model record stores `backend`, `base_url`, `model_identifier`, and a JSON configuration blob. The runner reads the keys `api_key`, `temperature`, `max_tokens`, `top_k`, and `supports_response_format`; unknown keys are warned about and ignored, and a malformed blob is warned about and treated as empty.

### Building

**Dependencies:** C compiler (MinGW or gcc/clang), SQLite 3, Qt 6 (Core, Widgets), cJSON (CLI + runner), curl (runner).

```sh
# 1. Database library (also builds and runs its test suite)
cd acta_db
make            # → libacta_db.a / libacta_db.so
make test       # C unit tests

# 2. CLI tool
cd ../acta_db_cli
make            # → ./actagamma_db
make test       # per-entity CLI tests

# 3. Runner (drives pending executions; needs a running
#    OpenAI-compatible backend, e.g. llama-server)
cd ../acta_runner
make            # → ./acta_runner
make test       # pipeline tests against a local stub backend

# 4. GUI
cd ../acta_gamma
qmake6 "CONFIG+=debug" ACTA_Gamma.pro -o Makefile
make
```

MinGW (MSYS2) setup for development:

```sh
pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-make
pacman -S mingw-w64-x86_64-qt6
pacman -S mingw-w64-x86_64-curl
```

Or, from the repository root, build the three C targets in dependency order with the top-level wrapper:

```sh
make all     # acta_db -> acta_db_cli -> acta_runner
make test    # all three C test suites
make clean
```

(the GUI still needs its own `qmake6` + `make` step in `acta_gamma/`)

## Minimal end-to-end example

Against a running OpenAI-compatible server (e.g. `llama-server` on `127.0.0.1:8080`):

```sh
# 1. Register the model
actagamma_db model create --json '{"name":"llama-local","backend":"openai","base_url":"http://127.0.0.1:8080","model_identifier":"qwen3-8b"}'

# 2. Create a versioned skill (prompt template + optional output schema)
actagamma_db skill create --json '{"name":"sentiment","prompt_template":"Classify the sentiment of the input. Reply with JSON: {\"label\": \"positive\"|\"negative\", \"confidence\": number}"}'

# 3. Create an immutable context (the input snapshot)
actagamma_db context create --json '{"type":"text","content":"The build system shipped on time and the release went smoothly."}'

# 4. Create an execution binding context + skill revision + model revision
actagamma_db exec create --json '{"prompt":"What is the sentiment of the context?","context_id":1,"skill_revision_id":1,"model_revision_id":1}'

# 5. Run it (hard per-call HTTP timeout: --timeout, default 300 s)
acta_runner run 1

# 6. Inspect the result and the audit trail
actagamma_db exec get 1
actagamma_db log list
```

## Current status

Early prototype / POC.

**Done:** entity model and persistence (C library + CLI + GUI), skill/model versioning and folder organization, execution lifecycle and execution log, replayable immutable contexts, the LLM call path as a standalone runner (`acta_runner`: claim → resolve → preflight → OpenAI-compatible chat call → raw response capture → optional output-schema validation → complete/fail, with `execution_log` phase rows — see `docs/runner_analysis.md`), the in-app "Run" button (the GUI spawns `acta_runner run <id>` via `QProcess` with live status polling), `sweep` (`acta_runner sweep --stale-seconds N`) as a last-resort safety net for executions left in `running` after a dead runner process — normal timeout handling is done by the runner itself (hard per-call HTTP timeout, `--timeout`, default 300 s), and rerun of failed executions (`failed → pending` via `acta_db_execution_reset`).

**Not yet implemented:** streaming responses and automatic retries (a failed execution can be retried manually via the `failed → pending` reset).

## Philosophy

> **The engine decides what happens. The LLM only does the work it's asked to do.**

ACTA Gamma is deliberately not an agent framework. It provides controlled, observable LLM actions that can be composed and evaluated by software outside the model.

## License

BSD Zero Clause License (BSD-0-Clause).
